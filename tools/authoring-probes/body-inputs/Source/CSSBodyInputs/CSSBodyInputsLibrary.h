#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSBodyInputsLibrary.generated.h"

UCLASS()
class UCSSBodyInputsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CSS|Diagnostics")
    static FString Evaluate(class USkeletalMesh* Mesh, class UAnimBlueprint* Blueprint, int32 Fps);
};
