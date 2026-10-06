#include "CSSFacialLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/MorphTarget.h"
#include "AssetCompilingManager.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"
#include "SkeletalMeshAttributes.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"

namespace
{
bool ReadFacialPointMap(const TSharedPtr<FJsonObject>& Root, const FMeshDescription& Description,
    TArray<int32>& PointMap, FString& Error)
{
    double Schema=0;
    const TArray<TSharedPtr<FJsonValue>> *Points=nullptr, *Indices=nullptr;
    bool DiagnosticOnly=false;
    if (Root->HasField(TEXT("diagnostic_only")) &&
        (!Root->HasTypedField<EJson::Boolean>(TEXT("diagnostic_only")) ||
         !Root->TryGetBoolField(TEXT("diagnostic_only"),DiagnosticOnly))) return false;
    if (DiagnosticOnly || !Root->HasTypedField<EJson::Number>(TEXT("schema")) ||
        !Root->TryGetNumberField(TEXT("schema"),Schema) || (Schema!=1 && Schema!=2) ||
        !Root->TryGetArrayField(TEXT("body_points"),Points) || Points->IsEmpty() || Points->Num()>100000)
        return false;
    if (Schema==2 && (!Root->TryGetArrayField(TEXT("point_indices"),Indices) || Indices->Num()!=Points->Num()))
        return false;
    if (Schema==1 && Root->HasField(TEXT("point_indices"))) return false;
    const auto Positions=FSkeletalMeshConstAttributes(Description).GetVertexPositions();
    TSet<int32> Seen;
    PointMap.Reset(Points->Num());
    for (int32 I=0;I<Points->Num();++I) {
        double Index=I;
        if (Indices && (!(*Indices)[I]->TryGetNumber(Index) || !FMath::IsFinite(Index) ||
            Index<0 || Index>2000000 || Index!=FMath::FloorToDouble(Index))) return false;
        const int32 Vertex=static_cast<int32>(Index);
        if (Seen.Contains(Vertex) || !Description.Vertices().IsValid(FVertexID(Vertex))) return false;
        const TArray<TSharedPtr<FJsonValue>>* Coordinates=nullptr;
        if (!(*Points)[I]->TryGetArray(Coordinates) || Coordinates->Num()!=3) return false;
        double V[3];
        for (int32 J=0;J<3;++J)
            if (!(*Coordinates)[J]->TryGetNumber(V[J]) || !FMath::IsFinite(V[J])) return false;
        if (FVector3f::Distance(Positions[FVertexID(Vertex)],FVector3f(V[0],V[1],V[2]))>.0002f) {
            Error=FString::Printf(TEXT("point %d (source %d): actual %s, expected %s"),I,Vertex,
                *Positions[FVertexID(Vertex)].ToString(),*FVector3f(V[0],V[1],V[2]).ToString());
            return false;
        }
        PointMap.Add(Vertex);
        Seen.Add(Vertex);
    }
    return true;
}
}

FString UCSSFacialLibrary::ExpressionVertexMap(USkeletalMesh* Candidate)
{
    if (!IsRunningCommandlet() || !Candidate ||
        !Candidate->GetPathName().StartsWith(TEXT("/Game/CSS/Locomotion/Faces/SK_Face_")))
        return TEXT("ERROR: expected shared facial candidate in a commandlet");
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FSkeletalMeshModel* Model=Candidate->GetImportedModel();
    const auto* Info=Candidate->GetLODInfo(0);
    if (!Model || Model->LODModels.Num()!=1 || !Info || !Candidate->GetMeshDescription(0))
        return TEXT("ERROR: missing source LOD or build settings");
    const auto& LOD=Model->LODModels[0];
    if (LOD.MeshToImportVertexMap.Num()!=LOD.NumVertices || LOD.NumVertices>2000000)
        return TEXT("ERROR: incomplete or oversized source vertex map");
    TArray<TSharedPtr<FJsonValue>> Map;
    Map.Reserve(LOD.MeshToImportVertexMap.Num());
    for (const int32 Index:LOD.MeshToImportVertexMap) {
        if (!Candidate->GetMeshDescription(0)->Vertices().IsValid(FVertexID(Index)))
            return TEXT("ERROR: invalid source vertex index");
        Map.Add(MakeShared<FJsonValueNumber>(Index));
    }
    auto Root=MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("mesh"),Candidate->GetPathName());
    Root->SetNumberField(TEXT("threshold_cm"),Info->BuildSettings.MorphThresholdPosition);
    Root->SetArrayField(TEXT("source_vertices"),MoveTemp(Map));
    FString Text;
    FJsonSerializer::Serialize(Root,TJsonWriterFactory<>::Create(&Text));
    return Text;
}

FString UCSSFacialLibrary::AddExpressionShapes(USkeletalMesh* Mesh, const FString& InputFile)
{
    if (!IsRunningCommandlet() || !Mesh ||
        !Mesh->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/SK_Face_")))
        return TEXT("ERROR: requires an isolated commandlet mesh");
    if (Mesh->GetLODNum() != 1 || !Mesh->GetMeshDescription(0))
        return TEXT("ERROR: expected one source-description LOD");
    FString Text;
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Text, *InputFile) || Text.Len() > 64*1024*1024 ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
        return TEXT("ERROR: invalid facial interchange");
    const TArray<TSharedPtr<FJsonValue>>* Shapes=nullptr;
    TArray<int32> PointMap;
    FString PointError;
    if (!ReadFacialPointMap(Root,*Mesh->GetMeshDescription(0),PointMap,PointError) ||
        !Root->TryGetArrayField(TEXT("morph_targets"), Shapes) || Shapes->IsEmpty() || Shapes->Num()>128)
        return TEXT("ERROR: invalid facial schema, counts or point correspondence: ")+PointError;
    // Build a detached description first. Validation cannot partly alter the candidate.
    FMeshDescription Description=*Mesh->GetMeshDescription(0);
    FSkeletalMeshAttributes Attributes(Description);
    auto Numbers=[](const TSharedPtr<FJsonValue>& Value, int32 Count, TArray<double>& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
        if (!Value->TryGetArray(Values) || Values->Num()!=Count) return false;
        Out.Reset();
        for (const auto& Entry:*Values) {
            double Number=0;
            if (!Entry->TryGetNumber(Number) || !FMath::IsFinite(Number)) return false;
            Out.Add(Number);
        }
        return true;
    };
    TArray<double> Values;
    const int32 PreviousCount=Mesh->GetMorphTargets().Num();
    TSet<FName> Names;
    for (const auto& Value:*Shapes) {
        const TSharedPtr<FJsonObject>* Shape=nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Deltas=nullptr;
        FString Name;
        if (!Value->TryGetObject(Shape) || !(*Shape)->TryGetStringField(TEXT("name"),Name) ||
            Name.IsEmpty() || Name.Len()>64 || Names.Contains(FName(*Name)) || Mesh->FindMorphTarget(FName(*Name)) ||
            !(*Shape)->TryGetArrayField(TEXT("deltas"),Deltas) || Deltas->IsEmpty() || Deltas->Num()>PointMap.Num())
            return TEXT("ERROR: invalid or existing expression shape");
        for (TCHAR Character:Name)
            if (!FChar::IsAlnum(Character) && Character!=TEXT('_')) return TEXT("ERROR: invalid shape name");
        const FName MorphName(*Name);
        Names.Add(MorphName);
        if (!Attributes.RegisterMorphTargetAttribute(MorphName,false)) return TEXT("ERROR: cannot register morph attribute");
        auto MorphPositions=Attributes.GetVertexMorphPositionDelta(MorphName);
        TSet<int32> Seen;
        for (const auto& Delta:*Deltas) {
            if (!Numbers(Delta,4,Values) || Values[0]<0 || Values[0]>=PointMap.Num() ||
                Values[0]!=FMath::FloorToDouble(Values[0])) return TEXT("ERROR: invalid morph index");
            const int32 Index=PointMap[static_cast<int32>(Values[0])];
            const FVector3f Position(Values[1],Values[2],Values[3]);
            if (Seen.Contains(Index) || Position.Size()>20) return TEXT("ERROR: duplicate or oversized face delta");
            Seen.Add(Index);
            MorphPositions[FVertexID(Index)]=Position;
        }
    }
    Mesh->CreateMeshDescription(0,MoveTemp(Description));
    Mesh->CommitMeshDescription(0);
    Mesh->Build();
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (Mesh->GetMorphTargets().Num()!=PreviousCount+Names.Num())
        return TEXT("ERROR: built morph count differs");
    for (FName Name:Names) {
        UMorphTarget* Morph=Mesh->FindMorphTarget(Name);
        if (!Morph || !Morph->HasDataForLOD(0)) return TEXT("ERROR: empty built facial shape");
    }
    return FString::Printf(TEXT("PASS: retained %d morphs, added %d expression shapes"),PreviousCount,Names.Num());
}

FString UCSSFacialLibrary::VerifyExpressionShapes(USkeletalMesh* Source, USkeletalMesh* Candidate,
    const FString& InputFile)
{
    if (!IsRunningCommandlet() || !Source || !Candidate || Source==Candidate ||
        (!Candidate->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/SK_Face_")) &&
         !Candidate->GetPathName().StartsWith(TEXT("/Game/CSS/Locomotion/Faces/SK_Face_"))))
        return TEXT("ERROR: invalid comparison assets");
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FMeshDescription *Before=Source->GetMeshDescription(0), *After=Candidate->GetMeshDescription(0);
    if (!Before || !After || Before->Vertices().Num()!=After->Vertices().Num() ||
        Before->Triangles().Num()!=After->Triangles().Num()) return TEXT("ERROR: topology count changed");
    FSkeletalMeshConstAttributes Old(*Before), Now(*After);
    const auto OldPositions=Old.GetVertexPositions(), NewPositions=Now.GetVertexPositions();
    for (FVertexID Vertex:Before->Vertices().GetElementIDs())
        if (!After->Vertices().IsValid(Vertex) || OldPositions[Vertex]!=NewPositions[Vertex])
            return TEXT("ERROR: base vertex positions changed");
    for (UMorphTarget* Morph:Source->GetMorphTargets()) {
        const auto A=Old.GetVertexMorphPositionDelta(Morph->GetFName());
        const auto B=Now.GetVertexMorphPositionDelta(Morph->GetFName());
        if (!A.IsValid() || !B.IsValid()) return TEXT("ERROR: original morph attribute missing");
        for (FVertexID Vertex:Before->Vertices().GetElementIDs())
            if (A[Vertex]!=B[Vertex]) return TEXT("ERROR: original morph deltas changed");
    }
    FString Text;
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Text,*InputFile) || Text.Len()>64*1024*1024 ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root)
        return TEXT("ERROR: invalid comparison interchange");
    const TArray<TSharedPtr<FJsonValue>>* Shapes=nullptr;
    TArray<int32> PointMap;
    FString PointError;
    if (!ReadFacialPointMap(Root,*After,PointMap,PointError) ||
        !Root->TryGetArrayField(TEXT("morph_targets"),Shapes) || Shapes->IsEmpty() || Shapes->Num()>128)
        return TEXT("ERROR: invalid comparison shapes");
    const FSkeletalMeshModel* Model=Candidate->GetImportedModel();
    if (!Model || Model->LODModels.Num()!=1) return TEXT("ERROR: unexpected built LODs");
    const auto& LOD=Model->LODModels[0];
    double MaxError=0;
    double MaxOmittedDelta=0;
    const auto* LODInfo=Candidate->GetLODInfo(0);
    if (!LODInfo || !FMath::IsFinite(LODInfo->BuildSettings.MorphThresholdPosition) ||
        LODInfo->BuildSettings.MorphThresholdPosition<0 || LODInfo->BuildSettings.MorphThresholdPosition>.015001f)
        return TEXT("ERROR: unexpected morph filtering threshold");
    int32 Compared=0;
    for (const auto& Value:*Shapes) {
        const TSharedPtr<FJsonObject>* Shape=nullptr;
        FString Name;
        const TArray<TSharedPtr<FJsonValue>>* Deltas=nullptr;
        if (!Value->TryGetObject(Shape) || !(*Shape)->TryGetStringField(TEXT("name"),Name) ||
            !(*Shape)->TryGetArrayField(TEXT("deltas"),Deltas)) return TEXT("ERROR: invalid comparison shape");
        const auto Attribute=Now.GetVertexMorphPositionDelta(FName(*Name));
        if (!Attribute.IsValid()) return TEXT("ERROR: facial attribute missing");
        TMap<int32,FVector3f> Expected;
        for (const auto& Delta:*Deltas) {
            const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
            if (!Delta->TryGetArray(Values) || Values->Num()!=4) return TEXT("ERROR: invalid comparison delta");
            double V[4];
            for (int32 I=0;I<4;++I)
                if (!(*Values)[I]->TryGetNumber(V[I]) || !FMath::IsFinite(V[I])) return TEXT("ERROR: invalid comparison number");
            if (V[0]<0 || V[0]>=PointMap.Num() || V[0]!=FMath::FloorToDouble(V[0])) return TEXT("ERROR: invalid comparison vertex");
            const int32 Index=PointMap[static_cast<int32>(V[0])];
            if (!After->Vertices().IsValid(FVertexID(Index)) || Expected.Contains(Index)) return TEXT("ERROR: invalid comparison point");
            Expected.Add(Index,FVector3f(V[1],V[2],V[3]));
        }
        for (FVertexID Vertex:After->Vertices().GetElementIDs()) {
            const FVector3f Wanted=Expected.FindRef(Vertex.GetValue());
            MaxError=FMath::Max(MaxError,static_cast<double>(FVector3f::Distance(Attribute[Vertex],Wanted)));
        }
        UMorphTarget* Morph=Candidate->FindMorphTarget(FName(*Name));
        if (!Morph) return TEXT("ERROR: built morph missing");
        int32 Count=0;
        const FMorphTargetDelta* Built=Morph->GetMorphTargetDelta(0,Count);
        if (!Built || Count==0) return TEXT("ERROR: built morph empty");
        TMap<int32,FVector3f> BuiltPositions;
        for (int32 I=0;I<Count;++I) {
            if (!LOD.MeshToImportVertexMap.IsValidIndex(Built[I].SourceIdx)) return TEXT("ERROR: built morph index invalid");
            const int32 Point=LOD.MeshToImportVertexMap[Built[I].SourceIdx];
            MaxError=FMath::Max(MaxError,static_cast<double>(FVector3f::Distance(Built[I].PositionDelta,Expected.FindRef(Point))));
            BuiltPositions.Add(Built[I].SourceIdx,Built[I].PositionDelta);
            ++Compared;
        }
        for (int32 I=0;I<LOD.MeshToImportVertexMap.Num();++I)
            if (!BuiltPositions.Contains(I))
                MaxOmittedDelta=FMath::Max(MaxOmittedDelta,static_cast<double>(Expected.FindRef(LOD.MeshToImportVertexMap[I]).Size()));
    }
    if (MaxError>.0002) return FString::Printf(TEXT("ERROR: facial delta error %.9f cm"),MaxError);
    // UE filters small deltas using the source mesh's build setting. Check omitted
    // vertices too, allowing only that threshold plus float reconstruction error.
    if (MaxOmittedDelta>LODInfo->BuildSettings.MorphThresholdPosition+.0002)
        return FString::Printf(TEXT("ERROR: omitted facial delta %.9f cm exceeds build threshold"),MaxOmittedDelta);
    return FString::Printf(TEXT("PASS: %d shapes, %d built deltas, retained error %.9f cm, filtered maximum %.9f cm (threshold %.9f)"),
        Shapes->Num(),Compared,MaxError,MaxOmittedDelta,LODInfo->BuildSettings.MorphThresholdPosition);
}

FString UCSSFacialLibrary::VerifyExpressionPreservation(USkeletalMesh* Source, USkeletalMesh* Candidate)
{
    if (!IsRunningCommandlet() || !Source || !Candidate || Source==Candidate ||
        !Candidate->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/SK_Face_")))
        return TEXT("ERROR: expected isolated facial comparison");
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (Source->GetLODNum()!=1 || Candidate->GetLODNum()!=1 ||
        !Source->GetMeshDescription(0) || !Candidate->GetMeshDescription(0))
        return TEXT("ERROR: expected one source-description LOD");

    // Remove only the new morph attributes from copies. Every other serialized
    // attribute, including skin weights, UVs, normals and topology, must match.
    FMeshDescription Before=*Source->GetMeshDescription(0), After=*Candidate->GetMeshDescription(0);
    FSkeletalMeshAttributes Original(Before), Derived(After);
    const auto OriginalNames=Original.GetMorphTargetNames();
    int32 Added=0;
    for (const FName Name:Derived.GetMorphTargetNames()) {
        if (!OriginalNames.Contains(Name)) {
            if (!Derived.UnregisterMorphTargetAttribute(Name)) return TEXT("ERROR: could not isolate base attributes");
            ++Added;
        }
    }
    TArray<uint8> A, B;
    FMemoryWriter WriterA(A,true), WriterB(B,true);
    Before.Serialize(WriterA);
    After.Serialize(WriterB);
    if (WriterA.IsError() || WriterB.IsError() || A!=B)
        return FString::Printf(TEXT("ERROR: non-expression mesh description differs (%d / %d bytes)"),A.Num(),B.Num());
    const int32 DescriptionBytes=A.Num();
    A.Empty(); B.Empty();

    const FSkeletalMeshModel *Old=Source->GetImportedModel(), *Now=Candidate->GetImportedModel();
    if (!Old || !Now || Old->LODModels.Num()!=1 || Now->LODModels.Num()!=1)
        return TEXT("ERROR: missing built LOD");
    const auto &OldLOD=Old->LODModels[0], &NewLOD=Now->LODModels[0];
    if (OldLOD.Sections.Num()!=NewLOD.Sections.Num() || OldLOD.IndexBuffer!=NewLOD.IndexBuffer ||
        OldLOD.MeshToImportVertexMap!=NewLOD.MeshToImportVertexMap ||
        OldLOD.ActiveBoneIndices!=NewLOD.ActiveBoneIndices || OldLOD.RequiredBones!=NewLOD.RequiredBones)
        return TEXT("ERROR: built topology or bone mapping changed");
    int64 SectionBytes=0;
    auto WriteSection=[](FArchive& Ar,FSkelMeshSection& Section) {
        Ar << Section.MaterialIndex << Section.BaseIndex << Section.NumTriangles << Section.BaseVertexIndex;
        Ar << Section.bRecomputeTangent << Section.RecomputeTangentsVertexMaskChannel;
        Ar << Section.bCastShadow << Section.bVisibleInRayTracing << Section.bDisabled;
        Ar << Section.BoneMap << Section.NumVertices << Section.MaxBoneInfluences << Section.bUse16BitBoneIndex;
        Ar << Section.CorrespondClothAssetIndex << Section.ClothingData << Section.ClothMappingDataLODs;
        Ar << Section.GenerateUpToLodIndex << Section.OriginalDataSectionIndex << Section.ChunkedParentSectionIndex;
        Ar << Section.OverlappingVertices;
        int32 Count=Section.SoftVertices.Num();
        Ar << Count;
        for (auto& Vertex:Section.SoftVertices) {
            Ar << Vertex.Position << Vertex.TangentX << Vertex.TangentY << Vertex.TangentZ << Vertex.Color;
            for (auto& UV:Vertex.UVs) Ar << UV;
            for (auto& Bone:Vertex.InfluenceBones) Ar << Bone;
            for (auto& Weight:Vertex.InfluenceWeights) Ar << Weight;
        }
    };
    for (int32 I=0;I<OldLOD.Sections.Num();++I) {
        auto OldSection=OldLOD.Sections[I], NewSection=NewLOD.Sections[I];
        TArray<uint8> OldBytes, NewBytes;
        FMemoryWriter OldWriter(OldBytes,true), NewWriter(NewBytes,true);
        WriteSection(OldWriter,OldSection);
        WriteSection(NewWriter,NewSection);
        if (OldWriter.IsError() || NewWriter.IsError() || OldBytes!=NewBytes)
            return FString::Printf(TEXT("ERROR: built section %d differs"),I);
        SectionBytes+=OldBytes.Num();
    }
    const FProperty* Cloth=FindFProperty<FProperty>(USkeletalMesh::StaticClass(),USkeletalMesh::GetMeshClothingAssetsMemberName());
    if (!Cloth || !Cloth->Identical_InContainer(Source,Candidate,0,PPF_DeepComparison))
        return TEXT("ERROR: persistent clothing data differs");
    return FString::Printf(TEXT("PASS: %d description bytes, %d sections (%lld bytes), %d cloth assets; %d added morphs excluded"),
        DescriptionBytes,OldLOD.Sections.Num(),SectionBytes,Source->GetMeshClothingAssets().Num(),Added);
}
