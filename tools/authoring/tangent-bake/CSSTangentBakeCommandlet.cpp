#include "CSSTangentBakeCommandlet.h"
#include "Engine/SkeletalMesh.h"
#include "AssetCompilingManager.h"
#include "Modules/ModuleManager.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SkeletalMeshAttributes.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "UObject/SavePackage.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, CSSAuthoring)
DEFINE_LOG_CATEGORY_STATIC(LogCSSTangentBake, Log, All);

namespace
{
int32 Fail(const FString& Message)
{
    UE_LOG(LogCSSTangentBake, Error, TEXT("%s"), *Message);
    return 1;
}

FString Key(int32 Material, const int32* Points)
{
    FString Result;
    for (int32 Rotation = 0; Rotation < 3; ++Rotation)
    {
        const FString Candidate = FString::Printf(TEXT("%08d:%08d:%08d:%08d"), Material,
            Points[Rotation], Points[(Rotation+1)%3], Points[(Rotation+2)%3]);
        if (Rotation == 0 || Candidate < Result) Result = Candidate;
    }
    return Result;
}

TSharedPtr<FJsonObject> Inspect(USkeletalMesh* Mesh)
{
    auto Result = MakeShared<FJsonObject>();
    const FSkeletalMeshLODModel& LOD = Mesh->GetImportedModel()->LODModels[0];
    int32 Zero = 0, Nonunit = 0;
    for (const auto& Section : LOD.Sections)
        for (const auto& Vertex : Section.SoftVertices)
        {
            Zero += Vertex.TangentX.IsNearlyZero(1.e-6f);
            Nonunit += FMath::Abs(Vertex.TangentX.Size()-1.f) > .0001f;
        }
    Result->SetNumberField(TEXT("vertices"), LOD.NumVertices);
    Result->SetNumberField(TEXT("triangles"), LOD.IndexBuffer.Num()/3);
    Result->SetNumberField(TEXT("morphs"), Mesh->GetMorphTargets().Num());
    Result->SetNumberField(TEXT("zero_tangents"), Zero);
    Result->SetNumberField(TEXT("nonunit_tangents"), Nonunit);
    Result->SetBoolField(TEXT("recompute_tangents"), Mesh->GetLODInfo(0)->BuildSettings.bRecomputeTangents);
    return Result;
}
}

UCSSTangentBakeCommandlet::UCSSTangentBakeCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

int32 UCSSTangentBakeCommandlet::Main(const FString& Params)
{
    FString Input, Output, Target;
    if (!FParse::Value(*Params, TEXT("Source="), Input) ||
        !FParse::Value(*Params, TEXT("Output="), Output)) return Fail(TEXT("Source and Output required"));
    if (!Input.StartsWith(TEXT("/Game/CSSAuthoring/EveSurface20261014/")) &&
        !Input.StartsWith(TEXT("/Game/CSS/AnimLab/"))) return Fail(TEXT("Diagnostic source required"));
    USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, *Input);
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!Source || !Source->GetImportedModel() || Source->GetLODNum()!=1 ||
        !Source->GetMeshClothingAssets().IsEmpty()) return Fail(TEXT("Expected one cloth-free LOD"));
    auto Report = MakeShared<FJsonObject>();
    Report->SetObjectField(TEXT("before"), Inspect(Source));
    if (FParse::Value(*Params,TEXT("Target="),Target))
    {
        if (!Target.StartsWith(TEXT("/Game/CSSAuthoring/EveSurface20261014/")) ||
            !FPackageName::IsValidLongPackageName(Target) || FPackageName::DoesPackageExist(Target))
            return Fail(TEXT("New diagnostic target required"));
        UPackage* Package = CreatePackage(*Target);
        USkeletalMesh* Mesh = DuplicateObject<USkeletalMesh>(Source, Package,
            *FPackageName::GetLongPackageAssetName(Target));
        Mesh->SetFlags(RF_Public | RF_Standalone);
        FMeshDescription* Description = Mesh->GetMeshDescription(0);
        if (!Description) return Fail(TEXT("Missing source mesh description"));
        FSkeletalMeshAttributes Attributes(*Description);
        auto Positions = Attributes.GetVertexPositions();
        auto Normals = Attributes.GetVertexInstanceNormals();
        auto Tangents = Attributes.GetVertexInstanceTangents();
        auto Signs = Attributes.GetVertexInstanceBinormalSigns();
        auto UVs = Attributes.GetVertexInstanceUVs();
        auto Slots = Attributes.GetPolygonGroupMaterialSlotNames();
        const FSkeletalMeshLODModel& LOD = Source->GetImportedModel()->LODModels[0];
        TArray<FSoftSkinVertex> Vertices;
        LOD.GetVertices(Vertices);
        if (Vertices.Num()!=LOD.MeshToImportVertexMap.Num()) return Fail(TEXT("Missing render-to-source map"));
        TMap<FString,TArray<int32>> Triangles;
        for (const auto& Section : LOD.Sections)
            for (int32 T = 0; T < Section.NumTriangles; ++T)
            {
                const int32 Start = Section.BaseIndex+T*3;
                int32 Points[3];
                for (int32 C = 0; C < 3; ++C) Points[C] = LOD.MeshToImportVertexMap[LOD.IndexBuffer[Start+C]];
                Triangles.FindOrAdd(Key(Section.MaterialIndex,Points)).Add(Start);
            }
        TSet<FVertexInstanceID> Assigned;
        TArray<TSharedPtr<FJsonValue>> Repaired;
        int32 Matched = 0;
        for (FTriangleID Triangle : Description->Triangles().GetElementIDs())
        {
            const auto Instances = Description->GetTriangleVertexInstances(Triangle);
            const FName Slot = Slots[Description->GetTrianglePolygonGroup(Triangle)];
            const int32 Material = Mesh->GetMaterials().IndexOfByPredicate([&](const FSkeletalMaterial& M) {
                return M.ImportedMaterialSlotName==Slot;
            });
            if (Material==INDEX_NONE) return Fail(TEXT("Unknown source material slot"));
            int32 Points[3];
            for (int32 C = 0; C < 3; ++C) Points[C] = Description->GetVertexInstanceVertex(Instances[C]).GetValue();
            TArray<int32>* Candidates = Triangles.Find(Key(Material,Points));
            if (!Candidates) return Fail(TEXT("No render triangle matches source point IDs"));
            int32 Match = INDEX_NONE, Rotation = 0;
            for (int32 Index = 0; Index < Candidates->Num() && Match==INDEX_NONE; ++Index)
                for (int32 R = 0; R < 3; ++R)
                {
                    bool Equal = true;
                    for (int32 C = 0; C < 3 && Equal; ++C)
                    {
                        const int32 V = LOD.IndexBuffer[(*Candidates)[Index]+(R+C)%3];
                        Equal = LOD.MeshToImportVertexMap[V]==Points[C] &&
                            Vertices[V].Position.Equals(Positions[FVertexID(Points[C])],.000001f) &&
                            FVector3f(Vertices[V].TangentZ).Equals(Normals[Instances[C]],.00002f);
                        for (int32 U = 0; U < UVs.GetNumChannels() && Equal; ++U)
                            Equal = Vertices[V].UVs[U].Equals(UVs.Get(Instances[C],U),.0000001f);
                    }
                    if (Equal) { Match=Index; Rotation=R; break; }
                }
            if (Match==INDEX_NONE) return Fail(TEXT("Triangle surface attributes do not match"));
            const int32 Start = (*Candidates)[Match];
            Candidates->RemoveAtSwap(Match);
            for (int32 C = 0; C < 3; ++C)
            {
                const FVertexInstanceID Instance = Instances[C];
                if (Assigned.Contains(Instance)) return Fail(TEXT("Expected single-use source corners"));
                Assigned.Add(Instance);
                const int32 V = LOD.IndexBuffer[Start+(Rotation+C)%3];
                const FSoftSkinVertex& Vertex = Vertices[V];
                FVector3f Tangent = Vertex.TangentX;
                float Sign;
                if (Tangent.IsNearlyZero(1.e-6f))
                {
                    const FVector3d E1 = FVector3d(Positions[FVertexID(Points[1])])-FVector3d(Positions[FVertexID(Points[0])]);
                    const FVector3d E2 = FVector3d(Positions[FVertexID(Points[2])])-FVector3d(Positions[FVertexID(Points[0])]);
                    const FVector2d D1 = FVector2d(UVs.Get(Instances[1],0))-FVector2d(UVs.Get(Instances[0],0));
                    const FVector2d D2 = FVector2d(UVs.Get(Instances[2],0))-FVector2d(UVs.Get(Instances[0],0));
                    const double Determinant = D1.X*D2.Y-D1.Y*D2.X;
                    if (!FMath::IsFinite(Determinant) || Determinant==0.)
                        return Fail(TEXT("Cannot derive a tangent from collapsed UVs"));
                    const FVector3d Raw = (E1*D2.Y-E2*D1.Y)/Determinant;
                    const FVector3d N = FVector3d(Normals[Instance]).GetSafeNormal();
                    Tangent = FVector3f((Raw-N*FVector3d::DotProduct(N,Raw)).GetSafeNormal());
                    Sign = 0.f;
                    for (int32 Other = 0; Other < 3; ++Other)
                    {
                        const auto& Neighbor = Vertices[LOD.IndexBuffer[Start+(Rotation+Other)%3]];
                        if (Neighbor.TangentX.IsNearlyZero(1.e-6f)) continue;
                        const float D = FVector3f::DotProduct(FVector3f::CrossProduct(
                            FVector3f(Neighbor.TangentZ),Neighbor.TangentX),Neighbor.TangentY);
                        if (!FMath::IsFinite(D) || FMath::Abs(D)<.000001f)
                            return Fail(TEXT("Neighbor has ambiguous handedness"));
                        const float NeighborSign = D<0.f ? -1.f : 1.f;
                        if (Sign!=0.f && Sign!=NeighborSign)
                            return Fail(TEXT("Triangle corners disagree on handedness"));
                        Sign = NeighborSign;
                    }
                    if (Tangent.IsNearlyZero() || Tangent.ContainsNaN() || FMath::Abs(Sign)!=1.f)
                        return Fail(TEXT("Triangle UV tangent is invalid"));
                    auto Row = MakeShared<FJsonObject>();
                    Row->SetNumberField(TEXT("source_triangle"),Triangle.GetValue());
                    Row->SetNumberField(TEXT("source_corner"),Instance.GetValue());
                    Row->SetNumberField(TEXT("render_vertex"),V);
                    Row->SetStringField(TEXT("tangent"),Tangent.ToString());
                    Row->SetNumberField(TEXT("binormal_sign"),Sign);
                    Row->SetNumberField(TEXT("uv_determinant"),Determinant);
                    Row->SetStringField(TEXT("method"),TEXT("double-precision UV gradient projected onto source normal; handedness from valid render triangle neighbors"));
                    Repaired.Add(MakeShared<FJsonValueObject>(Row));
                }
                else
                {
                    const float D = FVector3f::DotProduct(FVector3f::CrossProduct(Normals[Instance],Tangent),Vertex.TangentY);
                    if (!FMath::IsFinite(D) || FMath::Abs(D)<.000001f) return Fail(TEXT("Valid tangent has ambiguous handedness"));
                    Sign = D<0.f ? -1.f : 1.f;
                }
                Tangents[Instance] = Tangent;
                Signs[Instance] = Sign;
            }
            ++Matched;
        }
        for (const auto& Entry : Triangles)
            if (!Entry.Value.IsEmpty()) return Fail(TEXT("Unmatched render triangles remain"));
        if (Repaired.Num()!=1) return Fail(TEXT("Expected exactly one zero corner"));
        Mesh->GetLODInfo(0)->BuildSettings.bRecomputeTangents = false;
        Mesh->CommitMeshDescription(0);
        Mesh->Build();
        FAssetCompilingManager::Get().FinishAllCompilation();
        Mesh->SetImportedBounds(Source->GetImportedBounds());
        if (Mesh->GetMorphTargets().Num()!=Source->GetMorphTargets().Num()) return Fail(TEXT("Morph inventory changed"));
        auto After = Inspect(Mesh);
        if (After->GetNumberField(TEXT("zero_tangents"))!=0 || After->GetNumberField(TEXT("nonunit_tangents"))!=0)
            return Fail(TEXT("Baked mesh still has invalid tangent lengths"));
        FSavePackageArgs Save;
        Save.TopLevelFlags=RF_Public|RF_Standalone;
        Save.SaveFlags=SAVE_NoError;
        const FString Filename=FPackageName::LongPackageNameToFilename(Target,FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(Package,Mesh,*Filename,Save)) return Fail(TEXT("Could not save private target"));
        Report->SetObjectField(TEXT("after"),After);
        Report->SetArrayField(TEXT("repaired_corners"),Repaired);
        Report->SetNumberField(TEXT("matched_triangles"),Matched);
        Report->SetStringField(TEXT("target"),Target);
    }
    Report->SetStringField(TEXT("source"),Input);
    Report->SetStringField(TEXT("scope"),TEXT("Private editor tangent bake or reload inspection. Cooked surface/geometry/morph preservation and gameplay remain unverified."));
    FString Text;
    FJsonSerializer::Serialize(Report,TJsonWriterFactory<>::Create(&Text));
    return FFileHelper::SaveStringToFile(Text,*Output) ? 0 : Fail(TEXT("Could not save report"));
}
