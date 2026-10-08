#pragma once

#include "CoreMinimal.h"

namespace CSSClothBasisStudy
{
struct FShape
{
    const TCHAR* Morph;
    const TCHAR* Attribute;
};

inline constexpr FShape Shapes[] = {
    {TEXT("PBMGlutesSize"), TEXT("CSS_ClothGlute")},
    {TEXT("PBMHipSize"), TEXT("CSS_ClothHip")},
    {TEXT("PBMThighsTone"), TEXT("CSS_ClothThigh")},
    {TEXT("PBMWaistWidth"), TEXT("CSS_ClothWaist")},
    {TEXT("FBMBodyTone"), TEXT("CSS_ClothTone")},
};
}
