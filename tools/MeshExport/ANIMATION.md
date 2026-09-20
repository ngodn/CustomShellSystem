# Animation diagnostic exports

The tool targets .NET 10. The verified build used SDK 10.0.401 and the matching
CUE4Parse native ACL decoder. These modes read source assets; they do not repair
or reproduce the complete gameplay AnimGraph.

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
