"""Extend existing editor-only probes to the exact private Holiday candidate paths."""
from pathlib import Path
import difflib

root=Path(__file__).resolve().parents[3]
work=root/'CustomShellSystem/work/eve26'
source=root/'CSS-eins0fx-collections/tools/CSSAuthoring/Source/CSSAuthoring'
patch=[]
for name in ('CSSRetargetLibrary.cpp','CSSAnimationLibrary.cpp'):
    p=source/name
    before=p.read_text()
    backup=work/(name+'.before-holiday')
    assert not backup.exists()
    text=before
    if name=='CSSRetargetLibrary.cpp':
        old='Mesh->GetPathName() == TEXT("/Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2") &&'
        new='(Mesh->GetPathName() == TEXT("/Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2") ||\n         Mesh->GetPathName() == TEXT("/Game/CSS/EveTest/SK_HolidayBones.SK_HolidayBones")) &&'
        assert text.count(old)==1
        text=text.replace(old,new)
    else:
        text=text.replace('#include "AnimNode_ControlRig.h"','#include "AnimNode_ControlRig.h"\n#include "BoneControllers/AnimNode_AnimDynamics.h"')
        start=text.index('FString UCSSAnimationLibrary::EvaluateClip')
        at=text.index('    if (!IsRunningCommandlet()',start)
        text=text[:at]+'''    const bool Holiday = Mesh && Blueprint && Animation &&
        Mesh->GetPathName() == TEXT("/Game/CSS/EveTest/SK_HolidayBones.SK_HolidayBones") &&
        Blueprint->GetPathName() == TEXT("/Game/CSS/EveTest/ABP_Holiday.ABP_Holiday") &&
        (Animation->GetName() == TEXT("AN_Walk") || Animation->GetName() == TEXT("AN_Jog") || Animation->GetName() == TEXT("AN_Sprint")) &&
        Animation->GetPathName().StartsWith(TEXT("/Game/CSS/Eve/Anim/"));
'''+text[at:]
        old='''        Mesh->GetPathName() != TEXT("/Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2") ||
        Blueprint->GetPathName() != TEXT("/Game/CSS/SeduXtress/ABP_Secondary.ABP_Secondary") ||
        !Animation->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/RT_")) ||'''
        new='''        (!Holiday && (Mesh->GetPathName() != TEXT("/Game/CSS/SeduXtress/SK_BlackPearl2.SK_BlackPearl2") ||
        Blueprint->GetPathName() != TEXT("/Game/CSS/SeduXtress/ABP_Secondary.ABP_Secondary") ||
        !Animation->GetPathName().StartsWith(TEXT("/Game/CSS/AnimLab/RT_")))) ||'''
        assert text.count(old)==1
        text=text.replace(old,new)
        at=text.index('    auto VectorJson =',start)
        text=text[:at]+'''    if (Holiday)
    {
        int32 Count = 0;
        for (TFieldIterator<FStructProperty> It(Instance->GetClass()); It; ++It)
        {
            if (It->Struct != FAnimNode_AnimDynamics::StaticStruct()) continue;
            auto* Node = It->ContainerPtrToValuePtr<FAnimNode_AnimDynamics>(Instance);
            Node->bDoPhysicsUpdateInEditor = true;
            for (const auto& Body : Node->PhysicsBodyDefinitions)
            {
                const FName Name = Body.BoundBone.BoneName;
                const int32 Index = Ref.FindBoneIndex(Name);
                if (Index == INDEX_NONE || ChangedBones.Contains(Index) || !Name.ToString().StartsWith(TEXT("CSS_Cloth_Skirt_")))
                    return Fail(TEXT("Unexpected Holiday dynamics bone"));
                ChangedBones.Add(Index);
                ++Count;
            }
        }
        if (Count != 12) return Fail(TEXT("Expected twelve Holiday dynamics bones"));
    }
'''+text[at:]
    backup.write_text(before)
    p.write_text(text)
    patch.extend(difflib.unified_diff(before.splitlines(True),text.splitlines(True),fromfile='a/'+name,tofile='b/'+name))
(root/'CustomShellSystem/tools/eve-fit/native/holiday-probes.patch').write_text(''.join(patch))
