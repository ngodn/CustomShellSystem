#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSLightingLibrary.generated.h"

UCLASS()
class UCSSLightingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CSS|Lighting")
    static bool ConnectClothWeight(class UMaterial* Material, class UMaterialExpression* Weight);
};
