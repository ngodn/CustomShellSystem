# Three-outfit rig and physics research index

Updated 2026-10-08. The goal is active. See [implementation status](../development/three-outfit-rig-physics-status.md) for the current baseline and migration audit.

The October 8 goal amendment sets `CSS_SharedAssets_P` as the owner of reusable skeleton, locomotion and Astral assets, keeps character-specific work in each outfit mod, and adds necessary locomotion optimization followed by the remaining default-human animation conversions. The goal's ownership and conversion sections supersede older pack-organization instructions and the previous four-animation native limit. Historical research notes remain evidence of their original checkpoints.

The follow-up amendment explicitly adds implementation of facial-expression support for both Unholy Genessa variants. Its requirements and acceptance checks live in the goal's facial-support section; earlier wording about preserving facial support does not establish that this work is already complete.

## Entry points

- [Goal and implementation status](../development/three-outfit-rig-physics-goal.md).
- [Video observations and audio review](2026-10-08-rig-video-review.md).
- [Actual outfit/source/game-dump inventory](2026-10-08-three-outfit-rig-baseline.md).
- [UE 5.6 mechanisms and local 5.6.1 source evidence](2026-10-08-rig-physics-ue56.md).
- [Local extraction, hashes, transcripts and frames](../../work/rig-physics-review-20261008).

## Settled findings, do not repeat from scratch

1. V1 uses Blender Cloth on a proxy plus Surface Deform, not Soft Body or a runtime bone rig. Exact cloth settings were in an unavailable description.
2. V2 sculpts a foot without a rig. V3 demonstrates finger-joint placement. V4 is a footwear design example, not a full IK or physical-load tutorial.
3. Scope is six current Eve outfits, two Unholy Genessa variants and Commander White. The Eve directory's `SeduXtress` spelling does not authorize work on legacy `Seductress`.
4. The historical current mesh prefix has 386 bones, with four additional Genessa wing bones. These counts differ from both the full native Skeleton and older mesh references. Revalidate identities before editing.
5. Commander White's active shared facial replacement is newer than its outfit body. Preserve E9 and its optical repair.
6. Gameplay Physics Asset, cloth collisions, Control Rig contact and spring limits have separate consumers. Larger colliders already failed to materially improve one Commander White cloth trial.
7. UE 5.6 has a simulation-morph path, but legacy skeletal cloth does not automatically expose those morph arrays. Identify the actual asset path.
8. Historical audits are not fresh runtime acceptance. On October 8 the installed interruption-fix DLL hash was verified and the user confirmed attack, aim/fire, doubles and Genessa Form work with Iris Out.
9. Canonical asset organization is now `/Game/CSS/<author>/<project>/...`, using `eins0fx` for these projects and `eins0fx/Shared` for reusable content. The goal defines migration requirements; existing paths are historical inputs.

## Tooling notes

The local faster-whisper transcriber initially hit a PyAV `metadata_errors` API mismatch. Passing normalized PCM samples from extracted WAV files avoided that decoder path. Transcription completed for all four clips; versions are in `audio-transcripts.json`. After review, the task-local model and virtual environment were removed (582,088,872 logical bytes). All four source hashes were rechecked unchanged. Source videos, extracted evidence and the script remain; validation is recorded in `work/rig-physics-review-20261008/review-validation.json`.

## Open questions for implementation

- Which exact final source recipes reproduce all installed meshes and active facial adapters?
- Which visible defects remain in the accepted current build, and which layer actually causes each?
- Which region/solver owns each moving surface and receives resets in preview, gameplay and doubles?
- What changes improve visible fit without unacceptable frame-time cost or gameplay regression?

Answer these against matched evidence. Do not assume all outfits need new skeletons or every physics technique.
