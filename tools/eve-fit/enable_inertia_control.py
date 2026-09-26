"""Add an equal-axis inertia and reset-only control to the private editor probe."""
from pathlib import Path
import difflib
root=Path(__file__).resolve().parents[3]
p=root/'CSS-eins0fx-collections/tools/CSSAuthoring/Source/CSSAuthoring/CSSAnimationLibrary.cpp'
before=p.read_text();after=before
backup=root/'CustomShellSystem/work/eve26/CSSAnimationLibrary.cpp.before-inertia'
assert not backup.exists()
assert after.count('HolidayControl > 3')==1
after=after.replace('HolidayControl > 3','HolidayControl > 15')
old='            for (const auto& Body : Node->PhysicsBodyDefinitions)\n            {'
assert after.count(old)==1
after=after.replace(old,'''            for (auto& Body : Node->PhysicsBodyDefinitions)
            {
                if (HolidayControl & 4)
                    Body.BoxExtents = FVector(FMath::Sqrt(Body.BoxExtents.SizeSquared() / 3.));''')
old='        if (Count != 12) return Fail(TEXT("Expected twelve Holiday dynamics bones"));'
assert after.count(old)==1
after=after.replace(old,old+'\n        if (HolidayControl & 8) Instance->ResetDynamics(ETeleportType::ResetPhysics);')
backup.write_text(before);p.write_text(after)
(root/'CustomShellSystem/tools/eve-fit/native/holiday-inertia.patch').write_text(''.join(difflib.unified_diff(before.splitlines(True),after.splitlines(True),fromfile='a/CSSAnimationLibrary.cpp',tofile='b/CSSAnimationLibrary.cpp')))
