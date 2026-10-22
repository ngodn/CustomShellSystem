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

inline constexpr FShape ExpandedShapes[] = {
    {TEXT("PBMGlutesSize"), TEXT("CSS_ClothGlute")},
    {TEXT("PBMHipSize"), TEXT("CSS_ClothHip")},
    {TEXT("PBMThighsTone"), TEXT("CSS_ClothThigh")},
    {TEXT("PBMWaistWidth"), TEXT("CSS_ClothWaist")},
    {TEXT("FBMBodyTone"), TEXT("CSS_ClothTone")},
    {TEXT("PBMGlutesWidth"), TEXT("CSS_ClothGluteWidth")},
    {TEXT("PBMGlutesUpDown"), TEXT("CSS_ClothGluteLift")},
    {TEXT("PBMHipBoneSize"), TEXT("CSS_ClothHipBone")},
    {TEXT("PBMThighsSize"), TEXT("CSS_ClothThighSize")},
    {TEXT("PBMCalvesSize"), TEXT("CSS_ClothCalfSize")},
};

inline TConstArrayView<FShape> SelectShapes(bool bExpanded)
{
    return bExpanded ? MakeArrayView(ExpandedShapes) : MakeArrayView(Shapes);
}
}
