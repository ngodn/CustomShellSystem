# Genessa wing skeleton compatibility

2026-10-03. Unholy Genessa trial.4 introduces the private skeleton
`/Game/CSS/UnholyGenessa/SKEL_Wings2.SKEL_Wings2` for four animated wing hinges.
CSS rejected its appearance with `Different skeleton: appearance change refused`
because this path was absent from `compatible_standard_skeletons`.

Admit this exact audited path in both directions with the existing human, CSS
base, B2 and short-path rigs. Unknown skeletons, lookalike paths and the rejected
Wings1 trial remain refused. This is a maintained asset allowlist, not a runtime
structural validator for arbitrary custom skeletons. The change runs during
appearance compatibility checks and adds no tick work.

## Asset evidence

The audit belongs to sibling project
`../CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/authoring/`:

- `wing-gait-motion.md`: shared skeleton's first 386 raw bones preserved, four
  wing hinges appended under spine_05, 82 sockets and nine virtual definitions
  retained. W3 meshes use this private skeleton and ABP_Wings2.
- `wing-hinge-check.json`: both skins retain body collisions, geometry, all
  22 morph payloads, cloth geometry and simulation bindings. Only the four
  intended wing weight names change.
- `wing-skeleton-pose-check.json`: native UE authoring evaluation of idle,
  walk, jog and sprint on both skins retains original body and virtual-bone
  local poses, maximum component difference below 3.9e-15.
- `wing-driver-check.json`: the compiled post-process graph retains body poses
  while driving the four wing hinges. Synthetic speed checks, cloth disabled.
- `wing-cloth-check.json`: separate paired cloth evaluation against the old rig.
- `wing-idle-package-check.json`: cooked trial.4 assets and dependencies checked,
  including the retained 19 corrected movement payloads and seven custom idles.

These checks do not establish in-game visual or performance acceptance. The
installed game was still using its original Genessa mesh after the refusal.
Confirm W3 and ABP_Wings2 actually load after installing the matching CSS core,
then test appearance restoration, gait transitions, cloth and the seven idles.

## Regression

The added `css_skeleton_compatibility` cases reproduce the failure against the
old header (`Audited Genessa wing rig rejected`). They cover the exact new path,
restoring each older audited rig, unknown paths in both directions and identical
skeleton handling. Run with:

```sh
cmake --build build/release-host --target css_skeleton_tests -j 4
ctest --test-dir build/release-host -R '^css_skeleton_compatibility$' --output-on-failure
```

The focused skeleton, animation catalog and animation runtime tests all pass
after the change. The Windows Release development core builds successfully.
Installation and game acceptance are pending.

## First live load and movement follow-up

The `0cae679` development core was installed after a normal shutdown. A fresh
process maps that DLL and loads SK_EveW3 with ABP_Wings2. The user confirms the
appearance works. Evidence is in the mod's `authoring/wing-css-live-check.json`
and `wing-css-install-check.json`.

Custom movement then reports `Custom movement requires a 2D BlendSpace on this
mesh's skeleton`. `WalkOverride::custom_blendspace` separately required pointer
identity for the BlendSpace skeleton and every sample sequence skeleton. Those
assets intentionally retain SKEL_Base; changing only the appearance gate did
not cover locomotion.

Allow the specific directed SKEL_Base-to-SKEL_Wings2 animation remap in both
validation sites. Wings2 already lists SKEL_Base as compatible, and the native
remapping/pose audit preserves the original bones. Same-skeleton playback keeps
its existing behavior. Do not extend the appearance allowlist wholesale to
animation playback: other cross-skeleton combinations and the reverse wing
remap remain rejected. BlendSpace class, axes, bounded samples, additive mode,
root motion and rate checks remain intact. This validation is cached per gait
and invalidated when the target skeleton changes.

The new directed-animation regression fails under the old identity policy and
passes with this exception. The focused animation catalog/runtime regressions
also pass. The second DLL still needs installation and actual custom gait
playback verification; the first live appearance check does not prove that.
