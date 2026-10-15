# Boss appearance adaptations

Current scope is every straightforward boss adaptation for v1.0.0. The user
explicitly deferred Zhirelle and similarly substantial repairs to v1.0.1.
These additions and the remaining reported defects precede release preparation.

## Permanent shared assets installed

The four accepted adaptations now live under `/Game/CSS/eins0fx/Bosses/` in
`CSS_SharedAssets_P`. Their selection IDs and scale factors are unchanged.
`packaging/boss-appearances.json` owns the native, trial and permanent path map
and drives the NPC roster generator. The exact Skeleton allowlist uses these
permanent paths; CSS facial animations remain excluded for these rigs.

Recipe: `tools/shared-assets/add-boss-appearances.py`. Candidate and evidence:
`build/shared-boss-appearances1/`. It starts from the retained accepted shared
pack and boss trial, relocates only package references, and adds eight packages.
All 283 existing shared packages roundtrip byte-identically. Boss export/bulk
payloads match their staged inputs, and all eight complete decoded assets match
the accepted trial after normalizing paths. The Skeleton headers are reconstructed
by IoStore and lose the editor-only IsAsset flag; their package/export identities
and complete decoded contents are checked instead of requiring identical headers.
The shared catalog and facial resources are unchanged apart from container hashes
and sizes. All new imports resolve. The unused Orrem trial assets are excluded.

The Windows C++23 core build, data and Skeleton host suites, and 1,545 current
outfit/animation library checks pass. `host-validation.json` records the commands
and binary hashes. The first catalog fixture used directory symlinks, which package
discovery does not traverse. Real fixture directories with file symlinks pass.

`installation/deployment.json` records a normal quit, verified shared/core/catalog
installation, unchanged CSS state and a normal Steam restart. The active core is
`css_core-development-dc0a609e74750815-boss-permanent.dll`. The former shared and
boss-trial containers are retained outside mounted Paks at
`MortalShell2/Content/CSS-boss-permanent-backup1` for rollback.

`live/result.json` verifies all four permanent mesh/Skeleton bindings, native
player animation graph, expected body scale, unchanged capsule and all nine
equipped weapon scales. The user's Obsidian Eredrim appearance, customization,
scales, position and camera restore after selection checks. This follows the
normal restart; the restart itself naturally loads the game's saved location.
Walking was already checked on the content-identical trial, so it was not repeated.

Missing boss assets now produce an install/update CSS Shared Assets message.
No release tag, public ZIP or upload was produced. The coordinated release still
needs its final metadata/version pins and the remaining goal work.
Duplicate stage/readback directories, 195,635,580 logical bytes, were removed
after verification. Sources, package, recipe, metadata, receipts and rollback
remain. Root was 96 GiB free and theark 48 GiB before and after that cleanup at
`df -h` precision. See `docs/development/cleanup-2026-10-15-boss-assets.md`.

## Isolated rigs and live trial

Tarsus, Droeg, the Nameless Captive, Sir Isaac and Orrem's Discarded form have
coherent offline player-animation samples after translation retargeting.
Their trial package cooks and passes independent container/import readback.
The five were installed with development core `130c238d0bc23453` and loaded
through a normal game restart. `isolated-live2/result.json` verifies selection,
player animation, restoration to Peony and unchanged position/view. The first
post-load attempt stopped before selecting a boss when the view changed.

The live images show four intact bodies. Droeg and the Nameless Captive retained
giant native dimensions in that first trial. The user subsequently chose roughly
player-sized appearances for the bosses.
Orrem's material-bearing skeletal mesh renders only a few pieces. His native
Blueprint supplies fourteen additional Nanite static body parts. This is a
multipart reconstruction, not just retargeting, and is deferred to v1.0.1.
The source and installed roster and allowlist now exclude him.

`scale-deployment1/deployment.json` records development core `41eeb3897e4b7785`.
`player-size-live1/result.json` verifies all four scale factors, unchanged capsule,
player animation graph, and restoration of Peony's scale and customization.
All four resulting screenshots were inspected. The recipe used no gameplay input;
player location and camera were unchanged. The native leg lengths set these
initial factors against an 88 cm thigh-plus-calf target, preserving proportions:

| Appearance | Uniform scale |
| --- | --- |
| Tarsus | 0.7661 |
| Droeg | 0.3349 |
| The Nameless Captive | 0.4283 |
| Sir Isaac | 0.6523 |

The same capture exposed a real side effect: equipped weapons inherited the body
scale. Development core `4852c5317c30db5d` corrects it and is installed through a
normal restart, recorded in `weapon-scale-deployment1/deployment.json`.
`player-size-live2/result.json` verifies all four boss factors, native world scale
for all nine equipped weapon components, unchanged capsule, and exact restoration
of Peony's component/weapon scales and customization. Player position and view
stay unchanged during that test.

`player-size-preview1/result.json` and its inspected screenshot verify Droeg's
0.3349 scale in the visible CSS preview. Switching back to Peony while that same
preview actor remains alive restores scale 1 and Peony's mesh. The menu closes
and original customization restores. This is a representative preview check,
not a claim that every boss's preview was individually captured.

Weapon correction uses the equipped actor roots, including shell tools and body
weapons, only while they inherit scale from the current appearance component.
It preserves nonuniform native relative scale, handles native reattachments,
and restores only values still matching CSS's write. Absolute-scale and detached
components do not receive compensation. Other appearance cosmetics keep normal
scale inheritance. The C++23 Windows build and the data and skeleton host suites
pass; tests include repeated compensation, boss changes, native resets, restoration,
invalid factors and external scale changes.

The local UE 5.6.1 `SceneComponent.cpp` confirms that `SetRelativeScale3D` updates
the component transform and overlaps. The correction uses that setter, rather
than direct field writes. Epic's [scene-component reference](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USceneComponent)
documents relative versus absolute transform behavior.

The short native walking check now passes for all four bosses and Merrick.
`movement-summary.json` points to the individual observations in `movement3`
and `movement4`: each appearance walks forward and back for 450 ms per direction,
with valid moving poses and no held input after completion. Captured walking
frames show intact bodies. The user's original Obsidian Eredrim appearance and
customization restore. The final three-case pass ends within 2.3 cm of its start.
This checks short walking, not every combat animation or garment contact.

Interrupted attempts remain recorded. `movement1` stopped before input when
the player was moving elsewhere. `movement2` stopped after Tarsus when the view
changed substantially. After the user confirmed the game was idle, `movement3`
completed Tarsus and Droeg but stopped on a 0.164-degree yaw change. The remaining
three cases passed in `movement4` with a 0.5-degree camera tolerance, which changes
the one-metre walking direction by less than one centimetre. No completed walking
case was repeated after that adjustment. All attempts restored the appearance.

These movement captures used the earlier trial container. The permanent
shared-pack integration above supersedes that installation.

The first raw-native trial selected and restored all seven candidate meshes,
but most collapsed under player animation. The native Skeletons generally use
Animation translations, which replace their longer bind offsets with the
player's offsets. Merely allowing those Skeletons in CSS was insufficient.

The corrected assets use separate `/Game/CSS/Bosses/` packages. Root and IK
translations use Animation, pelvis uses AnimationScaled, and remaining bones
use Skeleton. Original bind transforms and sockets are retained. No original
game package is replaced. The native mesh payloads retain geometry, LODs,
materials, cloth, physics and post-process references. Only package names and
the Skeleton reference change. CSS facial animations remain disallowed.

The cook initially rejected the temporary seed mesh's editor preview reference.
`CSSNpcLibrary.ClearBossPreview` clears it only on isolated boss Skeletons in a
commandlet. The subsequent cook succeeds without an authoring dependency.

Evidence and original executed recipes are under `work/npc-appearances/bosses/`.
Unmodified copies of the four authoring/package recipes now live in
[`tools/authoring/bosses`](../../tools/authoring/bosses/README.md), with source
hashes and the distinction between the five-rig trial and four accepted bosses.
The editor helper is also tracked and matches the actual authoring project's
source. No asset rebuild or installation is part of this source preservation.

Retained inputs and receipts:

- `skeletons.json`, `exports.json` and each `SK_*/source-mesh.json`: native inputs.
- `trial1/live/result.json`: raw-native trial, including restoration. Its
  completed flag means capture completed, not visual acceptance.
- `selected-plan.json`, seed recipes and `configure-isolated.py`: five selected
  rigs, native metadata and retained H2 animation evaluation.
- `*-corrected-motion.json` and `*-corrected-renders/`: sampled deformation.
- `clear-preview.py`, `cook-command.json`, `cook3.log`: successful cook.
- `package.py` and `verify.py`: reversible path relocation, container build,
  dependency resolution, full decoded mesh comparison and bind/mode checks.
- `isolated-package1/verified.json`: all five pass those offline checks.

Sir Isaac's new Skeleton export index differs from the native package. The
comparison resolves the same Skeleton object name at its isolated package path
before excluding that numeric export index. Every other decoded mesh field
matches. Byte edits also reconstruct the original payload exactly when undone.

## Deferred to v1.0.1

| Boss | Reason |
| --- | --- |
| Magdalena | Seated reference pose and garment placement remain unsuitable after simple translation retargeting. |
| Orrem, the Reclaimed | The inspected mesh is only part of the body; native assembly must be reconstructed. |
| Orrem, the Discarded and Headless forms | The skeletal mesh alone renders only ornaments. Native Blueprints assemble fourteen Nanite body parts; both require multipart reconstruction. |
| The Lost Child | Lacks complete human leg, left-arm and spine chains; root/pelvis hierarchy differs. |
| Zmey | Asymmetric limbs and extra tail/neck chains require a dedicated mapping. |
| Hexapod | Tentacles do not provide the human locomotion chains. |
| The Monolith | Nineteen-bone rig lacks the human limb, head and spine chains. |

Zhirelle remains deferred separately. Offspring and the existing human-skeleton
miniboss variants already have roster entries. `headless-packages.json` and
`golem-setup.json` establish the shared multipart Orrem construction. Detached arms
and hidden/test mesh variants are not selectable appearances.
