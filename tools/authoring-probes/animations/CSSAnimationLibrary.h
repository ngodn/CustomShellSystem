#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSAnimationLibrary.generated.h"

/** Headless, editor-only animation authoring. The caller owns saving. */
UCLASS()
class UCSSAnimationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Retarget a curve-free, non-additive source clip into an isolated package. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static class UAnimSequence* RetargetClip(class USkeletalMesh* SourceMesh,
        class USkeletalMesh* TargetMesh, class UAnimSequence* Source,
        class UIKRetargeter* Retargeter, const FString& OutputPackage,
        bool PreserveUnmappedAttachments = true);

    /** Evaluate compressed animation with the installed authoring post-process, without saving. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static FString EvaluateClip(class USkeletalMesh* Mesh, class UAnimSequence* Animation,
        class UAnimBlueprint* Blueprint, int32 Loops = 3,
        class UBlendSpace* Carrier = nullptr, FVector BlendInput = FVector(0,0,0));

    /** Wrap a sequence in a private constant blend space; never alter a game asset. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static class UBlendSpace* CreateIdleCarrier(class USkeletalMesh* Mesh,
        class UAnimSequence* Animation, const FString& OutputPackage);
};
