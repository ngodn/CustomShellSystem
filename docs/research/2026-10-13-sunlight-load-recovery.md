# Sunlight option recovery after loading

The October 13 cold restart exposed a saved-option failure. CSS state still
selected `reduced`, but the live manager's directional light was back at 4
degrees. Diagnostics reported zero modified lights and one relinquished light.
The active game preset also specified 4 degrees and its timeline was stopped.
This follows the old ownership rule: any later value permanently released the
light until the user reselected the option.

## Change

A relinquished light can recover when its value matches the active game preset,
the lighting timeline is stopped, and the same preset/value is observed on two
successive two-second checks. The current game value becomes the restoration
value. Other custom angles remain untouched. No preset asset, CVar, shadow
method, weather choice or save-game data is edited. This is a source-angle
workaround with sharper shadows, not a general denoiser correction.

The game dump and live reflection establish `ActiveLightPreset.SourceAngle`
and `SpartaLightingTimeline`. Epic documents
[`UTimelineComponent::IsPlaying`](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Components/UTimelineComponent?application_version=5.5)
as its playback query; the local 5.6.1 headers and live signature confirm the
boolean, zero-input call used here.

## Validation

- Portable sunlight tests pass, including load reset, an active weather blend,
  restoration to the new weather value, unrelated later writers, expired lights,
  failed restoration retry and saved-state migration.
- Windows development core builds with `CSS_INVENTORY_DEV=ON`.
- Normal QuitGame exits the old process. The new core is installed while closed,
  then Steam launches a fresh process. The loader selector and mapped DLL agree.
- Normal ContinueGame loads the existing save. Reduced mode reads 0.535700023
  degrees, one modified light, no relinquished lights and no error.
- Default restores exactly 4 degrees. Reduced applies again. An explicit test
  write of the game's 4-degree value simulates a later preset reset; recovery
  occurs about 3.8 seconds later. Default again restores exactly 4 degrees.
- Twenty post-load health samples over about 40 seconds pass: facial performance,
  all four selected locomotion assets, sunlight ownership and no maintenance error.
- Final CSS state matches the pre-test state exactly; Reduced is left enabled.
  The pawn remains unchanged throughout the lighting test.

Installed core SHA256:
`7a6d18a3fbc3916bf54955c050862c8538e2da801d5ca735eb670766ebf8cf7c`.
Scripts, raw requests, loader identity, values and screenshot are retained in
`work/three-outfit-rig-physics/sunlight-load-recovery1`. The screenshot is a
recovery record near a bright beacon, not a matched visual comparison.
The preliminary startup helper incorrectly required Wine's process ID to change;
Wine reused 388. Its read-only wait timed out. Verification instead uses the
new Linux PID, selected core and actual process mapping. No second game was
started by that helper.

Real weather transitions, other maps, continuous-motion artifact comparison and
performance still need coverage. The independent animation crash remains open;
this cold-installed change does not claim to fix it.

## October 15: enabling during an active transition

The initial acquisition path did not check the lighting timeline. Enabling
Reduced noise during a blend could therefore capture an intermediate source
angle and overwrite the game while it was still changing that value. Recovery
of an already owned light had the guard, but first acquisition did not.

A regression reproduced this before the fix, exiting with
`Enabling during a weather blend overwrote game lighting`. Initial acquisition
now uses the same stopped-timeline check as recovery. A missing timeline also
prevents acquisition. Turning the option off during that wait writes nothing;
after the timeline stops, CSS captures the completed value for restoration.

The targeted `css_sunlight` test passes, including enabling, disabling and
re-enabling during the blend, applying afterward, and restoring the completed
value. This is offline lifecycle coverage, not new visual acceptance or a
claim that the general sunlight artifact is fixed. The running game still uses
the accepted shell-list core `cd942c57df307581`; this follow-up is not installed.
Development (`CSS_INVENTORY_DEV=ON`) and shipping (`OFF`) Windows core builds
both pass. Test outcome and artifact hashes are recorded in
`work/three-outfit-rig-physics/sunlight-enable-transition-20261015/result.json`.
