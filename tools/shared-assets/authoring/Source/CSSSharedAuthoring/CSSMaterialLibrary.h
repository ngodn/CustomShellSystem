#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSSMaterialLibrary.generated.h"

class UMaterial;
class UMaterialExpression;

UCLASS()
class CSSSHAREDAUTHORING_API UCSSMaterialLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CSS Shared Authoring")
    static TArray<UMaterialExpression*> GetExpressions(UMaterial* Material);
};
