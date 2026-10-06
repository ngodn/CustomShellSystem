# Animation diagnostic exports

The tool targets .NET 10. The verified build used SDK 10.0.401 and the matching
CUE4Parse native ACL decoder. These modes read source assets; they do not repair
or reproduce the complete gameplay AnimGraph.

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

For a neutral GLB, use `CSS_BASE_POSE_MESH=1`. It skips morphs and materials;
ordinary exports retain both. The GLB writer uses four influences, so use the
skin audit for complete weight analysis. These opt-in diagnostics leave source
assets unchanged and do not replace ordinary customization exports.

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
