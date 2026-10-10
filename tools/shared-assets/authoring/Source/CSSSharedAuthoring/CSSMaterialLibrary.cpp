#include "CSSMaterialLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"

TArray<UMaterialExpression*> UCSSMaterialLibrary::GetExpressions(UMaterial* Material)
{
    TArray<UMaterialExpression*> Result;
    if (!IsRunningCommandlet() || !IsInGameThread() || !IsValid(Material))
        return Result;
    const FString Path = Material->GetPathName();
    if (!Path.StartsWith(TEXT("/Game/CSS/SharedAssets/Astral/")) &&
        !Path.StartsWith(TEXT("/Game/CSS/eins0fx/Shared/Astral/")))
        return Result;
    for (auto Expression : Material->GetExpressions())
        if (Expression) Result.Add(Expression.Get());
    return Result;
}
