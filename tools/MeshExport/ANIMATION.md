# Animation diagnostic exports

The tool targets .NET 10. The verified build used SDK 10.0.401 and the matching
CUE4Parse native ACL decoder. These modes read source assets; they do not repair
or reproduce the complete gameplay AnimGraph.

## Cooked morph correspondence

`CSS_MORPH_AUDIT=1` reads a separate skeletal mesh with one render LOD. It writes
original render positions and primary UVs, triangle indices, and every morph's
position/tangent deltas keyed by the original `SourceIdx`. The JSON manifest
describes the little-endian layouts, sections, material references and hashes.
The mode rejects duplicate names/indices, nonfinite values, invalid triangle
indices, conflicting export modes and an existing audit. Source positions are
Unreal centimetres, without axis conversion or welding.

Use this mode when transferring source morphs. The current glTF conversion
matches morph vertices by position, which is ambiguous at coincident seams.
An audit preserves those distinct indices; it does not establish correspondence
to another mesh or prove that its face supports the same controls.

## Neutral cooked skin diagnostics

`CSS_SKIN_AUDIT=1` exports the highest conventional render LOD directly from
the decoded mesh. `skin-audit.json` records its bones, material slots, sections,
counts, binary layout and SHA-256 hashes. `skin-vertices.bin` retains every
nonzero cooked influence, its integer weight and decoded floating-point weight;
`skin-indices.bin` retains triangle indices. Coordinates stay in Unreal
component space, in centimetres. No axis conversion, weight truncation or
normalization is applied. Run this mode separately from other export modes and
use a fresh output directory. An existing skin audit cannot be overwritten.

This is neutral geometry, not the final visible character: morphs, material
visibility, cloth, physics and post-process graphs are absent. In particular,
do not treat skirt vertices weighted to the foot as shoe soles. The companion
`tools/authoring-probes/animations/measure_skin_contact.py` keeps per-material
measurements and checks bind reconstruction before evaluating saved pose keys.

For cooked ACL samples, pass `measure_skin_contact.py --cooked-skeleton FILE`
with the corresponding Skeleton property readback and full-transform manifest.
This path applies the mesh's translation retargeting before skinning. It supports
the audited Animation, Skeleton and AnimationScaled modes, rejects hierarchy
remapping and other modes, and preserves all influences. The math follows
UE 5.6.1 `FAnimationRuntime::RetargetBoneTransform`. The current Eve comparison
against 13 raw editor poses (379 bones each) agrees within 0.000020 cm position
and 0.000000618 quaternion distance. Hidden flat-foot sections remain in the
per-material results; they cannot be called visible shoe penetration without
checking the outfit's active materials and post-process graph.

For a neutral GLB, use `CSS_BASE_POSE_MESH=1`. It skips morphs and materials;
ordinary exports retain both. The GLB writer uses four influences, so use the
skin audit for complete weight analysis. These opt-in diagnostics leave source
assets unchanged and do not replace ordinary customization exports.

## Full cooked ACL comparisons

`CSS_ABSOLUTE_TRACKS=1 CSS_ABSOLUTE_FULL=1` retains the normal five-sample
JSON and also writes `absolute-tracks.bin` plus a checksummed layout manifest.
The binary contains ten little-endian float32 components per track, ordered
by frame, then track. Every source-key time is included; ACL wrap endpoints
are restored explicitly. The existing two-million-transform allocation bound
still applies. Ordinary exports do not load the optional native helper.

For comparison with Unreal's linear playback, also set
`CSS_ABSOLUTE_RUNTIME=1`. This uses `RuntimeAclSamples.cpp` and records rounding
policy `none`. The older CUE4Parse reader uses `nearest`, which can snap to a
retained key instead of interpolating across a stripped key. Do not interpret
that difference as a defect in the cooked animation. UE 5.6.1's
`ACLPlugin/Private/ACLDecompressionImpl.h:19` selects `none` for linear playback
and `floor` for step playback; this helper is specifically the linear case.

Build the optional Linux helper beside the diagnostic `MeshExport.dll`, from
the repository root (replace `OUTPUT_DIRECTORY` with that directory):

```sh
c++ -std=c++11 -O2 -fPIC -shared tools/MeshExport/RuntimeAclSamples.cpp \
  -Ireference/CUE4Parse/CUE4Parse-Natives/ACL/includes \
  -Ireference/CUE4Parse/CUE4Parse-Natives/common \
  -Ireference/CUE4Parse/CUE4Parse-Natives/ACL/external/acl/includes \
  -Ireference/CUE4Parse/CUE4Parse-Natives/ACL/external/acl/external/rtm/includes \
  -o OUTPUT_DIRECTORY/libCSSACLStudy.so
```

`decode_shared_cooked.py --all-frames --runtime-sampling` enables both modes.
Add `--substeps 2` or `--substeps 4` for native ACL evaluation between source
keys, equivalent to `CSS_ABSOLUTE_SUBSTEPS=2` or `4`. Dense export requires the
runtime mode and currently rejects wrap-optimized clips. It retains the original
five source-clock JSON samples, while the binary manifest records the denser
clock, original rate and substep count. The allocation bound scales with the
explicit substeps, to at most eight million transforms. The two current dance
studies retain bit-identical original-key transforms and five-sample JSON at
two substeps. Dense sampling does not modify an animation or its playback rate.

Use `capture_raw_cook_reference.py` inside the authoring commandlet to capture
unretargeted raw poses, then `verify_full_cooked_motion.py` to compare them.
Mesh-contact captures deliberately apply mesh retargeting and are rejected
as compression references. Direct authored virtual-bone tracks also cannot
replace evaluated virtual poses: Unreal regenerates them during cooking.

The verifier cross-checks binary transforms against the JSON samples before
checking every key. Its existing review limits remain 0.05 cm translation,
0.25 degrees rotation and 0.001 scale. These checks do not establish dense
between-key contact, outfit adapters, physics or live acceptance. Epic's
[ACL documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-compression-library-in-unreal-engine?application_version=5.6)
describes compression thresholds as virtual-vertex errors, which are distinct
from these local-transform review limits.

When a sequence uses owned compression settings, cook those settings separately
and pass `package_shared_library.py --compression-cook LABEL`. The commandlet
accepts an owned standard ACL codec under `/Game/CSS/Locomotion/`; the packager
requires its authoring hash in the verification receipt, a successful cook,
the expected asset class and an exact export round-trip. Settings are package
dependencies, not playable catalog entries. Use the matching
`decode_shared_cooked.py --pack-name LABEL` when the package has a custom output
label. Original codec assets and released outfit packages stay unchanged.

## Stellar Blade source keys

Set `CSS_SOURCE_GAME=GAME_StellarBlade` and `CSS_SOURCE_TRACKS=1` to read an
explicit `AnimSequence` from the user's installed Stellar Blade base containers
with its retail mappings. The default game remains `GAME_UE5_6`; other game
names are rejected. This mode accepts non-additive UE per-track and variable-key compression and
rejects ACL data, which has its own diagnostic modes below.

`source-tracks.json` preserves decoded local quaternion, translation and scale
keys, their frame-index time arrays, source hierarchy and reference pose. It also
records duration, frame count, rate scale and interpolation. Empty channels use
the reference pose. A singleton channel is constant; an absent time array on a
multi-key channel means keys distributed across the complete endpoint interval.
No writer coordinate mirroring or translation retargeting is applied.

Do not use the conversion library's `CAnimSequence.FramesPerSecond` for this
timeline. It divides frame count by duration; Unreal's endpoint interpolation
uses key count minus one. The new export stores both original values, with
`endpointSampleRate=(frameCount-1)/duration`. Explicit source time arrays remain
unchanged. The local UE 5.6.1 `AnimEncoding.h:399` and
`AnimEncoding_PerTrackCompression.cpp:499` establish the endpoint rule in our
target engine; final imported animation must still be evaluated in the editor.

The exporter checks timeline dimensions, unique bone and track mappings,
parent order, finite transforms, quaternion norms and key times. It refuses to
overwrite an existing `source-tracks.json`. Curves, notifies and gameplay logic
are outside this key export; the accompanying `source-mesh.json` retains the
decoded asset metadata. Never install a foreign-game animation directly.

`tools/authoring-probes/animations/review_source_tracks.py WORK --video` plots
the source body joints for up to eight exports under `WORK/*/source-tracks.json`.
It preserves clip duration, rejects non-unit body scale and unsupported playback
settings, and uses normalized quaternion interpolation. This is an early pose
review, not a skinned Eve render or evidence of gameplay compatibility.

The first eight source clips and the unchanged Mortal Shell H2 ACL regression
are documented in [the animation investigation](../../docs/development/eve-animation-sources.md).

`prepare_eve.py --sample-rate 60` prepares endpoint-inclusive 60 Hz candidates.
Native 60 Hz timelines keep their sample indices; 30 Hz timelines retain every
original sample and add interpolated samples between them. Durations must match
a 30 or 60 Hz timeline within float32 rounding. Other rates and downsampling fail
for separate review. Without an override, preparation preserves the source rate,
including the existing scripts' 30 Hz inputs.
`retarget_eve.py` imports and evaluates using the prepared clip's actual rate.
The established normalized shortest-arc quaternion interpolation matches
`AnimEncoding_PerTrackCompression.cpp:528-538` in the local UE 5.6.1 source.

Run the timing/interpolation checks with:

```sh
python3 -m unittest discover -s tools/authoring-probes/animations -p test_sample_timeline.py -v
```

The new library's source inventory and decoder limits are recorded in
[the shared animation investigation](../../docs/development/shared-animation-library.md).

Set `CSS_ANIMATION_POSES=1` to export up to five exact native-decoded ACL samples
in `animation-poses.json`. Schema 2 includes all raw source bones and the authored
retarget base. Additive animations are rejected in this mode.

Also set `CSS_ANIMATION_FULL_HAND=1` to export every ACL sample for the left wrist
and 19 finger/metacarpal bones. Schema 3 omits ancestors and is not a complete
pose. The retarget base is selected by the same source bone indices; the source
base may include a virtual-bone suffix absent from raw animation tracks.

Set `CSS_ADDITIVE_POSE_DELTAS=1` instead for five raw local-space additive samples
in `additive-deltas.json`. The native decoder uses identity rotation and zero
translation/scale deltas for omitted channels. Applying them requires the real
base pose and gameplay blend alpha. Do not treat them as absolute transforms.

## Verified fixtures and limits

The running scythe attack exports 112 frames at 30 Hz in hand mode. All five
previous sparse anchors and their selected retarget-base entries match exactly.
The default mode also preserves all five 1199-bone arrays and the complete base.
Evidence: `work/grip-grounding-v1/running-attack-dense-hand-v2/verification.json`.
The matching native decoder SHA-256 is
`db73331bed384520b4745c3c6efac367fff00237962361600bea2bea2704885b`.

Before further hand experiments, read the
[hand investigation](../../../CSS-Mod-Authoring/docs/next-gen-left-hand-source-contact.md).
V43's grip is accepted; the left fingers remain broken. Source decoding omits
animation overlays, blending, retargeting and IK. A successful export is not a
successful hand repair.
