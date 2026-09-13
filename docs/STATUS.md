# CSS status, 2026-09-13

The native C++23 wardrobe and three self-contained appearance packages are installed and working. HIT2 is credited to XTGMods. Both Beaute mods are credited to dantemk2, confirmed by the user. After testing HIT2 appearance, movement and attacks, the user replied: "yes seems perfect".

## Completed and verified

| Capability | Evidence |
| --- | --- |
| General Python converter | Accepts pak/utoc/ucas members or directories, discovers companions, isolates asset paths and verifies cooked round trips |
| Required naming | Default `CSS_${NAME}_${AUTHORorMODDER}_P`; custom templates receive `_P` if omitted; all nine installed files follow it |
| Complete outfit packages | Metadata, actual model portrait and conversion audit inside each pak, cooked assets in utoc/ucas |
| HIT2 DE Scyther | 13 relocated assets, explicit body/face/hair materials, live read-back and user gameplay confirmation |
| BeauteGenessa | Eight assets, regular and corrupted variants together, stable `beaute.genessa` ID preserved |
| BeauteKnightLady | Two assets, stable `beaute.knightlady` ID preserved |
| Package UI | All three packages discovered; embedded portraits rendered; single-variant entries show author instead of repeating the title |
| Migration | Old merged prototype/catalog backed up and retired; previous Genessa selection restored from its new package |
| Cosmetic isolation | Gameplay shell, abilities and animation instance preserved; no unlocks or game-save edits needed |
| Custom state | CSS settings, favorites and saved looks use atomic replacement and backups |
| Camera/input | Controller navigation and orbit, vertical-only inversion, deadzones, conditional recentering |
| Preview protection | Animated collision-free copy while world time is frozen; separate cloth tick now advances during pause |
| Cleanup and reload | Preview/camera/input/pause/visibility restoration verified, including reload while open |

Current core: `css_core-902a45f5dbf27a59-1789299689483247205.dll`. [Reload acknowledgement](../work/packaged-ui-final-reload.log). Current game observed as Linux PID 638723, Windows PID 372. Mounted container changes required one restart; `_P` correction was completed before that launch.

Validation: 34 native behavioral checks, 22 Python tests, native package reader/cache/corruption checks on all three final trios, full container hashes, cooked payload round trips, live four-appearance animation/cloth checks and user gameplay confirmation. Final native compilation had no warnings. This is not a claim of zero bugs or measured performance overhead.

## Artifacts

- [Package guide](css-packages.md), [converter](../tools/css_convert.py), [verifier/installer](../tools/css_package.py).
- [Final packages](../dist), [package verification](../work/final-package-verification.log), [Python checks](../work/final-python-tests.log).
- [Cloth findings](preview-cloth.md), [live lifecycle report](../work/cloth-preview-lifecycle.json), [wardrobe screenshot](../work/packaged-final-wardrobe.png).
- Prototype backup: `backups/packages-1789299191603415319`. Pre-`_P` package backup: `backups/packages-1789299338866944554`.
- [Earlier preview research](animated-preview.md), [native research](native-css-research.md), [SDK notes](../reference/ue4ss-sdk-d7e7826d/SDK-NOTES.md).

## Remaining broader work

Full CNS parity is unfinished: independent cosmetic slots, material editing and an animation library. Death/travel/load boundaries, other gameplay shell saves, long combat sessions, more controller/display combinations and controlled performance profiling need broader regression. Different per-variant material recipes are not supported yet. Other procedural physics and third-party behavior require individual validation; the cloth fix does not prove universal compatibility.
