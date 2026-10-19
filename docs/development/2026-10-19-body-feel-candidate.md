# Body feel candidate: core 1.0.2-rc1

A core-only candidate for the body motion work on `feature/uuepbs-body-sliders`. Not installed. Nothing here changes the loader, the outfit packages or saved looks.

| | |
| --- | --- |
| File | `work/core-1.0.2-rc1/css_core-1.0.2-rc1.dll` |
| SHA-256 | `565c02992197a638bafac3ea16cd57f87aec080ca9370de181130b552a4bf935` |
| Size | 4700160 bytes |
| Source | `1913a5d` on `feature/uuepbs-body-sliders`, Release, clang-cl, UE4SS headers d7e7826d (`build/uuepbs-windows`) |
| Exports | `css_get_api`, `css_get_api2`, `cssx_services` (core ABI 1; works with the installed 1.0.1 loader) |
| Receipt | `work/core-1.0.2-rc1/candidate.json` |

What it carries:

- The travel channel on angular body rigs (`b2c89a5`): a fifth slider value, Travel, in degrees, saved beside the four channel values.
- The Travel slider in the physics details (`be7a7c9`), dimmed with "This outfit's rig predates Travel" when the worn rig lacks the inputs.
- The soft limit band per template (`24ae9d8`): presets set the rig's SoftZone through the optional `CSSBodyRegionSoftZones`/`CSSBodySoftZone` pair.
- The body geometry reader for 6 to 8 morph axes (`1913a5d`): schema 2 models with 256 corners, and the cooked six-axis models unchanged.

Host tests on the same tree: ctest 29 of 29 suites, `css_control_tests` 516 checks, `css_body_geometry_tests` 6072 synthetic checks plus parity on the cooked six-axis model and Eve's 8-axis model, Python 97 tests.

## What the candidate does before any recook

On today's outfits it should behave like 1.0.1. No shipped recipe declares a `travel` range, so no Travel row appears. No shipped rig has the travel or soft zone inputs, so presets write frequency, damping and motion as before and the rigs keep their own hard limits. The cooked six-axis geometry models load unchanged. Installing it alone is therefore a regression check of the core, not a feel test.

## Install (game closed)

The installed selector today is `{"abi": 1, "file": "css_core-1.0.1.dll"}`. Paths below are this machine's.

```sh
GAME=/mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2
MOD="$GAME/Binaries/Win64/ue4ss/Mods/CustomShellSystem"
SRC=/home/eins0fx/development/mods/msII/CustomShellSystem/work/core-1.0.2-rc1/css_core-1.0.2-rc1.dll

# 1. The game must not be running (prints 0 when it is closed).
ps -eo args | grep -c '[M]ortalShell2-Win64-Shipping.exe'

# 2. Keep the current selector for rollback.
cp -p "$MOD/core.json" "$MOD/core.json.pre-1.0.2-rc1"

# 3. Copy the core and check it arrived intact.
cp "$SRC" "$MOD/cores/css_core-1.0.2-rc1.dll"
sha256sum "$MOD/cores/css_core-1.0.2-rc1.dll"
# expect 565c02992197a638bafac3ea16cd57f87aec080ca9370de181130b552a4bf935

# 4. Point the selector at it (write a temp file, then replace).
printf '{\n  "abi": 1,\n  "file": "css_core-1.0.2-rc1.dll"\n}\n' > "$MOD/core.json.tmp"
mv "$MOD/core.json.tmp" "$MOD/core.json"
```

Then start the game through Steam. `CSS.log` in `$MOD` should show `Core activated: css_core-1.0.2-rc1.dll`; "Core load failed" or "Core ABI rejected" means the loader kept the old core.

## UUEPBS beside the candidate

For the compatibility half of the live check, UUEPBS 2.7.1 is staged under `work/uuepbs-install/` with its own README (install steps, the seeded `MortalShell2.json` profile from `docs/research/uuepbs/MortalShell2.profile.json`, check list, removal). It installs after the core and the Eve recook, in the same closed-game window.

## Rollback

Close the game, then:

```sh
mv "$MOD/core.json.pre-1.0.2-rc1" "$MOD/core.json"     # back to css_core-1.0.1.dll
rm "$MOD/cores/css_core-1.0.2-rc1.dll"                  # optional
```

Saved looks stay readable both ways: 1.0.1 ignores the `travel` key a look saved with the candidate may carry.

## Recooks before the slider and the soft limit do anything

Per outfit (Eve, Unholy Genessa, Commander White, tHorny Proxima), in its package lane:

1. **Body rig** rebuilt as BodyAngularV11 from that outfit's current rig, which keeps its hierarchy and members:
   `tools/authoring/body-rig/build_controlrig_body_timed.py -CSSBodyTravelInput -CSSBodyHierarchyFrom=<current body rig>`.
   V11 adds `RegionTravel[7]`, `Travel`, `SoftZone`, `RegionSoftZones[7]` (both 0.5) and `ClampContacts`. For Eve that is the rig behind `CR_Body` and the post-process AnimBP `ABP_Secondary` in `/Game/CSS/CSS_SeduXtress_eins0fx_P/`; the other lanes name theirs.
2. **Post-process AnimBP** rebuilt on that rig with the CSSAuthoring module patched by `tools/authoring-patches/body-travel-inputs.patch` (travel and soft zone inputs). Add `body-geometry-axes.patch` when the outfit ships an 8-axis geometry model (Eve after her export with the new morphs; see the UUEPBS research doc).
3. **Recipe**: a `travel` range on each angular body control, default equal to that region's current effective cap; for the 8-axis model, `solver_axis` on the eight shape controls.
4. **Check before cook**: `check_combined_travel_inputs.py` (pairs present and transferring) and, for 8 axes, the geometry tools in `tools/authoring/body-geometry/`. After install, the live read-back probe (`sample_secondary_rigs.py` with `RegionTravel`, `Travel`, `RegionSoftZones`, `SoftZone` in its fields).

A rig recooked without the soft zone pair still gets travel; a rig with neither keeps its cooked limits. Commander White's AnimBP comes from its own library, so its lane ports the same inputs there.

## Presets in this candidate

Travel in degrees per region (chest, glutes, thighs, belly), clamped to each part's declared range. SoftZone is the band, as a fraction of the travel, where the rig pushes back instead of clamping.

| Preset | Chest | Glutes | Thighs | Belly | SoftZone | Chest rig tuning (Hz, damping, motion) |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Firm | 4 | 8 | 3 | 5 | 0.5 | 2.60, 0.65, 0.60 |
| Natural | 6 | 10 | 4 | 6 | 0.5 | 2.15, 0.48, 1.00 |
| Bouncy | 10 | 14 | 6 | 8 | 0.5 | 1.70, 0.28, 1.80 |
| Soft / Saggy | 14 | 16 | 7 | 9 | 0.5 | 1.25, 0.18, 2.60 |
| OMG! Earthquake! | 20 | 18 | 10 | 12 | 0.6 | 0.85, 0.06, 4.20 |

A part whose sliders no longer match a preset (hand-tuned) and every package preset use SoftZone 0.5. Natural 6/10 is looser than the 1.5.0 look the user accepted (about 3.8/8).

## Open

- **Soft limit gates.** Pass three (cubic, 0.5/0.6) passes gates 1, 3 and 5 but not 4 (5 of 12 fixtures under the 0.6 speed-change ratio, 10 needed) or 6 (tight bounce fails the knee check). The rows that fail still reach the hard limit. Numbers and options: `docs/research/2026-10-19-body-motion-travel-channel.md`, "Status after pass three". Fable decides the curve before the rig is recooked for players.
- **Trace comparison.** `compare_body_motion_feel.py`, the SpringBone reference and the analysis script are still to write and run (1.5.0 vs pre-limit vs 2.0.x vs travel/soft).
- **User live check.** Install the candidate, recook Eve with V11 and the patched AnimBP, then judge the five presets on walk, run and Iris Out. Needs the user's go for each install.
- **Template retune.** After the traces and the live verdict; travel per template and the SoftZone values may change.
- **Frame cost** of the V11 graph (`measure_body_graph_cost.py`) and the degree sign in the game font.
