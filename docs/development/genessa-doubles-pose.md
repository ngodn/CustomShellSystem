# Double pose ownership

Do not replace the native double's main skeletal mesh with an outfit using a
different skeleton. UE 5.6.1 `SkeletalMeshComponent.cpp`, `InitAnim` at line 1115,
clears the current AnimInstance when its skeleton differs from the new mesh.
`SetSkeletalMeshWithoutResettingAnimation` still reaches that check. This can
lose an active attack, even when the animation class and linked layers are
restored afterward.

Use an owned visual mesh attached to the double's native mesh. The native mesh
keeps combat animation, weapon attachments and collision. The visual mesh
copies the native pose, then runs its own authored post-process for visual
physics. Only hide native rendering once all visual components and ghost
materials are ready. Preserve and restore its hidden/visibility state and
visibility-based animation ticking, including failure and pooled reuse.

Exact-version `NeedToSpawnAnimScriptInstance`, line 867, explicitly falls back
to the component mesh's skeleton when the animation class has no target
skeleton. A single animation template can therefore supply the Copy Pose
graph without duplicating it per outfit. This does not make unrelated bone
hierarchies compatible: admission still requires a validated bone map and
reference transforms, or an original-double fallback.

`FAnimNode_CopyPoseFromMesh` uses the attached parent in PreUpdate and copies
the source pose on the game thread. Matching bones get their local source
transforms; unmatched target bones retain their reference pose. The child
then evaluates its own post-process. Register it with the parent attachment
already set, establish a tick prerequisite, and keep the native source's
bones refreshing while its rendering is hidden. Do not use a leader pose for
this primary visual component, since it needs independent post-process work.

Epic's [modular character documentation](https://dev.epicgames.com/documentation/unreal-engine/working-with-modular-characters?application_version=4.27)
describes the matching-bone restriction, parent-first ticking and separate
animation cost. API decisions above use the installed 5.6.1 source, not an
assumption that the older documentation covers every current detail.

## Evidence, October 5

Stage: `/mnt/eins0fxE/CSS-work/genessa-doubles/visuals1`.
Source: `tools/shared-assets/authoring`. Independent editor module builds with
UE 5.6.1 CL 44394996, bundled clang 18.1 and C++20. Commandlets run in a
read-only filesystem sandbox with only the stage writable.

`copy-pose-check.json` records three 60-frame cases using AN_S1_Walk:

- W3 to W3: 392 matching bones, 23,520 comparisons.
- EveG to EveW3: distinct skeletons, 388 matching and four extra wing bones,
  23,280 comparisons.
- EveG to GenessaW3: distinct skeletons, 388 matching and four extra wing
  bones, 23,280 comparisons.

With post-process disabled, maximum positional error is below 1.4e-13 cm;
rotation error is below 5.2e-8 radians. The source actually moves, rather than
passing a static reference-pose comparison. Each case retains the original
source AnimInstance and mesh. Protected source mesh/sequence hashes match.

`physics2.json` reloads the saved template and evaluates both W3 meshes with
their existing `ABP_Wings2` post-process. Extra wing bones move about 0.35
radians during each one-second fixture, with the source instance unchanged.
Hair moves as well. This proves graph execution only. The graph also changes
left-finger positions by up to 6.62 cm relative to the unprocessed input;
weapon-grip behavior needs a matched native attack check. The largest overall
position change, 31.43 cm, belongs to the ponytail, not a body joint.

Remaining: live tick order and visibility restoration, native attack/weapon
grips, captured body/physics controls, clone velocity-driven wing rates,
actual cloth collision, modular parts, pooled cleanup, Windows cook and
runtime integration. These commandlets use manually ordered components in an
editor world; they do not measure game FPS or validate ability behavior.

## Runtime visual owner

`native/src/astral_visual_mesh.inl` creates an owned skeletal component with
deferred registration, hidden rendering and no collision. It attaches before
registration, selects the shared pose template, applies morphs/hidden sections
and copies supported per-instance physics controls. It never changes the native
parent's mesh, animation, visibility or tick policy. Destruction removes only
the owned component; material bindings must be released before that step.

Both Windows configurations compile. The 17 portable regression tests pass,
but do not execute reflection against the game. The development-only
`astral.visual.probe` is prepared for that check once the shared asset is cooked
and mounted. It is not installed or live-verified. The automatic double
coordinator, material adapter selection and modular follower handling are
still pending. `AstralNativeRenderLease` now owns the native component's
render visibility, always-refresh-bones policy and update-rate flag. It
conditionally restores its own writes and does not activate disabled pooled
components. The synchronous hidden probe also exercises acquisition and
restoration, but has not run in-game yet.

## Windows cook

`work/genessa-doubles/pose-cook1` records a Windows cook using UE 5.6.1
CL 44394996 in the existing rootless Wine image. The process exited zero;
the cooker reports zero errors and warnings. The first two attempts could
not mount the secondary-drive stage because this daemon replaces `/mnt`
inside its private namespace. Neither attempt started the cooker. The actual
run used a separate CSS work directory, with the engine read-only. It did
not modify Unholy Genessa's authoring project or packages.

`/mnt/eins0fxE/CSS-work/genessa-doubles/pose-pack1` contains only the cooked
shared pose asset converted to IoStore, its Retoc verification and an
independent AssetReadback decode. The `.uexp` export payload is byte-identical
after container round trip. Four cooked exports retain the attached-parent,
curve and custom-attribute settings, with no target skeleton or reference to
the authoring module. This is an incomplete shared-assets trial, without
ghost materials, and is not installed or a release artifact.
