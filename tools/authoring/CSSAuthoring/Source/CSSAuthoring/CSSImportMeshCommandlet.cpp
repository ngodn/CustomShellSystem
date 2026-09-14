#include "CSSImportMeshCommandlet.h"

#include "Animation/Skeleton.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, CSSAuthoring)
DEFINE_LOG_CATEGORY_STATIC(LogCSSAuthoring, Log, All);

namespace
{
bool ReadNumbers(const TSharedPtr<FJsonValue>& Value, int32 Count, TArray<double>& Out)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Value.IsValid() || !Value->TryGetArray(Values) || Values->Num() != Count)
        return false;
    Out.Reset(Count);
    for (const auto& Item : *Values)
    {
        double Number = 0;
        if (!Item->TryGetNumber(Number) || !FMath::IsFinite(Number))
            return false;
        Out.Add(Number);
    }
    return true;
}

bool Index(double Value, int32 Count)
{
    return Value >= 0 && Value < Count && FMath::FloorToDouble(Value) == Value;
}

bool SaveAsset(UObject* Asset)
{
    UPackage* Package = Asset->GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Package, Asset, *Filename, Args);
}
}

UCSSImportMeshCommandlet::UCSSImportMeshCommandlet()
{
    IsEditor = true;
    IsClient = false;
    IsServer = false;
    LogToConsole = true;
}

int32 UCSSImportMeshCommandlet::Main(const FString& Params)
{
    auto Fail = [](const FString& Why) {
        UE_LOG(LogCSSAuthoring, Error, TEXT("%s"), *Why);
        return 1;
    };
    FString Filename;
    if (!FParse::Value(*Params, TEXT("Input="), Filename))
        return Fail(TEXT("Specify -Input=/absolute/path/mesh.json"));
    FString Text;
    TSharedPtr<FJsonObject> Root;
    const int64 FileSize = IFileManager::Get().FileSize(*Filename);
    if (FileSize <= 0 || FileSize > 512LL * 1024 * 1024)
        return Fail(TEXT("Input must be a non-empty JSON file of at most 512 MiB"));
    if (!FFileHelper::LoadFileToString(Text, *Filename) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
        return Fail(TEXT("Cannot read the input JSON object"));

    double Schema = 0;
    FString PackageName, SkeletonPackageName;
    if (!Root->TryGetNumberField(TEXT("schema"), Schema) || Schema != 1 ||
        !Root->TryGetStringField(TEXT("mesh_package"), PackageName) ||
        !Root->TryGetStringField(TEXT("skeleton_package"), SkeletonPackageName) ||
        !PackageName.StartsWith(TEXT("/Game/CSSAuthoring/")) ||
        !SkeletonPackageName.StartsWith(TEXT("/Game/")) ||
        !FPackageName::IsValidLongPackageName(PackageName) ||
        !FPackageName::IsValidLongPackageName(SkeletonPackageName))
        return Fail(TEXT("Invalid schema or asset package names"));
    if (PackageName == SkeletonPackageName ||
        FPackageName::DoesPackageExist(PackageName) ||
        FPackageName::DoesPackageExist(SkeletonPackageName))
        return Fail(TEXT("Choose distinct, unused mesh and skeleton package paths"));

    const TArray<TSharedPtr<FJsonValue>> *Bones, *Points, *Wedges, *Faces, *Influences, *Materials;
    if (!Root->TryGetArrayField(TEXT("bones"), Bones) || Bones->IsEmpty() || Bones->Num() > 2048 ||
        !Root->TryGetArrayField(TEXT("points"), Points) || Points->IsEmpty() || Points->Num() > 1000000 ||
        !Root->TryGetArrayField(TEXT("wedges"), Wedges) || Wedges->IsEmpty() || Wedges->Num() > 6000000 ||
        !Root->TryGetArrayField(TEXT("faces"), Faces) || Faces->IsEmpty() || Faces->Num() > 2000000 ||
        !Root->TryGetArrayField(TEXT("influences"), Influences) || Influences->IsEmpty() ||
        Influences->Num() > Points->Num() * 8 ||
        !Root->TryGetArrayField(TEXT("materials"), Materials) || Materials->IsEmpty() || Materials->Num() > 64)
        return Fail(TEXT("Missing arrays or exceeded mesh limits"));

    double UVCount = 1;
    if (Root->HasField(TEXT("uv_channels")) &&
        (!Root->TryGetNumberField(TEXT("uv_channels"), UVCount) || !Index(UVCount - 1, MAX_TEXCOORDS)))
        return Fail(TEXT("uv_channels must be an integer from one through MAX_TEXCOORDS"));
    const TArray<TSharedPtr<FJsonValue>> *Normals = nullptr, *Colors = nullptr;
    if (Root->HasField(TEXT("normals")) &&
        (!Root->TryGetArrayField(TEXT("normals"), Normals) || Normals->Num() != Wedges->Num()))
        return Fail(TEXT("normals must contain one vector per wedge"));
    if (Root->HasField(TEXT("colors")) &&
        (!Root->TryGetArrayField(TEXT("colors"), Colors) || Colors->Num() != Wedges->Num()))
        return Fail(TEXT("colors must contain one RGBA value per wedge"));
    FSkeletalMeshImportData Data;
    Data.NumTexCoords = static_cast<int32>(UVCount);
    Data.MaxMaterialIndex = Materials->Num() - 1;
    Data.bHasNormals = Normals != nullptr;
    Data.bHasTangents = false;
    Data.bHasVertexColors = Colors != nullptr;
    TArray<FVector3f> WedgeNormals;
    TSet<FName> BoneNames;
    TArray<FTransform> BoneTransforms;
    TArray<double> Values;
    for (int32 I = 0; I < Bones->Num(); ++I)
    {
        const TSharedPtr<FJsonObject>* Bone;
        FString Name;
        double Parent = 0;
        if (!(*Bones)[I]->TryGetObject(Bone) ||
            !(*Bone)->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty() || BoneNames.Contains(FName(*Name)) ||
            !(*Bone)->TryGetNumberField(TEXT("parent"), Parent) ||
            (I == 0 ? Parent != -1 : !Index(Parent, I)))
            return Fail(FString::Printf(TEXT("Invalid bone hierarchy at %d"), I));
        BoneNames.Add(FName(*Name));
        if (!ReadNumbers((*Bone)->TryGetField(TEXT("translation")), 3, Values))
            return Fail(TEXT("Invalid bone translation"));
        FVector Translation(Values[0], Values[1], Values[2]);
        if (!ReadNumbers((*Bone)->TryGetField(TEXT("rotation")), 4, Values))
            return Fail(TEXT("Invalid bone rotation"));
        FQuat Rotation(Values[0], Values[1], Values[2], Values[3]);
        if (FMath::Abs(Rotation.SizeSquared() - 1.0) > .001)
            return Fail(TEXT("Bone quaternion is not normalized"));
        if (!ReadNumbers((*Bone)->TryGetField(TEXT("scale")), 3, Values) ||
            Values[0] <= 0 || Values[1] <= 0 || Values[2] <= 0)
            return Fail(TEXT("Invalid bone scale"));
        FTransform Transform(Rotation, Translation, FVector(Values[0], Values[1], Values[2]));
        BoneTransforms.Add(Transform);
        SkeletalMeshImportData::FBone Entry{};
        Entry.Name = Name;
        Entry.ParentIndex = static_cast<int32>(Parent);
        Entry.BonePos.Transform = FTransform3f(Transform);
        Data.RefBonesBinary.Add(Entry);
        if (I > 0)
            ++Data.RefBonesBinary[Entry.ParentIndex].NumChildren;
    }
    TSet<FName> MaterialNames;
    for (const auto& Material : *Materials)
    {
        FString Name;
        if (!Material->TryGetString(Name) || Name.IsEmpty() || MaterialNames.Contains(FName(*Name)))
            return Fail(TEXT("Material names must be non-empty unique names"));
        MaterialNames.Add(FName(*Name));
        SkeletalMeshImportData::FMaterial Entry;
        Entry.MaterialImportName = Name;
        Entry.Material = UMaterial::GetDefaultMaterial(MD_Surface);
        Data.Materials.Add(Entry);
    }
    FBox Bounds(ForceInit);
    for (const auto& Point : *Points)
    {
        if (!ReadNumbers(Point, 3, Values) ||
            FMath::Abs(Values[0]) > 10000 || FMath::Abs(Values[1]) > 10000 || FMath::Abs(Values[2]) > 10000)
            return Fail(TEXT("Invalid mesh point (expected Unreal centimeters)"));
        Data.Points.Emplace(Values[0], Values[1], Values[2]);
        Bounds += FVector(Values[0], Values[1], Values[2]);
        Data.PointToRawMap.Add(Data.PointToRawMap.Num());
    }
    for (const auto& Wedge : *Wedges)
    {
        if (!ReadNumbers(Wedge, 1 + 2 * Data.NumTexCoords, Values) || !Index(Values[0], Points->Num()))
            return Fail(TEXT("Invalid wedge point or UV"));
        SkeletalMeshImportData::FVertex Entry{};
        Entry.VertexIndex = static_cast<uint32>(Values[0]);
        for (int32 Channel = 0; Channel < Data.NumTexCoords; ++Channel)
            Entry.UVs[Channel] = FVector2f(Values[1 + 2 * Channel], Values[2 + 2 * Channel]);
        Entry.Color = FColor::White;
        if (Normals)
        {
            if (!ReadNumbers((*Normals)[Data.Wedges.Num()], 3, Values))
                return Fail(TEXT("Invalid wedge normal"));
            FVector3f Normal(Values[0], Values[1], Values[2]);
            if (FMath::Abs(Normal.SizeSquared() - 1.0f) > .001f)
                return Fail(TEXT("Wedge normal is not normalized"));
            WedgeNormals.Add(Normal);
        }
        if (Colors)
        {
            if (!ReadNumbers((*Colors)[Data.Wedges.Num()], 4, Values))
                return Fail(TEXT("Invalid wedge color"));
            for (double Value : Values)
                if (!Index(Value, 256))
                    return Fail(TEXT("Wedge colors must be RGBA bytes, 0 through 255"));
            Entry.Color = FColor(Values[0], Values[1], Values[2], Values[3]);
        }
        Data.Wedges.Add(Entry);
    }
    for (const auto& Face : *Faces)
    {
        if (!ReadNumbers(Face, 4, Values) || !Index(Values[0], Wedges->Num()) ||
            !Index(Values[1], Wedges->Num()) || !Index(Values[2], Wedges->Num()) ||
            !Index(Values[3], Materials->Num()))
            return Fail(TEXT("Invalid face wedge or material index"));
        SkeletalMeshImportData::FTriangle Entry{};
        for (int32 Corner = 0; Corner < 3; ++Corner)
        {
            Entry.WedgeIndex[Corner] = static_cast<uint32>(Values[Corner]);
            if (Normals)
                Entry.TangentZ[Corner] = WedgeNormals[Entry.WedgeIndex[Corner]];
        }
        const FVector3f A = Data.Points[Data.Wedges[Entry.WedgeIndex[0]].VertexIndex];
        const FVector3f B = Data.Points[Data.Wedges[Entry.WedgeIndex[1]].VertexIndex];
        const FVector3f C = Data.Points[Data.Wedges[Entry.WedgeIndex[2]].VertexIndex];
        if (FVector3f::CrossProduct(B - A, C - A).SizeSquared() < 1.e-12f)
            return Fail(TEXT("Degenerate triangle in input"));
        Entry.MatIndex = static_cast<uint16>(Values[3]);
        Entry.SmoothingGroups = 1;
        Data.Faces.Add(Entry);
    }
    TArray<double> Totals;
    Totals.SetNumZeroed(Points->Num());
    TArray<int32> Counts;
    Counts.SetNumZeroed(Points->Num());
    TSet<uint64> SeenInfluences;
    for (const auto& Influence : *Influences)
    {
        if (!ReadNumbers(Influence, 3, Values) || !Index(Values[0], Points->Num()) ||
            !Index(Values[1], Bones->Num()) || Values[2] <= 0 || Values[2] > 1)
            return Fail(TEXT("Invalid skin influence"));
        SkeletalMeshImportData::FRawBoneInfluence Entry{};
        Entry.VertexIndex = static_cast<int32>(Values[0]);
        Entry.BoneIndex = static_cast<int32>(Values[1]);
        Entry.Weight = static_cast<float>(Values[2]);
        const uint64 Key = (static_cast<uint64>(Entry.VertexIndex) << 32) | static_cast<uint32>(Entry.BoneIndex);
        if (SeenInfluences.Contains(Key))
            return Fail(TEXT("Duplicate point/bone influence"));
        SeenInfluences.Add(Key);
        Totals[Entry.VertexIndex] += Values[2];
        ++Counts[Entry.VertexIndex];
        Data.Influences.Add(Entry);
    }
    for (int32 I = 0; I < Totals.Num(); ++I)
        if (FMath::Abs(Totals[I] - 1.0) > .001 || Counts[I] > 8)
            return Fail(FString::Printf(TEXT("Point %d needs normalized weights with at most eight influences"), I));

    UPackage* MeshPackage = CreatePackage(*PackageName);
    USkeletalMesh* Mesh = NewObject<USkeletalMesh>(MeshPackage,
        *FPackageName::GetLongPackageAssetName(PackageName), RF_Public | RF_Standalone);
    UPackage* SkeletonPackage = CreatePackage(*SkeletonPackageName);
    USkeleton* Skeleton = NewObject<USkeleton>(SkeletonPackage,
        *FPackageName::GetLongPackageAssetName(SkeletonPackageName), RF_Public | RF_Standalone);
    FReferenceSkeleton Reference;
    {
        FReferenceSkeletonModifier Modifier(Reference, Skeleton);
        for (int32 I = 0; I < Data.RefBonesBinary.Num(); ++I)
        {
            const auto& Bone = Data.RefBonesBinary[I];
            Modifier.Add(FMeshBoneInfo(FName(*Bone.Name), Bone.Name, Bone.ParentIndex), BoneTransforms[I]);
        }
    }
    Mesh->SetRefSkeleton(Reference);
    Mesh->SetSkeleton(Skeleton);
    for (const auto& Material : Data.Materials)
        Mesh->GetMaterials().Emplace(Material.Material.Get(), true, false,
            FName(*Material.MaterialImportName), FName(*Material.MaterialImportName));
    FSkeletalMeshLODInfo& LOD = Mesh->AddLODInfo();
    Mesh->GetImportedModel()->LODModels.Add(new FSkeletalMeshLODModel());
    LOD.BuildSettings.bRecomputeNormals = !Data.bHasNormals;
    LOD.BuildSettings.bRecomputeTangents = true;
    LOD.BuildSettings.bUseMikkTSpace = true;
    LOD.BuildSettings.bUseFullPrecisionUVs = true;
    LOD.BuildSettings.BoneInfluenceLimit = 8;
    LOD.ReductionSettings.NumOfTrianglesPercentage = 1;
    LOD.ReductionSettings.NumOfVertPercentage = 1;
    FMeshDescription Description;
    if (!Data.GetMeshDescription(Mesh, &LOD.BuildSettings, Description))
        return Fail(TEXT("Engine could not create the skeletal mesh description"));
    Mesh->CreateMeshDescription(0, MoveTemp(Description));
    Mesh->CommitMeshDescription(0);
    Mesh->SetImportedBounds(FBoxSphereBounds(Bounds));
    Mesh->CalculateInvRefMatrices();
    if (!Skeleton->MergeAllBonesToBoneTree(Mesh))
        return Fail(TEXT("Engine rejected the skeleton hierarchy"));
    Mesh->Build();
    FAssetCompilingManager::Get().FinishAllCompilation();
    const FSkeletalMeshLODModel& BuiltLOD = Mesh->GetImportedModel()->LODModels[0];
    if (BuiltLOD.NumVertices == 0 || BuiltLOD.Sections.IsEmpty() ||
        BuiltLOD.IndexBuffer.Num() != Data.Faces.Num() * 3)
        return Fail(TEXT("Built mesh has missing geometry or changed triangle count"));
    FAssetRegistryModule::AssetCreated(Skeleton);
    FAssetRegistryModule::AssetCreated(Mesh);
    Mesh->MarkPackageDirty();
    Skeleton->MarkPackageDirty();
    if (!SaveAsset(Skeleton) || !SaveAsset(Mesh))
        return Fail(TEXT("Could not save the imported assets"));
    UE_LOG(LogCSSAuthoring, Display, TEXT("Imported %s: %d points, %d triangles, %d bones. Materials are placeholders; this is not a release package."),
        *PackageName, Data.Points.Num(), Data.Faces.Num(), Mesh->GetRefSkeleton().GetNum());
    return 0;
}
