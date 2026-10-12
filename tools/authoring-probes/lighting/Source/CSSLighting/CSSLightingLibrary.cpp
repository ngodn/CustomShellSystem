#include "CSSLightingLibrary.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, CSSLighting)

bool UCSSLightingLibrary::ConnectClothWeight(UMaterial* Material, UMaterialExpression* Weight)
{
    if (!Material || !Weight || Weight->GetOuter() != Material ||
        !Material->GetShadingModels().HasShadingModel(MSM_Cloth))
        return false;
    // Python omits MP_CustomData0. Cloth stores its weight in this input.
    if (!UMaterialEditingLibrary::ConnectMaterialProperty(Weight, TEXT(""), MP_CustomData0))
        return false;
    const auto* Input = Material->GetExpressionInputForProperty(MP_CustomData0);
    return Input && Input->Expression == Weight && Input->OutputIndex == 0;
}
