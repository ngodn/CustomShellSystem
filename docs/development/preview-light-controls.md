# CSS preview light controls

The native orbit and menu controls are implemented and compiled locally. They
are not installed or visually accepted. V44 gameplay verification remains the
active live test. Lighting work proceeded while that game process had no player
pawn; this does not establish a finished, verified lighting feature.

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

CSS now reuses that mapped Inspect action only on its own character page. On
entry to lighting, it checks the native page is closed and those four listeners
remain disabled. It queries current mapped keys for all actions in the menu
mapping context and removes a lighting shortcut if another menu action or CSS
view-reset binding also uses it. The default captured `I` and Select/View keys
have no such conflicts. Remapping and other active contexts still need live
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

They contain no script-side lighting behavior. They must not be sent to the
currently installed V44 core, which predates these commands. Start is
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
locked)` and a move-light hint. The default shortcut is controller Select/View
or keyboard `I`, following the native Inspect remapping. If the current input
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
