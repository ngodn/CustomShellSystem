# CSS preview light controls

The native orbit and menu controls are installed in the short-path trial core.
Live lighting/input/lifecycle acceptance remains open. The controller correction
below is installed in `css_core-ui1.dll`; the user reports the controls work.
Controlled orbit/reset, tab exit/reentry and fresh-menu defaults now pass.
Input remapping, focus loss and broader performance coverage remain open.

## Actual game references

The retained `BP_DisplayMenu_Character` export contains `RectLight_Left`,
`RectLight_AxeLight`, `SpotLight_Fill` and `SpotLight_Rim`. A historical live
inventory capture resolves those components on the actual preview instance.
`RectLight_Left` is the candidate primary light: its template is above and to
one side, with intensity 1400 and attenuation radius about 2269 cm. The other
lights have distinct authored positions and lighting roles. Their relative
visual contributions still require a live comparison; component names alone
do not prove which causes every highlight in the user's screenshot.

Evidence is condensed with source hashes in
`work/preview-light-v1/source-evidence.json`. The inputs are:

- `work/inventory-integration/blueprints/BP_DisplayMenu_Character.json`
- `work/inventory-integration/development/1789409134885282595-inspect.json`

The recorded menu input map assigns `Gamepad_Special_Left` to `IA_Menu_Inspect`,
also bound to `I`. The same live capture has CSS active, the native character
page's `bOpen=false`, and all four character-page input listeners disabled.
The exported `IsMenuOpen` function simply returns that `bOpen` flag.
`work/preview-light-v2/inspect-routing.json` and `IsMenuOpen.json` retain these
observations. The native menu-close bytecode calls `UpdateOpenState(false)` and
invalidates character navigation and inventory before returning.

CSS reuses the keyboard side of mapped Inspect on its own character page.
The controller uses the Y/Select correction described below. On
entry to lighting, it checks the native page is closed and those four listeners
remain disabled. It queries current mapped keys for all actions in the menu
mapping context and removes a lighting shortcut if another menu action or CSS
view-reset binding also uses it. The original captured `I` and Select/View keys
had no such conflicts. The revised controller pair is checked against the
remaining native menu actions before assignment. Remapping and other active contexts still need live
verification; the historical capture is not evidence about all configurations.

## Implemented backend

`inventory_light.hpp` owns a rigid orbit around a captured character pivot.
It rotates the light's position and complete basis together, preserving the
authored distance, aim and roll. Camera right/up axes establish screen-relative
motion. Elevation is limited to 80 degrees in either direction; azimuth wraps
without interpolating across the wrap. Nonfinite transforms and input are
rejected before applying them.

`inventory_light.inl` captures only the current preview instance's
`RectLight_Left`, after checking the active CSS page and live component class.
It saves relative location/rotation for restoration, obtains actual world
axes and uses the pinned UE 5.6.1 reflected setters. It changes no intensity,
color, other lights, templates, outfit data or saved settings. Transform
functions are verified against `SceneComponent.h` and `KismetMathLibrary.h`.
[Epic documents world rotation as updating the relative rotation](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USceneComponent/K2_SetWorldRotation),
which is why the original relative transform is retained for restoration.

While armed, the existing right-stick and mouse-drag path moves the light.
Camera movement, framing and zoom calls are blocked. Entry clears camera
velocity and drag state and cancels scripted preview-camera motion. Loss of
focus uses the existing input reset. Mode stop, camera stop, detach and a
switch to/from CSSX restore the original light transform if the object survives.
Weak references prevent restoring into a destroyed preview instance.

Development builds expose these requests through the existing inventory
request channel:

- `inventory_light_start`
- `inventory_light_move`, with numeric `horizontal` and `vertical` degrees
- `inventory_light_stop`

They contain no script-side lighting behavior. They are available in the installed short-path trial core. The older accepted
V44 core predates them and must not receive these commands. Start is
idempotent, so repeated start requests cannot overwrite the captured original.
Stop currently restores the original light as a diagnostic cleanup operation.
The user-facing toggle is separate and retains the edited light when returning
to view controls. Re-entering lighting captures the current world basis while
retaining the first original relative transform for that component. Explicit
light reset restores the original and keeps lighting mode active. View reset
uses the original camera values without stopping/restarting the whole preview,
so it no longer discards edited lighting.

## Menu controls

The center controls now offer `Lighting` beside `Reset view`. In lighting mode,
they become `View controls` and `Reset light`, with `Lighting control (view
locked)` and a move-light hint. The requested controller shortcut is Y, with Select/View taking the former Y
row actions (favorite, reset all and delete profile). Keyboard `I` still follows
the native Inspect remapping; row actions retain their keyboard mappings. If the current input
method has no nonconflicting shortcut, the clickable control remains available
without advertising a misleading key glyph. Home/right-stick click resets the
currently controlled object. Text entry, pickers and confirmation dialogs retain
their existing input handling; lighting cannot be entered through them.

## Verification and remaining work

The C++23 host light test passes independently of `assert`/`NDEBUG`. It checks
known quarter/full rotations, a translated character pivot, aim and handedness,
rigid distance, 30/60/144 FPS integration, elevation bounds, overflow-resistant
large input and rejection without mutation. The existing camera-motion test
also passes. These tests do not prove live camera ownership or rendering.

`work/preview-light-v1` retains the build/test logs. The first host build failed
because the generated makefiles did not yet contain the new target; CMake was
regenerated, and `host-build-v2.log` plus `tests.log` pass. The final development
build (`windows-build-v2.log`) and shipping build (`shipping-build.log`) both
exit 0. `verification.json` retains their hashes and the unchanged installed
V44 core selector. No new DLL was deployed and no live light request was sent.

The menu integration builds separately in `work/preview-light-v2`. Development
and shipping Windows builds both exit 0. Orbit math is unchanged from the
passing host test. The menu input, retoggle/reset lifetime and visual layout
are compiled but still require live validation; a build is not evidence of
physical controller behavior or exact restoration in the game.

Before enabling the feature for users:

1. Verify the Inspect handoff, default and remapped keys, keyboard/controller
   prompts, conflicting bindings and clickable controls in the actual menu.
2. Check return-to-view/re-enter-light cycles preserve edits, view reset preserves
   the light, and light reset restores the first original while retaining mode.
3. In the actual CSS preview, compare rendered camera location, rotation and
   FOV before/during movement, verify actual light readback, and review Steam
   stills or a game-window clip from contrasting light positions.
4. Verify reset, focus loss, page switches, exit/re-entry and preview recreation,
   including exact restoration of game-owned light state. Check performance.

The full modular physics/motion architecture and V44 weapon, damage/parry,
grounding, heel geometry and gameplay acceptance remain open.

## Y shortcut correction

2026-09-21: The user reported B beside Lighting and explicitly requested Y.
The previous call supplied controller enum 1, which the shipped
`E_ControllerButton` defines as Back. The live mapping was still Select/View,
so the visible glyph did not describe the polled key. The shipped enum defines
FaceUp as 2 and SpecialLeft as 17. Evidence: retained
`work/inventory-integration/blueprints/E_ControllerButton.json`,
`WBP_Prompt.json`, and fresh `work/paths/live1/input1.json`.

CSS now assigns Y to lighting and Select/View to the former tertiary controller
action. It retains keyboard mappings and does not change native input settings.
Keys reserved by other native menu actions, including a remapped Back action,
take priority; the clickable controls remain. Both changed prompts use their
actual CSS key and omit the native InputAction that could refresh a conflicting
icon. An unavailable lighting action no longer falls through to a row action.

C++23 host tests cover Y/Select ownership, unchanged B/Escape and C/I bindings,
and conflicting remapped navigation. The first host compile exposed a missing
vector include; its subsequently executed old test binary is not evidence for
this change. After correcting the include, `work/light-y1/host2-exit.json`,
`tests-exit.json` and `build2-exit.json` all report zero. The development and shipping
Windows cores compile. `work/ui-live1/deployment.json` records the normal
restart into `css_core-ui1.dll`, SHA-256
`eeed7db4c1adf1533aae40a0134432af40d4b629e86e4012eb37686fce5fabe4`.
The outfit trio and saved state were unchanged by deployment. The user reports
Y/Select/B work, and `menu.json` confirms their actual CSS mappings.

The first check found CSS closed before any test mutation. After reopening,
`light2` verified actual camera lock, changed primary-light position, constant
orbit radius, unchanged intensity and untouched secondary lights. Its three
Steam stills were visually reviewed. Its exact reset comparison rejected only
floating-point differences, at most 8.15e-12 cm, so the checker now bounds the
restored transform at numerical precision while keeping untouched state exact.
`light3` subsequently observed camera/parent rotation changes during sequential
reads. The user confirmed they were testing the controls, so that run cannot
establish a camera-lock defect. Both unsuccessful attempts remain retained.

With controls left untouched, `light4/verification.json` passes both diagnostic
orbit positions, exact camera location/rotation/FOV preservation, unchanged
secondary lights, rigid orbit distance and restoration. Maximum restored
relative-location error is 1.081e-12 cm; world position is exact. Both contrasting
Steam stills were visually reviewed and show the changed illumination. Physical
Y/Select/B use is separately supported by the user's confirmation and actual
live bindings. Remapped-input UI, retoggle/view-reset preservation, focus loss,
menu recreation and performance still need their broader lifecycle checks.

## Tab exit and menu recreation

`tools/check_preview_light_lifecycle.py` passes in
`work/ui-live1/lifecycle3/verification.json` on the installed grounding core.
After an edited light, switching to Inventory restores the original transform
within 8.15e-12 cm and 2.85e-14 degrees, and CSS reentry retains that baseline.
After another edit and full menu close, reopening creates a different light
whose original relative position, rotation and intensity match exactly. The
reopened Steam still was reviewed. No runtime lighting code changed for this
check; the new grounding core carries the previously tested lighting code.

Retained failures explain the corrected probe:

- `lifecycle1` incorrectly required the old component's detached relative
  transform to match even after full menu teardown. A weak handle was still
  readable briefly; later it invalidated and a new light had the exact baseline.
- `lifecycle2` discovered that the old component's `GetOwner` returns null during
  teardown. The subsequent actor-function description rejected the null target
  before invocation. The probe now handles absent ownership explicitly.
- `lifecycle3` records that same ownerless old component and checks the fresh
  replacement. It does not claim the detached old component was restored.

The engine exposes component destruction separately from reference validity;
see the pinned `ActorComponent.h` and
[Epic's IsBeingDestroyed reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Components/UActorComponent/IsBeingDestroyed?application_version=5.5).
The observed old component returns false for that function despite having no
owner, so the check does not equate weak validity or that one flag with a live
preview actor. This covers one tab cycle and one complete menu recreation,
not input remapping, focus changes or a performance benchmark.
