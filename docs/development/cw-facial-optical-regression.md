# Commander White facial eyes and doubles regression, 7 October 2026

The user confirmed that Iris Out animated Commander White's face but hid her eyes.
Walking did not restore them. Selecting Eve Default Idle did restore them.

The original C4 mesh and facial E8 mesh had identical material bindings. Slots 8,
10 and 45 use the game's `M_refraction_01`, which lacks morph-target usage.
UE 5.6.1 `FSkeletalMeshSceneProxy::UpdateMorphMaterialUsage_GameThread` replaces
unsupported morph materials with the default material. Stopping curve updates
does not recreate that scene proxy. The engine behavior explains the reported
opaque optical surfaces and recovery after changing the mesh.

The private shared animation pack now contains facial mesh E9 and four optical
assets under `/Game/CSS/Locomotion/Faces/Optical1`. The new translucent material
supports skeletal meshes, morph targets and clothing. Its refraction and
reflection arithmetic follows the independently decoded native shader. The three
instances retain the original opacity, roughness, IOR and reflection intensity.
The original Commander White outfit package is unchanged.

The authoring cubemap and lighting collection are compile-time stand-ins. They
are excluded from the package. The compiled material references the game's
original cubemap and preserves the lighting collection's parameter identities
and StateId. Do not ship these stand-ins or replace the game's collection.

`generate-adapter-catalog.py --include-locomotion-optical` includes this cooked
material's exact StateId and static defaults. It reuses the existing Astral eye
refraction companion. Unknown material graphs still follow the existing fallback.
The catalog has 13 supported roots instead of 12.

A separate doubles regression came from development builds starting with
`astral_trial_enabled` false despite both saved MISC choices being `css`.
Commit `6d74a49` makes diagnostic builds honor the saved choices on startup.
The explicit diagnostic suspend request remains available. This does not change
user settings or require new outfit packages.

## Validation

- Independent cooked E8/E9 comparison preserved all 206 morphs, geometry,
  skeleton, physics, postprocess bindings and 67 other material slots.
- Windows SM5 and SM6 shader maps include skin and cloth surface/distortion
  shaders, with 27 and 33 embedded shader hashes respectively.
- All 103 export payloads survived the IoStore roundtrip unchanged.
- Installed catalog comparison permits only the E8 to E9 facial adapter change
  and container hashes. All external facial resources match byte for byte.
- CSS Windows build, UE 5.6.1 editor helper build and FacialReadback build passed.
  Nine relevant runtime tests and 12 installer rollback tests passed.

Evidence: `work/cine-animation-port/cw-optical-fix-v4/final-validation.json`,
`cooked-mesh-preservation.json`, `optical-stage.json`, build logs, and
`work/cine-animation-port/astral-diagnostic-regression/before.json`.
Pack: `/mnt/eins0fxE/CSS-authoring-scratch/cw-optical-trial-v1`.

Live acceptance still requires CW Iris Out eyes, interruption/default-idle
restoration, and Faithful/Stray Use CSS summons. The After Brute Battle loop-end
crash is a separate unresolved investigation. This is not a release acceptance.
