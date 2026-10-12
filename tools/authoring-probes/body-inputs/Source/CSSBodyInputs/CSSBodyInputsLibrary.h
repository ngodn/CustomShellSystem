#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSBodyInputsLibrary.generated.h"

UCLASS()
class UCSSBodyInputsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CSS|Authoring")
    static bool SetBodyMovementLimits(class UBlueprint* Blueprint,
        const TArray<float>& Angles, const TArray<float>& Travel);

    UFUNCTION(BlueprintCallable, Category="CSS|Diagnostics")
    static FString Evaluate(class USkeletalMesh* Mesh, class UAnimBlueprint* Blueprint, int32 Fps);

    UFUNCTION(BlueprintCallable, Category="CSS|Diagnostics")
    static FString CompareRestoreSnapshot(class USkeletalMesh* Mesh, class UAnimBlueprint* Blueprint);
};
