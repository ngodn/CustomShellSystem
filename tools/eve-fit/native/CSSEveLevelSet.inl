#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

static int32 CreateEveLevelSet(const FString& Params)
{
    auto Fail=[](const TCHAR* Why) { UE_LOG(LogCSSEvePanel,Error,TEXT("%s"),Why); return 1; };
    FString Output, Report;
    const bool Inspect=FParse::Param(*Params,TEXT("InspectLevelSet"));
    if (!FParse::Value(*Params,TEXT("Output="),Output) ||
        !Output.StartsWith(TEXT("/Game/CSS/EveTest/PA_CBody")) ||
        !FPackageName::IsValidLongPackageName(Output) || (!Inspect && FPackageName::DoesPackageExist(Output)) ||
        !FParse::Value(*Params,TEXT("Report="),Report) || IFileManager::Get().FileExists(*Report))
        return Fail(TEXT("Require unused private PA_CBody output and report"));
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CSS/EveTest/SK_CBody.SK_CBody"));
    if (!Mesh) return Fail(TEXT("Missing verified body-only mesh"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    const auto* OriginalPhysics=Mesh->GetPhysicsAsset();
    FPhysAssetCreateParams Settings;
    Settings.GeomType=EFG_SkinnedLevelSet;
    Settings.VertWeight=EVW_AnyWeight;
    Settings.MinBoneSize=.01f;
    Settings.bCreateConstraints=false;
    Settings.LevelSetResolution=64;
    Settings.LatticeResolution=16;
    FParse::Value(*Params,TEXT("Grid="),Settings.LevelSetResolution);
    FParse::Value(*Params,TEXT("Lattice="),Settings.LatticeResolution);
    if (Settings.LevelSetResolution<8 || Settings.LevelSetResolution>128 ||
        Settings.LatticeResolution<8 || Settings.LatticeResolution>64)
        return Fail(TEXT("Grid or lattice outside trial bounds"));
    UPhysicsAsset* Asset=nullptr;
    if (Inspect)
    {
        const FString ObjectPath=Output+TEXT(".")+FPackageName::GetShortName(Output);
        Asset=LoadObject<UPhysicsAsset>(nullptr,*ObjectPath);
        if (!Asset) return Fail(TEXT("Cannot reload saved collider"));
    }
    else
    {
        auto* Package=CreatePackage(*Output);
        Asset=NewObject<UPhysicsAsset>(Package,*FPackageName::GetShortName(Output),RF_Public|RF_Standalone);
        FText Error;
        if (!FPhysicsAssetUtils::CreateFromSkeletalMesh(Asset,Mesh,Settings,Error,false,false))
            return Fail(*Error.ToString());
    }
    if (Mesh->GetPhysicsAsset()!=OriginalPhysics) return Fail(TEXT("Source physics changed"));
    auto Data=MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("mesh"),Mesh->GetPathName());
    Data->SetStringField(TEXT("asset"),Asset->GetPathName());
    Data->SetStringField(TEXT("scope"),TEXT("Private collider generation only; coverage, motion and cost unverified"));
    Data->SetBoolField(TEXT("fresh_load"),Inspect);
    if (!Inspect)
    {
        Data->SetNumberField(TEXT("requested_grid"),Settings.LevelSetResolution);
        Data->SetNumberField(TEXT("requested_lattice"),Settings.LatticeResolution);
    }
    TArray<TSharedPtr<FJsonValue>> Bodies;
    int32 Count=0;
    for (const USkeletalBodySetup* Body:Asset->SkeletalBodySetups)
    {
        for (const auto& Element:Body->AggGeom.SkinnedLevelSetElems)
        {
            const auto* Volume=Element.WeightedLevelSet().GetReference();
            if (!Volume || Volume->GetUsedBones().IsEmpty()) return Fail(TEXT("Empty weighted level set"));
            auto Row=MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("root_bone"),Body->BoneName.ToString());
            Row->SetStringField(TEXT("level_set_grid"),Element.LevelSetGridResolution().ToString());
            Row->SetStringField(TEXT("lattice_grid"),Element.LatticeGridResolution().ToString());
            TArray<TSharedPtr<FJsonValue>> Bones;
            for (FName Bone:Volume->GetUsedBones())
            {
                if (Mesh->GetRefSkeleton().FindBoneIndex(Bone)==INDEX_NONE) return Fail(TEXT("Unresolved collider bone"));
                Bones.Add(MakeShared<FJsonValueString>(Bone.ToString()));
            }
            Row->SetArrayField(TEXT("bones"),Bones);
            Bodies.Add(MakeShared<FJsonValueObject>(Row));
            ++Count;
        }
    }
    if (Count!=1 || !Asset->ConstraintSetup.IsEmpty()) return Fail(TEXT("Unexpected collider count or constraints"));
    Data->SetArrayField(TEXT("bodies"),Bodies);
    if (!Inspect)
    {
        FSavePackageArgs Save;
        Save.TopLevelFlags=RF_Public|RF_Standalone;
        Save.SaveFlags=SAVE_NoError;
        const FString Filename=FPackageName::LongPackageNameToFilename(Output,FPackageName::GetAssetPackageExtension());
        if (!UPackage::SavePackage(Asset->GetOutermost(),Asset,*Filename,Save)) return Fail(TEXT("Collider save failed"));
    }
    FString Text;
    if (!FJsonSerializer::Serialize(Data,TJsonWriterFactory<>::Create(&Text)) || !FFileHelper::SaveStringToFile(Text,*Report))
        return Fail(TEXT("Collider report failed"));
    return 0;
}
