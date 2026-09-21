# Animation diagnostic exports

The tool targets .NET 10. The verified build used SDK 10.0.401 and the matching
CUE4Parse native ACL decoder. These modes read source assets; they do not repair
or reproduce the complete gameplay AnimGraph.

## Stellar Blade source keys

Set `CSS_SOURCE_GAME=GAME_StellarBlade` and `CSS_SOURCE_TRACKS=1` to read an
explicit `AnimSequence` from the user's installed Stellar Blade base containers
with its retail mappings. The default game remains `GAME_UE5_6`; other game
names are rejected. This mode accepts non-additive UE per-track compression and
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
