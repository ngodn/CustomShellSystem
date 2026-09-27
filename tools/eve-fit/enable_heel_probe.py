"""Extend the editor evaluator to one private foot-correction graph."""
import difflib
from pathlib import Path

root = Path(__file__).resolve().parents[3]
path = root / 'CSS-eins0fx-collections/tools/CSSAuthoring/Source/CSSAuthoring/CSSAnimationLibrary.cpp'
before = path.read_text()
assert 'const bool Heels =' not in before
text = before.replace('#include "BoneControllers/AnimNode_AnimDynamics.h"',
    '#include "BoneControllers/AnimNode_AnimDynamics.h"\n#include "BoneControllers/AnimNode_ModifyBone.h"')
anchor = '    const bool Holiday = Mesh && Blueprint && Animation &&'
assert text.count(anchor) == 1
text = text.replace(anchor, '''    const bool Heels = Mesh && Blueprint && Animation &&
        Mesh->GetPathName() == TEXT("/Game/CSS/EveTest/SK_HolidayFollow.SK_HolidayFollow") &&
        Blueprint->GetPathName() == TEXT("/Game/CSS/EveTest/ABP_BikiniFeet.ABP_BikiniFeet") &&
        (Animation->GetName() == TEXT("AN_Walk") || Animation->GetName() == TEXT("AN_Jog") || Animation->GetName() == TEXT("AN_Sprint")) &&
        Animation->GetPathName().StartsWith(TEXT("/Game/CSS/Eve/Anim/"));
''' + anchor)
old = '(!Holiday && !Follow && (Mesh->GetPathName()'
assert text.count(old) == 1
text = text.replace(old, '(!Holiday && !Follow && !Heels && (Mesh->GetPathName()')
anchor = '    if (Holiday)\n    {\n'
assert text.count(anchor) == 1
text = text.replace(anchor, '''    if (Heels)
    {
        int32 Count = 0;
        for (TFieldIterator<FStructProperty> It(Instance->GetClass()); It; ++It)
        {
            if (It->Struct != FAnimNode_ModifyBone::StaticStruct()) continue;
            const auto* Node = It->ContainerPtrToValuePtr<FAnimNode_ModifyBone>(Instance);
            const FName Name = Node->BoneToModify.BoneName;
            const int32 Index = Ref.FindBoneIndex(Name);
            if ((Name != TEXT("foot_l") && Name != TEXT("foot_r")) || Index == INDEX_NONE || ChangedBones.Contains(Index) ||
                Node->RotationMode != BMM_Additive || Node->RotationSpace != BCS_BoneSpace ||
                Node->TranslationMode != BMM_Ignore || Node->ScaleMode != BMM_Ignore)
                return Fail(TEXT("Invalid private foot correction"));
            ChangedBones.Add(Index);
            ++Count;
        }
        if (Count != 2) return Fail(TEXT("Expected two foot controls"));
    }
''' + anchor)
patch = root / 'CustomShellSystem/tools/eve-fit/native/bikini-feet-probe.patch'
assert not patch.exists()
patch.write_text(''.join(difflib.unified_diff(before.splitlines(True), text.splitlines(True),
    fromfile='a/CSSAnimationLibrary.cpp', tofile='b/CSSAnimationLibrary.cpp')))
path.write_text(text)
