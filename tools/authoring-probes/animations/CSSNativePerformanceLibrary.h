#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSNativePerformanceLibrary.generated.h"

UCLASS()
class CSSAUTHORING_API UCSSNativePerformanceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Preserve an Eve locomotion blend's samples and rates with native sequence replacements. */
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static class UBlendSpace* BindNativeLocomotionBlend(class UBlendSpace* Source,
        const TArray<class UAnimSequence*>& Replacements, class USkeleton* NativeSkeleton,
        const FString& OutputPackage);

    /** Exercise native montage ownership in an isolated preview component. */
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString EvaluateMontageOwnership(class USkeletalMesh* ReferenceMesh,
        class UAnimSequence* Animation);
};
