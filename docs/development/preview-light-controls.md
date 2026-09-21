# CSS preview light controls

The native orbit backend is implemented and compiled, but is not installed or
exposed as a normal menu action yet. V44 gameplay verification remains the
active live test. The light backend was developed while that game process had
no player pawn. It does not establish a finished lighting feature.

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

The recorded menu input map already assigns `Gamepad_Special_Left` to
`IA_Menu_Inspect`, also bound to `I`. Do not add a second unexamined handler for
Select/View. Confirm the native Inspect listener is inactive on the CSS page,
or explicitly arbitrate that action only while CSS owns the page. `L`, `O` and
`P` are unused in this recorded menu map; this is not proof about every active
mapping context or a user's current remapped keys.

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
The eventual user-facing toggle must retain the edited light when returning to
view controls, and restore it only on explicit reset or preview exit.

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

Before enabling the feature for users:

1. Resolve Inspect routing and current key conflicts; add Select/View and a
   verified keyboard shortcut plus clear mode feedback and clickable controls.
2. Implement the user toggle separately from diagnostic stop: retain edited
   light placement on return to view controls, with a separate light reset.
3. In the actual CSS preview, compare rendered camera location, rotation and
   FOV before/during movement, verify actual light readback, and review Steam
   stills or a game-window clip from contrasting light positions.
4. Verify reset, focus loss, page switches, exit/re-entry and preview recreation,
   including exact restoration of game-owned light state. Check performance.

The full modular physics/motion architecture and V44 weapon, damage/parry,
grounding, heel geometry and gameplay acceptance remain open.
