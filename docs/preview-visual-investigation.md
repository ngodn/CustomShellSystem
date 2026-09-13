# Seductress wardrobe rendering and floor alignment

2026-09-14. The user reports darker, glossier materials in the wardrobe and an
apparent gap under the feet. The wardrobe lighting remains unresolved. The user deferred further lighting investigation on 2026-09-14.
The live comparison is `python3 tools/check_preview_visual.py`, with the
wardrobe open. It compares effective material references, custom primitive data
and lighting channels. Socket positions and capsule bottom are measured, but a
bone socket is not automatically the visible shoe sole.

The first extended diagnostic core loaded, then crashed while automatically
reapplying saved colors before its new inspection ran. The crash at 02:29:33 is
`UECC-Windows-3A5DDAB440BEAFB6733D62A5202DDDC3_0000` in the game's Proton Saved/Crashes
directory. Its CSS frames resolve using the unchanged-object linker map to
`Appearance::customize` and its `mid_for` helper, at the weak-handle assignment
after `CreateDynamicMaterialInstance`. UE4SS frames include
`FWeakObjectPtr::operator=`, `FUObjectArray::AllocateSerialNumber`,
`Conv_ObjectToSoftObjectReference` and `FString::operator=`.

The pinned SDK's `Unreal/src/UObjectArray.cpp:297` allocates missing serials by
calling the compiled soft-reference wrapper. That wrapper uses the SDK's older
soft-reference layout. CSS already uses reflected parameter frames for asset
loading to avoid that ABI mismatch. `WeakObject` now does the same for serial
initialization, checks object-array identity and a nonzero resulting serial,
then delegates to the normal weak handle. It does not pin objects or cache raw
handles across garbage collection. All CSS weak handles use this route.

The corrected build compiled and is staged as
`css_core-d4132ec952cca9d9-1789324511853428531.dll`. Cold launch, color application, several active core reloads and fresh inspection subsequently succeeded. The settings comparison passes, but the user still sees the visual lighting defect.


## Deferred: wardrobe lighting and washed-out appearance

User confirmation: still visibly wrong across outfits, including after both fixes
below. Do not mark this resolved from material/settings equality. Resume only
when requested. The user suggested environment-aware lighting attached to the
wardrobe camera; this remains a possible direction, not a completed feature.

Confirmed corrections currently retained:

- Source lighting channel bits were 3, preview bits were 1. Preview now copies
  all three channels through SetLightingChannels.
- Preview now copies the player's effective CustomPrimitiveDataInternal values.
- Hiding the whole pawn also hid its BP_PlayerLightRigComponent child. CSS now
  hides only the source mesh and weapon actors, preserving that light actor and
  the pawn's other components. The rig's original pause-tick flag is restored on
  close. A direct ReceiveTick experiment was removed when investigation was deferred.

Live readback: rig visible, Cam_Light channel bits 2 / intensity 70,
Rim_Left_Light and Rim_Right_Light channel bits 2 / intensity 150. Matching
channels and visibility do not prove the lights reach the preview correctly.
The preview camera has default, non-overridden exposure and bloom settings.
The saved gameplay view target is the pawn, so reading a CameraComponent from
that target does not retrieve the gameplay camera's effective post-processing.
Next investigation should inspect PlayerCameraManager.CameraCachePrivate.POV
and the game's CameraStateFramework processing, with actual image comparisons.

## Preview alignment

The standalone idle lacks gameplay foot IK. After its first evaluated pose,
CSS aligns the lowest ball socket to the frozen gameplay pose once. A measured
adjustment was -2.572 cm. It moves only the disposable preview, preserves the
camera, and requests a cloth teleport. This is pose alignment, not terrain IK
or a guarantee that every outfit's visible sole touches the ground.

## Beacon cancellation

Captured live after cancelling the Marrow Keep beacon: Seductress's eight-slot
mesh had eleven stock Genessa override slots, with corrupt-top materials on its
skin and garment. Custom primitive data was zero. The game's shell effect had
restored gameplay materials onto the cosmetic mesh.

CSS now remembers its applied override pointers, detects stock asset materials
restored by the game, and reapplies the outfit and saved colors without resetting
the skeletal mesh when it is unchanged. Unfamiliar materials and transient MIDs
remain under game control. The first recovery used three 250 ms polls; the user
confirmed recovery but saw a brief flash. The current version checks at engine
post-tick and removes that deliberate delay. The shortened transition still
needs user confirmation. No new hooks into the reloadable core were installed.

## Separate Seductress asset correction

The source boots and heeled feet used only BG3 Knee_L/Knee_R weights. Automatic
calf fitting left their rest-pose soles about 12 cm high. The collection's
fit_seductress_feet.py now extends the fitted sole to the rest floor, keeps the
upper leg seam fixed, and adds ankle/toe weights. A first candidate that rotated
the shoes along Genessa's steep ankle-to-ball vector was rejected in visual review.

Prepared package: ../CSS-eins0fx-collections/build/packages/prototype-v3,
from native cook release-v4. Verification confirms 1,513 changed foot/boot
positions, unchanged topology/UVs, all 258 bones, other weights, material slots,
and both cooked cloth simulations (equal-weight influence order normalized).
This asset package is NOT installed or tested in gameplay yet. Current installed
package remains prototype-v2 until the game can be closed for container replacement.

## Inventory preview materials

The separate BP_Character_Menu preview had the CSS mesh with eleven stock
Genessa overrides. CSS now copies the selected player's effective overrides to
that actor, only while it displays the current gameplay shell. It restores the
menu's original mesh and materials when ownership ends. It leaves the native
menu camera, lighting and pose intact.

`python3 tools/check_menu_preview.py` failed before this change and passed after
live reload. The user confirmed the character looked correct, including after
closing and reopening inventory. The fix is committed as 2517e47.

## Lighting investigation resumed using inventory as a reference

After confirming inventory, the user requested comparison with its correct
lighting, reopening the previously deferred investigation. Fresh inspection
found four dedicated menu lights (SpotLight_Rim, RectLight_AxeLight,
SpotLight_Fill and RectLight_Left). Its character is staged separately from the
player, almost 100 metres higher. Therefore material equality alone cannot
reproduce its illumination in the world-based CSS preview. Measurements are in
work/inventory-lighting.json. A CSS lighting fix is not yet confirmed.
