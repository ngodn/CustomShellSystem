# V44 trial installation and live acceptance

V44 is the user-accepted baseline as of 2026-09-21. After testing, the user said:
"ok it's better now, we lock this, document this, and we move on".
The active hand-tuning iteration is closed. Next work is grounding and missing
heel supports. CSSX remains disabled.

## Accepted baseline and verified world entry

[The baseline manifest](v44-accepted-baseline.json) pins the installed package
trio and core by SHA-256, freshly checked after acceptance. Preserve the hand
graph, original body proportions, accepted body motion and hair 200/24/0.
Isolate footwear changes. The locally compiled lighting cores are not the
installed `css_core-v44-f434601d670b1502.dll`.

The earlier black screen was the startup-video widget in `L_MainMenu`.
`op=player` resolves the gameplay controller/pawn; its null result did not mean
no main-menu controller existed. Actual viewport/controller and top-level
widget queries identified the state. Two verified normal Skip/Confirm actions,
then Confirm on the selected, enabled Continue button, entered
`L_Core_World_01` without restarting or sending gameplay input. Evidence:
`work/grip-grounding-v1/v44-startup-inspect-v1`. Reuse this measured path rather
than repeating pawn-only polling or diagnosing a crash from a black still.

In `work/grip-grounding-v1/v44-trial-live-v1`, both
`binding-after-continue.json` and `binding-geometry.json` pass without failures:
actual mesh `SK_B2PhysicsBound_V1`, Skeleton
`SKEL_B2GameReferenceMetadata_V2`, Physics Asset `PA_B2BodyFit_V3`, no override,
and the B2 hand post-process with enabled raw-game input correction. Hair is
200/24/0. The mesh reports `PlayerMesh` and `QueryAndProbe`; the pawn can be
damaged. All three exterior asset/pelvis-body distance samples are positive.
These geometry checks do not prove damage filtering, overlap events or parry.

The reviewed Steam still `world-after-continue.jpg` shows the loaded world.
`world-user-test-1.mp4` records 30 seconds without injected input. Six reviewed
frames at five-second intervals show heavy-weapon idle and a small view change;
they do not establish attack or enemy-contact coverage. User acceptance is
separate from these bounded observations. The earlier full weapon snapshot
includes an Axatana attack montage, so do not label it Martyr's Blade idle.
Full weapon/sidearm coverage, damage/parry, death/travel and performance remain
release checks. Do not reopen accepted hand tuning without new evidence.

## Prepared package

`tools/prepare_v44_trial.py` assembles the five verified packages from
`work/grip-grounding-v1/b2-body-bound-cook-v2` with the V43 cooked textures,
materials and control resources. The previous main mesh is replaced in the
explicit package list by `SK_B2PhysicsBound_V1`. Other existing cooked packages
are retained. Original material payload hashes are checked before copying.

The candidate is under the SeduXtress authoring mod's
`work/v44-trial-v1/candidate`, with exactly one matching pak/utoc/ucas trio.
The existing packager verifies the container, manifest, resource hashes and
customization recipes. Its shared staging directory is not reused.

The catalog keeps the outfit and variant IDs and all body controls. It points
the variant at the new mesh. Hair defaults in both the catalog and embedded
recipe are updated from 150/18 to the user's accepted 200/24, matching the
verified animation defaults. Existing saved overrides are retained.

`work/grip-grounding-v1/v44-final-package-readback-v1` independently decodes the
**final combined container**. All five new package JSON exports exactly match
the independently verified collision cook, including the mesh's Physics Asset
reference. The preparation and final decode both exit 0.

## Exact runtime compatibility

The old native allowlist only accepted the game human/CSS base pair. It would
reject the separate B2 reference Skeleton despite the retained engine tests.
The regression first fails with `Audited V44 B2 target rejected`.

`skeleton_compatibility.hpp` now permits directional transitions among the
exact audited human, CSS base and B2 paths. The B2 path is:

`/Game/CSSAuthoring/DiagnosticReferences/SKEL_B2GameReferenceMetadata_V2.SKEL_B2GameReferenceMetadata_V2`

Unknown paths, similarly named rigs in other folders, missing paths and invalid
suffixes remain rejected. This is not generic structural validation or a shared
Skeleton migration. The host regression and C++23 Windows core build pass in
`work/grip-grounding-v1/v44-core-build-v1`. The pre-build DLL hash matched the
installed preview-layer core, so this rebuild starts from that known binary.

## Deployment and startup evidence

`tools/deploy_v44_trial.py` requires the verified candidate, final decode,
passing build/test results, exactly one game process, the pinned UE4SS hash,
the exact V43 baseline trio and disabled CSSX. It backs up the outfit, old core
and selector, and CSS state. It invokes the reflected normal `QuitGame`, waits
for the process to exit, then replaces the trio and selects the new core.
Partial file-copy failure restores the old trio and selector. It never writes
player movement input or changes save files directly.

Evidence and rollback files are in
`work/grip-grounding-v1/v44-trial-live-v1`:

- `deployment.json`: exact installed hashes, old/new core selectors and state
  hashes. The CSS state after normal quit is unchanged by installation.
- `backup/outfit`: original V43 trio.
- `backup/core.json` and the prior core DLL: original core selection and binary.
- `backup/state-before-quit` and `backup/state-after-quit`: retained CSS state.
- `startup.json`: fresh Linux process 1127949, distinct from old process
  2384672; loader reports `css_core-v44-f434601d670b1502.dll`.
- `startup.requests.jsonl`: fresh acknowledged player query after restart.
  Controller and pawn are null, so it is not gameplay validation.
- `startup.jpg`: reviewed Steam screenshot is black before character load.
  It does not establish that the title menu or world has rendered correctly.
- `settled-startup.json` / `settled-startup.jpg`: a later fresh query still has
  no controller or pawn, and the Steam capture is still black. The process
  remains responsive; do not infer a rendered menu or diagnose a crash from it.

The launcher exits 0 and the new process answers requests. Runtime catalog
errors are empty. These initial observations prove startup/core activation only.
The request to Continue was subsequently resolved through verified
normal menu actions as recorded above. No gameplay movement input was sent.

To roll back, first close the game normally and confirm no shipping process
remains. Copy the backed-up outfit trio over the installed trio and restore
`backup/core.json` to the runtime selector. The old DLL remains in `cores`.
Do not restore saved CSS state unless needed and explicitly intended; installing
this trial did not modify it. Relaunch through Steam and verify the live mesh.

## Next live checks

`tools/check_v44_live_binding.py --output work/<fresh-name>.json` now provides
a read-only first check. It requires one live game process and a fresh output,
enumerates properties before reads, describes functions before calls, checks
parameter sizes against the pinned UE 5.6.1 declarations, and rechecks player
and mesh identity after sequential reads. It verifies the exact mesh, Skeleton,
Physics Asset, no component override, actual post-process class, hair 200/24/0
and enabled hand correction using the raw-game convention. Differences in
saved hair overrides are reported, never reset by the check.

Exit 2 means `waiting_for_character`, exit 1 means an error or failed binding,
and exit 0 means only `binding_verified`. `gameplay_accepted` remains false.
The historical run `v44-trial-live-v1/binding-check-1.json` exited 2: process
1127949 answers, but controller and pawn remain null. No mesh or physics query
was executed. Replaying the previously recorded V43 references through the
same inspector correctly rejects them before any function invocation; evidence
is `binding-old-v43-negative.json`. This historical check covered rejection.
The success path now passes
in the world-entry evidence above.

Optional `--geometry` reads three exterior samples around the pelvis through
the asset closest-point function and the component's pelvis-body distance
function. This branch now passes in `binding-geometry.json`. The pinned engine
header
and [Epic's API description](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/USkeletalMeshComponent/K2_GetClosestPointOnPhysicsAsset)
explicitly distinguish the asset query from collision state: it uses animation
geometry independently of collision and welding. Even a positive component
distance does not establish the game's damage channels, overlap events or
parry behavior. Do not promote either measurement into gameplay acceptance.

1. Fresh player/mesh probe must resolve `SK_B2PhysicsBound_V1`, the B2 reference
   Skeleton and `PA_B2BodyFit_V3`, with no unexpected component override.
2. Check actual body/hair/hand post-process, accepted 200/24 and preserved public
   customization. Review Steam stills and game-window motion footage.
3. Test Martyr's Blade, scythe, sidearm and the remaining weapon/posture matrix,
   including transitions and the previously tolerable Axe & Dagger/Axatana.
4. Test enemy hit/damage and parry behavior. Asset presence is not causality.
5. Verify death/recovery, travel, menu recreation, profiles/reset and performance.

Grounding/heel geometry, preview light controls, modular/fantasy-chain coverage,
full native/UI/build/distribution acceptance and the complete CSS Next-Gen goal
remain active. Original body proportions and source blends remain preserved.
