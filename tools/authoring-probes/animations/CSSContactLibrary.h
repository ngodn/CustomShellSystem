#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSContactLibrary.generated.h"

UCLASS()
class UCSSContactLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Bake measured sole clearance into an isolated bone-only clip. Never changes the source. */
    UFUNCTION(BlueprintCallable, Category = "CSS|Authoring")
    static class UAnimSequence* BakeSoleContact(class USkeletalMesh* Mesh,
        class UAnimSequence* Source, const TArray<float>& LeftHeights,
        const TArray<float>& RightHeights, float SmoothingCm, const FString& OutputPackage,
        bool PreserveSourceKneePlane = false);
};
