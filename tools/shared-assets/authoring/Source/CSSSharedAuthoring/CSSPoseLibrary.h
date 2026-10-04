#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSPoseLibrary.generated.h"

class UAnimBlueprint;
class UAnimSequence;
class USkeletalMesh;

UCLASS()
class CSSSHAREDAUTHORING_API UCSSPoseLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CSS Shared Authoring")
    static UAnimBlueprint* CreateCopyPoseTemplate(const FString& OutputPackage);

    UFUNCTION(BlueprintCallable, Category="CSS Shared Authoring")
    static FString CheckCopyPose(UAnimBlueprint* Blueprint, USkeletalMesh* SourceMesh,
        USkeletalMesh* VisualMesh, UAnimSequence* Sequence, bool bWithPostProcess = false);
};
