# Shared animation cook

2026-10-06. Private candidate, not installed or released.

`work/cine-animation-port/shared-v1/` contains 57 UE 5.6.1 Windows-cooked
sequences. CSSCookAssets exited 0. Its read-only user-config warnings did not
affect the cook. The separate `CSS_LocomotionSharedAssets_P` trio is 7,012,825
bytes and contains only `/Game/CSS/Locomotion/` assets plus CSS library metadata.

The package uses `/Game/CSS/Shared/SKEL_Base`, supplied for this check by the
installed Eve 1.4.0 trio. It is not an independent skeleton distribution.
The verification mount contains the base game, that dependency and the new
pack. No other outfit or editor content supplied missing imports. The dependency
hashes are retained in `pack/verification.json`; no existing trio was modified.
Genessa-only dependency closure still needs a separate check before release.

All 57 export payloads remain byte-for-byte equal after IoStore conversion and
extraction. Reconstructed headers contain no unresolved imports. The metadata
pak also round-trips exactly. AssetReadback independently decodes all 57 packages.

Native ACL decoding and UE raw evaluation compare 273 sampled poses across all
57 clips, including all nine mapped virtual bones. Every duration and sample
rate matches the fresh authoring receipt. ACL wrap optimization removes a
repeated endpoint in some clips; the decoder reports this flag and reconstructs
that endpoint explicitly. It does not accept an arbitrary missing frame.

Worst measured sample errors:

| Quantity | Maximum |
| --- | --- |
| Local translation | 0.008586 cm |
| Quaternion chord distance | 0.001728 (about 0.198 degrees) |
| Scale vector distance | 0.0000266 |

Review limits are 0.05 cm, 0.25 degrees and 0.001 scale distance. The largest
translation error is Lily Catwalk's virtual left forearm; the largest rotation
error is its right middle fingertip. These are sampled compression checks,
not visual, gameplay or all-frame acceptance.

The first decoder attempt rejected sequence-owned reference poses because it
only consulted the skeleton. `RawAbsoluteTracks` now follows UE 5.6.1
`UAnimSequence::GetRetargetTransforms`: use the sequence's stored reference when
RetargetSource is None; otherwise use the named skeleton reference or default.
The installed UE source confirms this order. Epic also documents the stored
[sequence reference pose](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UAnimSequence?application_version=5.6).
No cooked animation was changed to satisfy the decoder.

Evidence:

- `cook-command.json`, `cook-exit.json`, `cook-console.log`
- `pack/verification.json`, `cooked-properties.json`
- `cooked-samples-v3/decoded.json`, `motion-verification.json`
- `compare-v3-exit.json`, `compare-v3-console.log`

Tools: `package_shared_library.py`, `decode_shared_cooked.py`, and
`verify_shared_cooked.py` under `tools/authoring-probes/animations/`.
The comparison commandlet reads `CSS_ANIM_WORK` and `CSS_COOKED_SAMPLES`.
Outputs are exclusive to preserve failed attempts as evidence.

The two unresolved dances, runtime one-shot/pose controls, facial support,
outfit-specific physics adapters and live acceptance remain unfinished.
