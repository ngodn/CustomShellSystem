# Three-outfit rig and physics research index

Updated 2026-10-08. The goal is active. See [implementation status](../development/three-outfit-rig-physics-status.md) for the current baseline and migration audit.

The October 8 goal amendment sets `CSS_SharedAssets_P` as the owner of reusable skeleton, locomotion and Astral assets, keeps character-specific work in each outfit mod, and adds necessary locomotion optimization followed by the remaining default-human animation conversions. The goal's ownership and conversion sections supersede older pack-organization instructions and the previous four-animation native limit. Historical research notes remain evidence of their original checkpoints.

The follow-up amendment explicitly adds implementation of facial-expression support for both Unholy Genessa variants. Its requirements and acceptance checks live in the goal's facial-support section; earlier wording about preserving facial support does not establish that this work is already complete.

## Entry points

- [Goal and implementation status](../development/three-outfit-rig-physics-goal.md).
- [Video observations and audio review](2026-10-08-rig-video-review.md).
- [Actual outfit/source/game-dump inventory](2026-10-08-three-outfit-rig-baseline.md).
- [Current editable mesh structure and migration sources](2026-10-08-editable-mesh-audit.md).
- [Genessa facial candidate and rejected trials](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P/authoring/facial-controls-notes.md).
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
10. All 790 migration rows have editable candidates or historical alias recipes. Sixty-one animation paths are generated aliases, not missing original clips. Eve's two v1.4.0 hair instances must come from their receipt-pinned private project, not the older main-project copies.
11. Fresh editor-buffer checks of nine outfit meshes and seven facial replacements found normalized weights, valid section/influence/morph indices and finite poses/deltas. This does not establish deformation or collision quality. Mesh raw-bone counts and Skeleton helper counts remain distinct.
12. Genessa's mask mouth is sealed. Direct expression transfer stretches this surface and produces poor eyelids. The retained facial candidate splits the lip contact line, uses mask-specific lid controls and adds an oral surface. Its underlying head must receive synchronized facial deltas to avoid obstructing the opening or breaking through the cheeks. This is proven on source-based previews; W3 integration and game acceptance remain pending.
13. The subsequent W3 candidates pass independent cooked geometry/morph readback and full cloth-data comparison. Their private facial catalog has two revision-bound adapters, 21 presets and six existing facial performances. Packaged metadata preserves Eve, Commander White and all 63 body entries. This establishes offline integration, not live acceptance. The status document points to current reports; do not repeat the original sealed-mouth or missing-catalog diagnosis.
14. The main authoring project's native-human skeleton is a smaller placeholder. Use the receipt-pinned `native-binding-v6/game-reference` skeleton only inside isolated migration projects. The rejected cook reduced native mappings from 1,208 to 182 tracks; the corrected cook matches all 20 installed native sequences and four blend spaces. The twenty Eve reference-pose refreshes have full source-key motion evidence, including virtual tracks.
15. The five playback adapters contain character-specific secondary-motion nodes and belong to their respective outfit packages. `namespace-3` corrects that ownership without changing any of the 63 public animation IDs. The 124-package editor7/cook6 trial passes independent readback and 547 imported references with explicit baseline dependencies. It remains a partial migration, not game acceptance or final split packaging.

## Tooling notes

The local faster-whisper transcriber initially hit a PyAV `metadata_errors` API mismatch. Passing normalized PCM samples from extracted WAV files avoided that decoder path. Transcription completed for all four clips; versions are in `audio-transcripts.json`. After review, the task-local model and virtual environment were removed (582,088,872 logical bytes). All four source hashes were rechecked unchanged. Source videos, extracted evidence and the script remain; validation is recorded in `work/rig-physics-review-20261008/review-validation.json`.

## Open questions for implementation

- Which exact final source recipes reproduce all installed meshes and active facial adapters?
- Which visible defects remain in the accepted current build, and which layer actually causes each?
- Which region/solver owns each moving surface and receives resets in preview, gameplay and doubles?
- What changes improve visible fit without unacceptable frame-time cost or gameplay regression?

Answer these against matched evidence. Do not assume all outfits need new skeletons or every physics technique.
