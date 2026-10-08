#include "CSSRigAuditLibrary.h"

#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Serialization/JsonSerializer.h"
#include "SkinnedAssetCompiler.h"

FString UCSSRigAuditLibrary::SnapshotMesh(USkeletalMesh* Mesh)
{
    if (!IsRunningCommandlet() || !Mesh) return TEXT("ERROR: expected a commandlet mesh");
    USkinnedAsset* Assets[]{Mesh};
    FSkinnedAssetCompilingManager::Get().FinishCompilation(Assets);
    const auto* Render = Mesh->GetResourceForRendering();
    if (!Render || Render->LODRenderData.IsEmpty()) return TEXT("ERROR: no render LODs");
    const auto& Reference = Mesh->GetRefSkeleton();
    auto Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("raw_bones"), Reference.GetRawBoneNum());
    Root->SetNumberField(TEXT("final_bones"), Reference.GetNum());
    TArray<TSharedPtr<FJsonValue>> Bones;
    for (int32 Index = 0; Index < Reference.GetNum(); ++Index)
    {
        auto Bone = MakeShared<FJsonObject>();
        Bone->SetStringField(TEXT("name"), Reference.GetBoneName(Index).ToString());
        Bone->SetNumberField(TEXT("parent"), Reference.GetParentIndex(Index));
        Bone->SetBoolField(TEXT("finite_pose"), !Reference.GetRefBonePose()[Index].ContainsNaN());
        const auto& Pose = Reference.GetRefBonePose()[Index];
        const auto T = Pose.GetTranslation();
        const auto Q = Pose.GetRotation();
        const auto S = Pose.GetScale3D();
        TArray<TSharedPtr<FJsonValue>> Transform;
        for (double Value : {T.X, T.Y, T.Z, Q.X, Q.Y, Q.Z, Q.W, S.X, S.Y, S.Z})
        {
            if (!FMath::IsFinite(Value)) return TEXT("ERROR: nonfinite reference pose");
            Transform.Add(MakeShared<FJsonValueNumber>(Value));
        }
        Bone->SetArrayField(TEXT("local_transform"), MoveTemp(Transform));
        Bones.Add(MakeShared<FJsonValueObject>(Bone));
    }
    Root->SetArrayField(TEXT("bones"), MoveTemp(Bones));
    TArray<TSharedPtr<FJsonValue>> Lods;
    for (const auto& Lod : Render->LODRenderData)
    {
        const auto& Weights = Lod.SkinWeightVertexBuffer;
        if (!Weights.GetDataVertexBuffer()->GetWeightData())
            return TEXT("ERROR: CPU skin weights unavailable");
        if (Weights.GetNumVertices() != Lod.GetNumVertices())
            return TEXT("ERROR: vertex/weight counts differ");
        auto Row = MakeShared<FJsonObject>();
        Row->SetNumberField(TEXT("vertices"), Lod.GetNumVertices());
        Row->SetNumberField(TEXT("max_influences"), Weights.GetMaxBoneInfluences());
        Row->SetBoolField(TEXT("weight_16bit"), Weights.Use16BitBoneWeight());
        TArray<TSharedPtr<FJsonValue>> Sections;
        uint64 Visited = 0;
        uint32 ExpectedBase = 0;
        for (const auto& Section : Lod.RenderSections)
        {
            if (Section.BaseVertexIndex != ExpectedBase ||
                Section.BaseVertexIndex > Weights.GetNumVertices() ||
                Section.NumVertices > Weights.GetNumVertices() - Section.BaseVertexIndex)
                return TEXT("ERROR: inconsistent section vertex ranges");
            ExpectedBase += Section.NumVertices;
            auto Part = MakeShared<FJsonObject>();
            Part->SetNumberField(TEXT("material_index"), Section.MaterialIndex);
            Part->SetNumberField(TEXT("vertices"), Section.NumVertices);
            Part->SetNumberField(TEXT("triangles"), Section.NumTriangles);
            Part->SetBoolField(TEXT("disabled"), Section.bDisabled);
            Part->SetNumberField(TEXT("cloth_asset_index"), Section.CorrespondClothAssetIndex);
            Part->SetNumberField(TEXT("cloth_mapping_lods"), Section.ClothMappingDataLODs.Num());
            TArray<TSharedPtr<FJsonValue>> BoneMap;
            uint32 InvalidBones = 0, Unweighted = 0, NonFinite = 0;
            uint32 MinSum = MAX_uint32, MaxSum = 0, UsedMax = 0;
            for (const auto Bone : Section.BoneMap)
            {
                if (Bone >= Reference.GetNum()) return TEXT("ERROR: invalid section bone map");
                BoneMap.Add(MakeShared<FJsonValueNumber>(Bone));
            }
            for (uint32 Vertex = Section.BaseVertexIndex; Vertex < ExpectedBase; ++Vertex)
            {
                if (Lod.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(Vertex).ContainsNaN()) ++NonFinite;
                uint32 Offset = 0, Count = 0, Sum = 0, Used = 0;
                Weights.GetVertexInfluenceOffsetCount(Vertex, Offset, Count);
                if (Count > Weights.GetMaxBoneInfluences()) return TEXT("ERROR: invalid influence count");
                for (uint32 Influence = 0; Influence < Count; ++Influence)
                {
                    const auto Weight = Weights.GetBoneWeight(Vertex, Influence);
                    if (!Weight) continue;
                    const auto Bone = Weights.GetBoneIndex(Vertex, Influence);
                    if (Bone >= static_cast<uint32>(Section.BoneMap.Num())) ++InvalidBones;
                    Sum += Weight;
                    ++Used;
                }
                if (!Sum) ++Unweighted;
                MinSum = FMath::Min(MinSum, Sum);
                MaxSum = FMath::Max(MaxSum, Sum);
                UsedMax = FMath::Max(UsedMax, Used);
                ++Visited;
            }
            Part->SetArrayField(TEXT("bone_map"), MoveTemp(BoneMap));
            Part->SetNumberField(TEXT("invalid_weight_bones"), InvalidBones);
            Part->SetNumberField(TEXT("unweighted_vertices"), Unweighted);
            Part->SetNumberField(TEXT("nonfinite_positions"), NonFinite);
            Part->SetNumberField(TEXT("min_weight_sum_u16"), Section.NumVertices ? MinSum : 0);
            Part->SetNumberField(TEXT("max_weight_sum_u16"), MaxSum);
            Part->SetNumberField(TEXT("max_used_influences"), UsedMax);
            Sections.Add(MakeShared<FJsonValueObject>(Part));
        }
        if (Visited != Weights.GetNumVertices()) return TEXT("ERROR: sections do not cover all vertices");
        Row->SetArrayField(TEXT("sections"), MoveTemp(Sections));
        Lods.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("render_lods"), MoveTemp(Lods));
    TArray<TSharedPtr<FJsonValue>> Morphs;
    for (const UMorphTarget* Morph : Mesh->GetMorphTargets())
    {
        if (!Morph) return TEXT("ERROR: null morph target");
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), Morph->GetName());
        TArray<TSharedPtr<FJsonValue>> Models;
        int32 LodIndex = 0;
        for (const auto& Model : Morph->GetMorphLODModels())
        {
            if (!Render->LODRenderData.IsValidIndex(LodIndex)) return TEXT("ERROR: morph references missing LOD");
            uint32 InvalidIndices = 0, NonFinite = 0;
            for (const auto& Delta : Model.Vertices)
            {
                if (Delta.SourceIdx >= Render->LODRenderData[LodIndex].GetNumVertices()) ++InvalidIndices;
                if (Delta.PositionDelta.ContainsNaN() || Delta.TangentZDelta.ContainsNaN()) ++NonFinite;
            }
            auto Detail = MakeShared<FJsonObject>();
            Detail->SetNumberField(TEXT("deltas"), Model.Vertices.Num());
            Detail->SetNumberField(TEXT("invalid_indices"), InvalidIndices);
            Detail->SetNumberField(TEXT("nonfinite_deltas"), NonFinite);
            Models.Add(MakeShared<FJsonValueObject>(Detail));
            ++LodIndex;
        }
        Row->SetArrayField(TEXT("lods"), MoveTemp(Models));
        Morphs.Add(MakeShared<FJsonValueObject>(Row));
    }
    Root->SetArrayField(TEXT("morphs"), MoveTemp(Morphs));
    FString Result;
    if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Result)))
        return TEXT("ERROR: could not serialize mesh snapshot");
    return Result;
}
