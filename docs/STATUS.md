# CSS status, 2026-09-13

The native C++23 wardrobe and three self-contained appearance packages are installed and working. HIT2 is credited to XTGMods. Both Beaute mods are credited to dantemk2, confirmed by the user. After testing HIT2 appearance, movement and attacks, the user replied: "yes seems perfect".

## Color milestone

The rebuilt DLL includes the credited wardrobe artwork, per-part RGB sliders, glow strength, Original restoration, Crimson/Midnight palettes, linked material parameters and saved colors. The three color-enabled packages from `dist/colors-final/` are installed and their nine files match the verified builds. Matching developer recipes and masks were backed up and retired in `backups/packages-1789304494820124044`. The credited wardrobe artwork also matches the project source. Fresh-launch validation passed for all four appearances using embedded colors, with no developer recipes present. Saved HIT2 custom colors restored automatically. Genessa skin, face, eyes and glow controls then applied from its package. Closing restored the real character and unpaused gameplay; the Colors page is open for user testing. [Packaged live report](../work/colors-packaged-live.json), [final view](../work/colors-packaged-final.png).

Validation: 54 native behavioral checks and 26 Python tests pass. The final three color packages pass full container verification and native metadata/mask cache repair and corruption tests. Twelve live appearance/palette combinations passed (regular Genessa, corrupted Genessa, Knight Lady, HIT2; Original, Crimson, Midnight). Checks also covered saved custom looks, steady default framing, cloth tick isolation and reload with active custom colors. [Color guide and limitations](colors.md), [live report](../work/color-lifecycle.json), [reload evidence](../work/colors-reload-verified.json), [package checks](../work/colors-final-package-build.log).

Current core: `css_core-bcd707d21ddab9e5-1789304199265211966.dll`, acknowledged by Windows PID 372. The build completed without warnings. Color masks and render targets use 2048 pixels in these POC recipes; Original retains authored texture resolution. Initial texture loading and broader performance, damage/death/travel and third-party material interactions still need profiling or regression.

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

Earlier package milestone core: `css_core-902a45f5dbf27a59-1789299689483247205.dll`. [Reload acknowledgement](../work/packaged-ui-final-reload.log). Current game observed as Linux PID 638723, Windows PID 372. Mounted container changes required one restart; `_P` correction was completed before that launch.

Validation: 34 native behavioral checks, 22 Python tests, native package reader/cache/corruption checks on all three final trios, full container hashes, cooked payload round trips, live four-appearance animation/cloth checks and user gameplay confirmation. Final native compilation had no warnings. This is not a claim of zero bugs or measured performance overhead.

## Artifacts

- [Package guide](css-packages.md), [converter](../tools/css_convert.py), [verifier/installer](../tools/css_package.py).
- [Final packages](../dist), [package verification](../work/final-package-verification.log), [Python checks](../work/final-python-tests.log).
- [Cloth findings](preview-cloth.md), [live lifecycle report](../work/cloth-preview-lifecycle.json), [wardrobe screenshot](../work/packaged-final-wardrobe.png).
- Prototype backup: `backups/packages-1789299191603415319`. Pre-`_P` package backup: `backups/packages-1789299338866944554`.
- [Earlier preview research](animated-preview.md), [native research](native-css-research.md), [SDK notes](../reference/ue4ss-sdk-d7e7826d/SDK-NOTES.md).

## Remaining broader work

Full CNS parity is unfinished: independent cosmetic slots, general texture choices, shape keys, material toggles and an animation library. Death/travel/load boundaries, other gameplay shell saves, long combat sessions, more controller/display combinations and controlled performance profiling need broader regression. Other procedural physics and third-party behavior require individual validation; the cloth fix does not prove universal compatibility.


## Variant port update, 2026-09-14

Four new local bundles contain fourteen outfit variants with per-variant material overrides and color recipes. Their final containers passed extraction, payload round-trip, resolved material/skeleton/physics checks and native cache repair tests. The updated core and four trios are installed. All fourteen variants passed live palette/custom-color/Original restoration, animated preview/cloth and clean-close checks. The user confirmed mouse and controller scrolling across the full catalog. The user reported that the outfits seem correct during the requested visual and gameplay check; long-session regression remains open. See [porting-variants.md](porting-variants.md) for source details, limits and artifact locations. Earlier current-core entries above are historical.

The user confirmed all new mods are working. The tested variant, color and scrolling changes are the basis for the next runtime release.
