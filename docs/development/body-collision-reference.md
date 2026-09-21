# V44 body collision reference

Fresh live reads confirm that installed V43 has no mesh Physics Asset and no
component Physics Asset override. This removes the earlier uncertainty about
a runtime override. It does not yet establish that the missing asset explains
every reported damage/parry failure. No collision state, player movement,
character selection or installed package was changed during this investigation.

## Live evidence and remaining hypotheses

`combat-collision-live-v2` resolves the current gameplay pawn and its actual
`SpartaSkeletalMeshComponent`, enumerates available properties and reads the
references. Both `SkeletalMesh` and `SkinnedAsset` identify installed
`SK_SeduXtress_HandBindV43`. `PhysicsAssetOverride`, mesh `PhysicsAsset` and
`ShadowPhysicsAsset` are null. `bCanBeDamaged` is true. The player identity
and revision remain stable through the read. The reusable tool repeats this
in `combat-collision-live-v3`, with repeated mesh handles deduplicated.

The pinned UE 5.6.1 implementation of
`USkinnedMeshComponent::GetPhysicsAsset`, in
`Engine/Source/Runtime/Engine/Private/Components/SkinnedMeshComponent.cpp`,
returns the override if set, otherwise the mesh's Physics Asset, otherwise
null. These readings therefore establish the absence of an effective skeletal
Physics Asset. The failed earlier `GetPhysicsAsset` reflected-function query
should not be repeated: the engine getter is not a UFUNCTION. Read the actual
reflected references instead. Epic's [Set Physics Asset documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/Components/SkinnedMesh/SetPhysicsAsset)
also describes this override relationship.

The next tests distinguish these explanations:

1. Missing skeletal collision shapes cause body traces to miss. An otherwise
   identical component with a fitted asset should register the same bounded
   traces that the null-asset control misses.
2. Collision channels or responses prevent contacts despite valid shapes.
   If geometry queries pass but the relevant channel trace fails, inspect
   the component's game collision filtering before touching combat state.
3. Damage or parry state rejects valid contacts. Only investigate that path
   as the cause after proving contact reaches the player; `bCanBeDamaged=true`
   alone does not prove every game-specific filter permits a hit.

The previous live observation found `QueryAndPhysics` and a `Custom` profile
on mesh/capsule. Those are historical readings, not a substitute for the next
contact test. The player capsule is separate from the missing body asset.

## Game reference and selected template

The independently decoded original Genessa mesh and MoreBeaute mesh both
reference
`/Game/Sparta/Characters/NPCs/SesterGenessa/Art/Mesh/PA_Sester_Genessa_V5_Capsules_NoCloth`.
The MoreBeaute mesh was not already loaded during this live session:
`combat-physics-reference-v1` records a null exact-path lookup and stops.
No actor switch or asset load was performed to obtain that control. The
reference comparison uses the retained decoded packages, not a claim that
MoreBeaute damage/parry was tested live.

`combat-physics-decode-v1` extracts the referenced Physics Asset from the game
containers. It has 60 skeletal bodies and 59 constraints. Despite its name,
38 bodies belong to Genessa cloth chains, including 25 explicitly simulated
bodies. Binding it wholesale is not yet an accepted replacement for CSS's
existing hair/body dynamics.

`prepare_body_physics_reference.py` writes an isolated, **unfitted** template
in `b2-body-physics-reference-v1`:

- 22 human body entries, all present in the verified 379-bone B2 mesh.
- 21 constraints whose two body names are retained.
- 18 capsules, four boxes and one tapered capsule.
- 212 retained collision-disable pairs, remapped from the original ordered
  body array to the selected array, with original values preserved.
- Original body/constraint properties, bounds membership and solver settings.
- All 38 cloth bodies and their 38 constraints omitted. None of the selected
  bodies explicitly requests simulation; component/ragdoll behavior still
  requires verification.

The selector resolves owned exports by name and follows the Physics Asset's
ordered body/constraint lists. JSON export order is not body index order.
Every removed body must match the known Genessa cloth family; arbitrary
unknown names cannot silently disappear. The source and B2 bind hashes are
recorded. All selected source properties remain unchanged.

An initial check rejected `spine_04` because its capsule cylinder length is
zero. This is valid with a positive radius: `FKSphylElem::Length` excludes
the round ends, as documented in the pinned engine's
`Classes/PhysicsEngine/SphylElem.h`. The selector now allows zero cylinder
length while requiring positive radii/box dimensions. The failed script and
reason are retained; the game geometry was not changed to satisfy the check.

The independent exporter writes a Physics Asset JSON but an empty mesh-export
results list for this non-mesh class. Acceptance checks the actual typed asset,
60 body exports, 59 constraints and their references rather than treating an
empty list as a successful mesh export.

## Next implementation

Fit collider centers, orientation, dimensions and constraint frames to B2's
preserved anatomy. Inspect the collider/body overlay before building a saved
Physics Asset. Keep original proportions and the accepted hair 200/24/body
motion; this work adds collision data, not a replacement animation system.
Then test actual physics queries with null-asset and fitted-asset components,
including ragdoll/collision lifecycle and public morph extremes, and verify
damage/parry in the game. Do not mark the user-visible defect solved from
reference presence, a selected JSON template or an editor-only ray test.

`tools/inspect_collision_references.py --output <fresh-grip-work-directory>`
performs bounded read-only requests through the CSS core. It requires a
responding game, validates property names, records each request/response and
uses fresh player handles. It neither enables CSSX nor reloads a DLL. The
raw logs remain workspace-local; runtime request writes use the user's
existing authorization.

The complete Next-Gen goal remains active: weapon/default-animation coverage,
modular physics and morphs, native controls, UI/profile/reset/lifecycle behavior,
performance and distribution. V43 remains installed; the cooked V44 reference
candidate still has a null Physics Asset and is not ready to replace it.

## Fitted geometry and actual component queries

`fit_body_physics_reference.py` now produces an isolated neutral collider
candidate from the verified B2 mesh export. It assigns each skin vertex to the
largest summed weight among the 22 human bodies, folding helper/finger weights
into their nearest retained ancestor. The two pelvis primitives partition their
assigned points by original signed distance. Each shape retains its original
orientation, fits the assigned cloud with a 0.35 cm margin, and checks enclosure.
Constraint frame 2 moves to frame 1's world anchor and preserves the source
joint's relative rest-frame rotation. Limits, drives and other settings remain
unchanged. Maximum anchor separation is 0.000015572 cm.

The initial fitter failed before fitting because it assumed more than 30,000
main skin points. The pinned export actually has 19,105. A second run found
zero foot points because feet are separate modular sections. Slots 23 through
26 contain covered and footwear skin/nails, as defined in `wardrobe_regions.py`.
Including both variants yields 24,480 points. These are a union envelope, not
evidence that both foot variants should be visible together. The failed scripts
and logs remain in `b2-body-physics-fit-launch-v1` and `-v2`; the corrected
export hash, material identities and audited topology counts are checked.

`b2-body-physics-fit-v3` passes 23 shape checks and 21 anchor checks.
`b2-body-physics-fit-v4` adds ray fixtures and produces a byte-identical candidate.
Of the assigned points, 11,319 lie outside their original assigned shape; that
is not a union-of-all-original-shapes coverage count. All assigned points are
inside their fitted shape. This enclosure test is restricted to neutral skin.

`render_body_physics_fit.py` renders both templates against the unchanged
export in front, side and rear views. All six images in
`b2-body-physics-render-v1` were reviewed. Original torso/limb shapes are offset
or undersized. Fitted shapes cover the sample, but the pelvis, calves and foot
boxes are broad. The second pelvis shape receives only 14 points. This is a
query-test candidate, not an accepted final ragdoll or gameplay collider layout.
The renderer shows both foot variants, which explains their overlapping surface.

The C++20 / UE 5.6.1 editor patch
[b2-body-physics-probes.patch](../../tools/authoring-patches/b2-body-physics-probes.patch)
adds commandlet-only authoring, inspection and actual physics queries. It builds
successfully in `b2-body-physics-engine-v1`, after the reference-metadata patch.
It creates a fresh diagnostic `UPhysicsAsset`, imports the retained body/joint
properties, constructs physics meshes, restores 212 collision-disable pairs,
and updates body/bounds indexes. Every supplied property is checked against an
engine export, including nested fields, rather than trusting converter success.

The pinned `PhysicsConstraintTemplate.cpp` serializer saves `DefaultProfile`
instead of a temporarily edited `DefaultInstance.ProfileInstance`. The helper
calls `UpdateProfileInstance()` before saving. Omitting this would risk losing
the imported game limits/drives during save or cook. This is a save-path
requirement identified in source, not a claim about an earlier game crash.

Saved asset: `/Game/CSSAuthoring/DiagnosticReferences/PA_B2BodyFit_V1`.
`probe_body_physics_candidate.py` first saves it, then a fresh-process invocation
loads it without editing. The probe uses a temporary skeletal mesh component
in its own physics world, forced reference pose, post-process disabled, and
explicit `QueryAndPhysics` / block responses. It does not bind or save the
source mesh, invoke the game, or change the accepted motion settings.

The 270 rays contain 69 primitive-axis rays, 132 rays through regional skin
extrema, and 69 off-body controls. Four phases apply null, fitted, null and
fitted assets. `b2-body-physics-query-v1` passes all 1,080 observations:

- Null/removal: zero bodies and zero ray hits.
- Fitted/reapplied: 22 valid bodies, all 201 intended hits, all 69 controls miss.
- Cleanup destroys the temporary component/owner/world; protected source
  assets remain byte-identical.

The query calls `USkeletalMeshComponent::LineTraceComponent`, whose pinned
implementation tests its actual `FBodyInstance` entries. It is not an analytic
Python intersection surrogate. Epic's [FBodyInstance reference](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FBodyInstance)
describes that per-body ray API; the local 5.6.1 source fixes the exact version.
`b2-body-physics-query-v2` loads the saved asset in a fresh process and repeats
all 1,080 checks successfully. Every supplied body/constraint/solver field and
all collision-disable pairs survive saving. Both query processes exit zero;
the fitted asset hash is identical before and after fresh-load testing.
No assets are saved during the second run.

Remaining acceptance: refine the broad shapes, check public morph extremes,
posed/moving skin, simulation and ragdoll, collision channels, full cooked
binding and live damage/parry. The test's block-all query setup does not prove
the game's channel filtering or damage logic. V43 remains installed; V44's
new Physics Asset remains isolated and uninstalled. The complete modular
Next-Gen asset/runtime/UI/profile/lifecycle/performance objective is unchanged.
