"""Add transient spring/collision controls to the exact Holiday editor probe."""
from pathlib import Path
import difflib

root=Path(__file__).resolve().parents[3]
source=root/'CSS-eins0fx-collections/tools/CSSAuthoring/Source/CSSAuthoring'
work=root/'CustomShellSystem/work/eve26'
patch=[]
for name in ('CSSAnimationLibrary.h','CSSAnimationLibrary.cpp'):
    path=source/name
    before=path.read_text()
    backup=work/(name+'.before-controls')
    assert not backup.exists()
    after=before
    if name.endswith('.h'):
        old='bool AdvanceBlendClock = false);'
        assert after.count(old)==1
        after=after.replace(old,'bool AdvanceBlendClock = false, int32 HolidayControl = 0);')
    else:
        old='FVector BlendInput, bool AdvanceBlendClock)'
        assert after.count(old)==1
        after=after.replace(old,'FVector BlendInput, bool AdvanceBlendClock, int32 HolidayControl)')
        old='    if (AdvanceBlendClock && !Carrier)'
        assert after.count(old)==1
        after=after.replace(old,'''    if (HolidayControl < 0 || HolidayControl > 3 || (!Holiday && HolidayControl != 0))
        return Fail(TEXT("Invalid Holiday transient control"));
    if (AdvanceBlendClock && !Carrier)''')
        old='            Node->bDoPhysicsUpdateInEditor = true;'
        assert after.count(old)==1
        after=after.replace(old,old+'''
            if (HolidayControl & 1) Node->bAngularSpring = false;
            if (HolidayControl & 2) Node->bUseSphericalLimits = false;''')
        old='    Report->SetBoolField(TEXT("compressed_source"), true);'
        assert after.count(old)==1
        after=after.replace(old,'    Report->SetNumberField(TEXT("holiday_control"), HolidayControl);\n'+old)
    backup.write_text(before)
    path.write_text(after)
    patch.extend(difflib.unified_diff(before.splitlines(True),after.splitlines(True),fromfile='a/'+name,tofile='b/'+name))
(root/'CustomShellSystem/tools/eve-fit/native/holiday-controls.patch').write_text(''.join(patch))
