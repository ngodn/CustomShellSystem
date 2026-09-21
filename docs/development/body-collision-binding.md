# Saved V44 body collision binding

The isolated mesh `/Game/CSSAuthoring/DiagnosticReferences/SK_B2PhysicsBound_V1`
now references `/Game/CSSAuthoring/DiagnosticReferences/PA_B2BodyFit_V3`.
It duplicates the verified B2 game-reference mesh and changes only its Physics
Asset. Installed V43 and the shared Skeleton remain unchanged.

## Saved binding and automatic body creation

`tools/authoring-probes/hand-transfer/bind_b2_body_physics.py` creates the
single-use candidate, or verifies it after fresh loading. Its two controls are
`CSS_BODY_BIND_DIR` (a fresh direct child of `work/grip-grounding-v1`) and
`CSS_BODY_BIND_CREATE` (`1` to create, `0` to verify). UE 5.6.1 editor Python and
the project's C++20 diagnostic library are used.

The script compares all 379 raw mesh-bind bones, 30 material bindings, 22 morph
names, Skeleton, post-process class and shadow Physics Asset with the source.
It saves the new mesh, then queries a new skeletal component in a private
physics world. It deliberately makes no `SetPhysicsAsset` call. The component
must obtain its 22 valid bodies from the saved mesh reference. A second phase
recreates physics state and repeats every query.

The library's opt-in `UseMeshBinding` path is recorded in
`tools/authoring-patches/b2-body-physics-probes.patch`. The default path still
uses the previous null/apply/remove/reapply protocol. The bound path rejects
a mismatched mesh/asset pair and reports its effective asset in both phases.
It does not mutate either asset.

Evidence under `work/grip-grounding-v1`:

| Artifact | Result |
| --- | --- |
| `b2-body-binding-engine-v1` | Editor build exits 0; source-before copies retained. |
| `b2-body-binding-v1` | New saved mesh; 844 component ray observations and 304 intended-body distance observations pass. |
| `b2-body-binding-v2` | Fresh process repeats automatic creation and recreation checks; saved hash remains identical. |

Both commandlets exit 0. The mesh SHA256 is
`18689027cc7dace76135c9c682235e1f873851d94170146f2be147db62947de4`.
The Physics Asset retains the independently verified V3 SHA256
`5a6e1a7cec42781d81031f3f9c786e0e3fc483e35ef6f403cea0df1c44a8183a`.
These private-world checks use block-all query responses, not game damage or
parry filtering.

## Cook and independent decode

`b2-body-bound-cook-v2` and all pack/verify/decode stages in
`b2-body-bound-cooked-readback-v1` exit 0. The explicit package list includes the new
mesh, corrected Skeleton, body/hair/hand post-process, hand rig and Physics
Asset. The readback stages and verifies a Zen container and decodes it with
the retained mappings/exporter. No game files are installed by that pipeline.

`verify_b2_physics_cook.py` requires terminal-success records before checking
the independently decoded data. It compares the entire decoded mesh against
the prior verified cook after normalizing the new owner identity,
removing the newly added Physics Asset reference, and checking/normalizing the
file offset of an empty Nanite stream. Both stream element count and disk size
must be zero, with no bulk-data flags. No stream payload is excluded. It separately compares the
complete ActorX bytes and bind export, and requires identical Skeleton,
post-process and hand-rig JSON. The collision verifier resolves and checks
all 22 owned bodies, 21 constraints, 23 primitives, solver fields, bounds
indices and collision-exclusion pairs against the tested candidate.

## Retained cooker failure

`b2-body-bound-cook-v1` exits 1 before cooking because the custom commandlet's
asset allowlist omitted Physics Assets. This is an explicit commandlet guard,
not evidence of failed Chaos serialization. `cook-physics-assets.patch` adds
only `UPhysicsAsset` and its include. Existing package-path, output-directory,
material/shader and Blueprint checks remain in place. The editor rebuild in
`b2-body-binding-engine-v2` exits 0. The original failed output is retained;
`b2-body-bound-cook-v2` uses a fresh output directory.

## Decoded preservation result

The final verifier (`b2-body-bound-cook-v2/verify-v3.exit.json`) exits 0.
All 22 bodies, 21 constraints, 23 primitives and 212 collision-exclusion pairs
survive with the tested properties. The primitives are 18 capsules, four boxes
and one equal-radius tapered capsule. The mesh's cooked Physics Asset reference
resolves to V3. Complete ActorX geometry/skin/material/morph bytes and mesh bind
are identical to the earlier verified B2 cook. Corrected Skeleton, nine virtual
bones, full post-process defaults/wiring (including accepted hair 200/24) and
hand-rig JSON remain identical.

Two verifier failures are retained rather than hidden:

- Initial full mesh comparison found only the empty Nanite stream offset moving
  from `0x2717D7C` to `0x2717D99`. The added property changes package layout;
  the stream contains zero bytes. `mesh-differences.json` records this single
  difference. The verifier now requires empty data before normalizing its offset.
- The next comparison found qualified versus unqualified enum spelling.
  `physics-differences.json` contains 82 such differences and no others:
  `ECollisionTraceFlag::CTF_UseSimpleAsComplex` and
  `EAngularConstraintMotion::ACM_Limited` appear without their enum type prefix
  in this decoder. Comparison accepts the exact enum value with that prefix
  removed. Numeric property checks were not relaxed.

No assets were edited or recooked to pass either verifier correction.

## Remaining acceptance

Cooked preservation is not cooked execution. Required follow-up remains the
actual game's character selection/recreation, damage/parry, weapon and sidearm
poses, death/recovery, travel and performance. The enlarged fixed hand boxes
must be checked for unwanted contacts. Grounding and heel geometry defects,
preview light controls and the complete modular Next-Gen runtime/UI/profile
and distribution requirements remain active. Preserve source proportions,
379 authored bones plus nine virtual bones, accepted hair 200/24 and body
motion, and disabled CSSX. V43 remains the installed fallback.

## Trial promotion

The [V44 trial](v44-live-trial.md) now installs this verified binding and the
exact native compatibility update. V43 is retained as the rollback copy.
Startup is verified; actual gameplay acceptance remains pending.
