"""Extend the private footwear fixture to Knitwear's mesh and material slots."""
import difflib
from pathlib import Path

root = Path(__file__).resolve().parents[3]
path = root / 'CSS-eins0fx-collections/tools/CSSAuthoring/Source/CSSAuthoring/CSSAnimationLibrary.cpp'
before = path.read_text()
assert 'const bool Knit =' not in before
anchor = '    const bool Visibility = Bikini && Blueprint->GetName() == TEXT("ABP_BikiniFeet2");'
assert before.count(anchor) == 1
text = before.replace(anchor, '''    const bool Knit = Mesh && Blueprint && Animation &&
        Mesh->GetPathName() == TEXT("/Game/CSS/EveTest/SK_KFit2.SK_KFit2") &&
        (Blueprint->GetPathName() == TEXT("/Game/CSS/EveTest/ABP_BikiniFeet.ABP_BikiniFeet") ||
         Blueprint->GetPathName() == TEXT("/Game/CSS/EveTest/ABP_KnitFeet1.ABP_KnitFeet1") ||
         Blueprint->GetPathName() == TEXT("/Game/CSS/SeduXtress/ABP_Secondary.ABP_Secondary")) &&
        (Animation->GetName() == TEXT("AN_Walk") || Animation->GetName() == TEXT("AN_Jog") || Animation->GetName() == TEXT("AN_Sprint")) &&
        Animation->GetPathName().StartsWith(TEXT("/Game/CSS/Eve/Anim/"));
    const bool Visibility = (Bikini && Blueprint->GetName() == TEXT("ABP_BikiniFeet2")) ||
        (Knit && Blueprint->GetName() == TEXT("ABP_KnitFeet1"));''')
replacements = {
    'const bool BikiniFeet = Bikini &&': 'const bool BikiniFeet = (Bikini || Knit) &&',
    '!Heels && !Bikini && (Mesh': '!Heels && !Bikini && !Knit && (Mesh',
    'for (int32 Material = 23; Material <= 28; ++Material)':
        'for (int32 Material = Knit ? 17 : 23; Material <= (Knit ? 22 : 28); ++Material)',
}
for old, new in replacements.items():
    assert text.count(old) == 1, old
    text = text.replace(old, new)
patch = root / 'CustomShellSystem/tools/eve-fit/native/knit-visibility-probe.patch'
assert not patch.exists()
patch.write_text(''.join(difflib.unified_diff(before.splitlines(True), text.splitlines(True),
    fromfile='a/CSSAnimationLibrary.cpp', tofile='b/CSSAnimationLibrary.cpp')))
path.write_text(text)
