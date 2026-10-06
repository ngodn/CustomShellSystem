#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSNativePerformanceLibrary.generated.h"

UCLASS()
class CSSAUTHORING_API UCSSNativePerformanceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Exercise native montage ownership in an isolated preview component. */
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static FString EvaluateMontageOwnership(class USkeletalMesh* ReferenceMesh,
        class UAnimSequence* Animation);
};
