# Long Hair Proxima accessory report

Reviewed and tested 16 September 2026. The CSS attachment correction is live in
the local developer core. Published ZIPs are unchanged.

## Recording

JayLuk3 says only **LongHairProxima Ducttus - CSS Port** is installed. The supplied
recording is `~/Downloads/Watch 2026-09-15 08-58-42 - Trim Streamable.mp4`
(32.24 seconds, 1280×720, approximately 59.94 fps). Sampled frames across the
recording and closer samples around the accessory and transition are saved under
`work/support/jayluk3-2026-09-16/` (ignored local evidence).

An accessory lies flat near the character's feet, clearly visible around 9–14
seconds. Long Hair Proxima remains visible. The recording does not clearly show
a persistent return to vanilla appearance or an unresponsive CSS menu after
repeated hotkey swaps. Installed CSS, UE4SS and Cheat Menu versions are unknown.
The exact accessory and active gameplay shell need runtime identification.

## Attachment evidence

The source-exported Long Hair Proxima rig at
`../CSS-eins0fx-collections/ports/CSS_LongHairProxima_Ducttus/work/export-source-01/`
has 173 exported joint nodes. Neither `tiel_dagger` nor `Eredrim_Diapazon` is
present. The portrait export agrees. Exported joints are a diagnostic lead;
they are not a direct readback of every bone in the cooked runtime mesh.

The cached shared skeleton export at
`../CSS-eins0fx-collections/shibari-eins0fx-CSS/work/genessa-base/SKEL_Human_Skeleton.json`
contains these sockets:

| Socket | Parent bone |
| --- | --- |
| `tiel_daggerSocket` | `tiel_dagger` |
| `tiel_secondbelt` | `tiel_dagger` |
| `tiel_seconddagger` | `tiel_dagger` |
| `Eredrim_DiapazonSocket` | `Eredrim_Diapazon` |
| `Socket_Prop_Tiel_Dagger` | `spine_01` |
| `Socket_Prop_R_Diapason` | `prop_r` |

The generic prop sockets are different from the dedicated attachment sockets.
A shared skeleton can expose a socket whose parent bone is absent from an
individual replacement mesh.

UE 5.6.1 source confirms that `USkinnedMeshComponent::GetSocketTransform` starts
with the component transform and keeps it when a found socket has
`SocketBoneIndex == INDEX_NONE` (except a parent-bone-space request).
See `SkinnedMeshComponent.cpp:3221` in the local installed engine source.
This fallback is consistent with an accessory appearing at the mesh origin near
the feet. Epic also documents the component-transform fallback for unresolved
[socket transforms](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/Components/USkinnedMeshComponent/GetSocketTransform?application_version=5.5).

**Initial hypothesis:** cross-shell attachments require dedicated bones absent
from this replacement Proxima mesh. This fits both the recording and static
evidence, but requires checking the actual attachment parent, socket and bone
index in-game with Long Hair Proxima. The earlier `GetBoneIndex=-1` result was
for Seductress, not Long Hair Proxima. There is no evidence here that a different
UE4SS build caused the floor attachment.

## Separate shell-switch recovery report

CSS already has fixes and controlled tests for empty material instances and
leftover CSS dye materials blocking appearance recovery. Those tests do not
establish the cause of this user's earlier report, nor verify every original
Cheat Menu hotkey path. Keep this separate from missing accessory bones.

## Initial test plan (completed below)

On a safe session, compare stock Tiel and Eredrim against Long Hair Proxima on
the same gameplay shells. Record each accessory's component, attachment socket,
parent bone index and world transform before and after a switch. Separately
repeat the original hotkey sequence and record CSS recovery and menu state.
Do not globally offset, hide or reparent accessories based only on this clip.
The earlier follower-component experiment remains incomplete and unshipped.

No runtime changes, package changes or fixes were installed for the initial review.

## Fix development, 16 September

_eins0fx requested a fix after the initial review. Direct CUE4Parse readback of
the cooked Long Hair mesh now confirms 173 reference bones and the absence of
both `tiel_dagger` and `Eredrim_Diapazon`. The shared generic prop names listed
above do exist as bones, but do not satisfy the dedicated socket parents.

A hidden component fixture reproduced the engine failure without replacing the
player's mesh or switching gameplay shells. Commands from the CSS repository:

```sh
python3 work/support/jayluk3-2026-09-16/attachment-fixture.py
python3 work/support/jayluk3-2026-09-16/attachment-fixture.py --follower
python3 work/support/jayluk3-2026-09-16/attachment-fixture.py --eredrim
python3 work/support/jayluk3-2026-09-16/attachment-fixture.py --eredrim --follower
```

| Fixture | Without fallback | Stock follower fallback |
| --- | --- | --- |
| Tiel dagger | Bone index -1, socket and child at origin, assertion fails | Bone index 82, socket Z 114.89 cm, assertion passes |
| Eredrim diapason | Bone index -1, socket and child at origin, assertion fails | Bone index 91, socket Z 114.03 cm, assertion passes |

These positions are relative to an isolated reference-pose fixture, not a
measurement of the user's animated character. The fixture destroys its helper
components and leaves the player's mesh and gameplay shell unchanged. An early
fixture marshalling error was corrected and its empty component removed before
these comparisons.

The candidate `native/src/attachment_follower.inl` gives missing mesh-accessory
sockets a hidden stock-mesh follower. Unreal supplies the missing rest transform
relative to a shared animated ancestor. The helper has collision, shadows and
component ticking disabled, and uses
[leader-pose following](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/Components/USkinnedMeshComponent/SetLeaderPoseComponent?application_version=5.5)
without a separate pose tick. Audio, particles and cameras are excluded.

CSS tracks only attachments it moved, releases ones reclaimed by the game, and
returns owned children before destroying the helper. Mesh, component and stock
source changes invalidate the helper. Polling is bounded to four checks per
second with error backoff; newly applied outfits request an immediate check.
Weak references and validation after asset loading protect expired objects.

The Windows development core builds. The existing nine portable test groups
pass, but do not exercise Unreal attachment behavior. The hidden fixture tests
the engine mechanism, not the integrated C++ lifecycle.

## Integrated live results

After _eins0fx confirmed the character was somewhere safe:

- Stock Tiel's dagger passed with bone 82. Long Hair on the previous core failed
  the same check with bone -1 and visibly floated near the stairs. The new core
  passed with bone 82; the before/after screenshots show the floating dagger gone.
- Stock Eredrim and Long Hair with the correction both passed with bone 91. The
  diapason is visibly attached at the hip.
- Both weapons' native `PutInHand` and `Stow` calls passed: drawing returns the
  accessory to the game's hand socket; stowing uses the corrected stock socket.
  This is an attachment lifecycle test, not proof of every ability animation.
- Six alternating Tiel/Eredrim switches retained Long Hair and exactly one
  helper. Choosing Original removed the helper, and reselecting Long Hair worked.
- Three stock-mesh, empty-MID and active-effect recovery cycles passed. CSS
  preserved the active effect until removal. Native Inventory opened and closed
  without leaving input paused or changing the gameplay animation instance.
- Core reload removed the old helper from the actor's component list and created
  exactly one new helper. The dagger attachment still passed afterward.
- Windows developer and distribution cores build; all nine portable test groups
  pass. Portable tests do not exercise the engine attachment behavior above.

The first material test was invalid because its developer-only setup assumed a
non-null material in slot 0. Tiel has an empty slot there. The setup now selects
the first populated slot; all three cycles passed after rebuilding. No production
material-effect guard was relaxed.

Repeatable read-only attachment checks, after selecting the gameplay shell and
Long Hair in a safe session:

```sh
python3 tests/live_attachment_check.py --expect tiel --label live-tiel-after
python3 tests/live_attachment_check.py --expect diapason --label live-eredrim-after
python3 tests/live_transition_check.py
```

The first two commands require the named accessory to be present and visible;
an absent subject fails. The third requires the matching developer build and
an applied outfit. Local evidence under `work/support/jayluk3-2026-09-16/`:
`live-tiel-before.json`, `live-tiel-after.json`, `live-eredrim-stock.json`,
`live-eredrim-after.json`, `live-cycles.json`, `tiel-draw-stow.json`,
`eredrim-draw-stow.json`, `core-reload.json` and `material-recovery-live.json`.
Screenshots: `tiel-before.jpg`, `tiel-after.jpg`, `eredrim-after.jpg`.

The original shell-less corrupted Genessa state, scythe, Nail Shotgun and exact
CSS preference data were restored after testing. Loadout object paths were
compared against the pre-test snapshot. Restoration evidence is
`restored-loadout.json` and `restored-state-check.json`.

This is a CSS runtime fix. The Long Hair package is unchanged and needs no
repacking. Other outfits with the same missing-bone pattern use the same
fallback, but were not all visually retested. Combat ability timing, death/new
pawn cleanup and the user's original Lua hotkey sequence remain unverified.
This does not establish a fix for the separate portal warning or crash reports.
