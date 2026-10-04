#include "CSSMaterialLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"

TArray<UMaterialExpression*> UCSSMaterialLibrary::GetExpressions(UMaterial* Material)
{
    TArray<UMaterialExpression*> Result;
    if (!IsRunningCommandlet() || !IsValid(Material) ||
        !Material->GetPathName().StartsWith(TEXT("/Game/CSS/SharedAssets/Astral/")))
        return Result;
    for (auto Expression : Material->GetExpressions())
        if (Expression) Result.Add(Expression.Get());
    return Result;
}
