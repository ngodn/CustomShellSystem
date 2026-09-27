"""Add bounded shown/hidden/toggled playback cases for the private Bikini mesh."""
import difflib
from pathlib import Path
root = Path(__file__).resolve().parents[3]
path = root / 'CSS-eins0fx-collections/tools/CSSAuthoring/Source/CSSAuthoring/CSSAnimationLibrary.cpp'
before = path.read_text()
assert 'const bool Bikini =' not in before
anchor = '    const bool Heels = Mesh && Blueprint && Animation &&'
assert before.count(anchor) == 1
text = before.replace(anchor, '''    const bool Bikini = Mesh && Blueprint && Animation &&
        Mesh->GetPathName() == TEXT("/Game/CSS/EveTest/SK_BFit1.SK_BFit1") &&
        (Blueprint->GetPathName() == TEXT("/Game/CSS/EveTest/ABP_BikiniFeet.ABP_BikiniFeet") ||
         Blueprint->GetPathName() == TEXT("/Game/CSS/EveTest/ABP_BikiniFeet2.ABP_BikiniFeet2") ||
         Blueprint->GetPathName() == TEXT("/Game/CSS/SeduXtress/ABP_Secondary.ABP_Secondary")) &&
        (Animation->GetName() == TEXT("AN_Walk") || Animation->GetName() == TEXT("AN_Jog") || Animation->GetName() == TEXT("AN_Sprint")) &&
        Animation->GetPathName().StartsWith(TEXT("/Game/CSS/Eve/Anim/"));
    const bool Visibility = Bikini && Blueprint->GetName() == TEXT("ABP_BikiniFeet2");
    const bool BikiniFeet = Bikini && Blueprint->GetName() != TEXT("ABP_Secondary");
''' + anchor)
text = text.replace('!Holiday && !Follow && !Heels && (Mesh', '!Holiday && !Follow && !Heels && !Bikini && (Mesh')
text = text.replace('(!Holiday && HolidayControl != 0)', '(!Holiday && !Visibility && HolidayControl != 0) || (Visibility && HolidayControl > 2)')
assert text.count('    if (Heels)\n') == 1
text = text.replace('    if (Heels)\n', '    if (Heels || BikiniFeet)\n')
anchor = '        for (auto* C : {Upstream, Component})\n'
assert text.count(anchor) == 1
text = text.replace(anchor, '''        const bool ShoeShown = HolidayControl == 0 || (HolidayControl == 2 && (Frame / 20) % 2 == 0);
        if (Visibility)
            for (int32 Material = 23; Material <= 28; ++Material)
                Component->ShowMaterialSection(Material, INDEX_NONE, ShoeShown, 0);
''' + anchor)
anchor = '        Sample->SetObjectField(TEXT("upstream"), PoseJson(A));'
assert text.count(anchor) == 1
text = text.replace(anchor, anchor + '''
        if (Visibility)
        {
            const auto* Enabled = FindFProperty<FBoolProperty>(Instance->GetClass(), TEXT("CSSHeelsEnabled"));
            if (!Enabled) return Fail(TEXT("Missing private heel visibility state"));
            Sample->SetBoolField(TEXT("shoe_visible"), ShoeShown);
            Sample->SetBoolField(TEXT("heel_enabled"), Enabled->GetPropertyValue_InContainer(Instance));
        }''')
patch = root / 'CustomShellSystem/tools/eve-fit/native/bikini-visibility-probe.patch'
assert not patch.exists()
patch.write_text(''.join(difflib.unified_diff(before.splitlines(True), text.splitlines(True),
    fromfile='a/CSSAnimationLibrary.cpp', tofile='b/CSSAnimationLibrary.cpp')))
path.write_text(text)
