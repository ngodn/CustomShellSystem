#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSAnimationLibrary.generated.h"

/** Headless, editor-only animation authoring. The caller owns saving. */
UCLASS()
class UCSSAnimationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Rotate a closed bone-only loop by whole keys, preserving its fitted binding. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static class UAnimSequence* ShiftLoop(class UAnimSequence* Source,
        int32 StartFrame, const FString& OutputPackage);

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
        class UBlendSpace* Carrier = nullptr, FVector BlendInput = FVector(0,0,0),
        bool AdvanceBlendClock = false);

    /** Wrap a sequence in a private constant blend space; never alter a game asset. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static class UBlendSpace* CreateIdleCarrier(class USkeletalMesh* Mesh,
        class UAnimSequence* Animation, const FString& OutputPackage);

    /** Author a private direction/speed blend with explicit per-sample rates. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static class UBlendSpace* CreateMovementBlend(class USkeletalMesh* Mesh,
        const TArray<class UAnimSequence*>& Animations, const TArray<FVector>& Points,
        const TArray<float>& Rates, float MaxSpeed, const FString& OutputPackage);

    /** Query the engine's interpolation weights, including wrapped directions. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static FString InspectMovementBlend(class UBlendSpace* Blend,
        const TArray<FVector>& Inputs);
};
