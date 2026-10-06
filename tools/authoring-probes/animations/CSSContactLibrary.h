#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSContactLibrary.generated.h"

UCLASS()
class UCSSContactLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Read exact stored keys without converting frame indices through float time. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static FString ReadAuthoredTrack(class UAnimSequence* Source, FName Bone);

    /** Lift an isolated pelvis in world Z without rewriting Euler rotation channels. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static class UAnimSequence* BakePelvisClearance(class UAnimSequence* Source,
        const TArray<float>& Heights, const FString& OutputPackage);

    /** Reproduce UE 5.6's float-quaternion to Euler-channel import for diagnostics. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static FQuat SequencerRotationReference(FQuat Rotation);

    /** Bake measured sole clearance into an isolated bone-only clip. Never changes the source. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static class UAnimSequence* BakeSoleContact(class USkeletalMesh* Mesh,
        class UAnimSequence* Source, const TArray<float>& LeftHeights,
        const TArray<float>& RightHeights, float SmoothingCm, const FString& OutputPackage,
        bool PreserveSourceKneePlane = false);

    /** Optional measured world-space knee directions; zero entries retain the source plane. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static class UAnimSequence* BakeContactWithKneeDirections(class USkeletalMesh* Mesh,
        class UAnimSequence* Source, const TArray<float>& LeftHeights,
        const TArray<float>& RightHeights, float SmoothingCm, const FString& OutputPackage,
        bool PreserveSourceKneePlane, const TArray<FVector>& LeftDirections,
        const TArray<FVector>& RightDirections);
};
