# Eve idle expansion

Target: 14 reference animations plus the accepted Eve Default Idle, 15 choices total. The accepted six-outfit v1.2.0 release remains untouched.

## Evidence and current state

- Source inventory and archive hashes: `work/eve-idle1/inventory.json`.
- ESAP contains numbered entries 700 through 713, with additional companion entries. Actual motion must be inspected before assigning names or suitability. Asset names alone are not motion evidence.
- Retained peaceful idles 02, 03 and 04 were prepared in `work/eve-idle1/prepared` as workflow reference material. They are not substitutes for the requested reference animations.
- Preparation writes source-named JSON files. `batch.json` maps short labels to those filenames, matching the retarget reader. The earlier lookup for `Idle2.json` was incorrect; no preparation-script fix is needed.
- First reference decode: clip 700 loads as UAnimSequence (101 frames, approximately 3.333 seconds), but source-track validation rejects its skeleton map. See `work/eve-idle1/decode700-check.json` and `decode700.log`.
- Its map contains indices beyond the retained 3267-bone base skeleton. Determine the actual compatible skeleton before retargeting. Do not truncate tracks or weaken validation to make decoding pass.
- The first mount attempt omitted global containers. Corrected the source mount with read-only symlinks to the installed game containers. No game installation changed.

## Next work

Locate the matching skeleton or establish the reference map from source evidence. Decode and visually inspect each requested character clip, distinguish companion tracks, then use the accepted source-basis and retarget workflow against the current CSS skeleton. Record per-clip offline and in-game checks before packaging.

No new animation has been cooked, installed or accepted yet. Other agents' runtime edits are outside this change.

## Batch inspection

All 14 primary ESAP clips loaded successfully through metadata-only inspection. Each has 101 frames, approximately 3.333 seconds, 234 compressed tracks and indices reaching 3341. The 75 indices from 3267 through 3341 are common to all clips. Full records: `work/eve-idle1/clip-metadata.json`.

| Clip | Metadata | Track mapping | Visual review | Conversion / acceptance |
| --- | --- | --- | --- | --- |
| 700 | Loaded | 75 unresolved indices | Pending | Pending |
| 701 | Loaded | 75 unresolved indices | Pending | Pending |
| 702 | Loaded | 75 unresolved indices | Pending | Pending |
| 703 | Loaded | 75 unresolved indices | Pending | Pending |
| 704 | Loaded | 75 unresolved indices | Pending | Pending |
| 705 | Loaded | 75 unresolved indices | Pending | Pending |
| 706 | Loaded | 75 unresolved indices | Pending | Pending |
| 707 | Loaded | 75 unresolved indices | Pending | Pending |
| 708 | Loaded | 75 unresolved indices | Pending | Pending |
| 709 | Loaded | 75 unresolved indices | Pending | Pending |
| 710 | Loaded | 75 unresolved indices | Pending | Pending |
| 711 | Loaded | 75 unresolved indices | Pending | Pending |
| 712 | Loaded | 75 unresolved indices | Pending | Pending |
| 713 | Loaded | 75 unresolved indices | Pending | Pending |

A fresh read of the installed base skeleton confirms 3267 reference bones and no serialized VirtualBones property. This rules out attributing the extra indices to an available virtual-bone definition without further evidence. Epic distinguishes final reference bones from raw bones, with final bones including virtual bones: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FReferenceSkeleton .

Read-only scans of the installed third-party IoStore containers found no override at the exact referenced `CH_P_EVE_01_Skeleton.uasset` path. The scan does not cover legacy-only pak contents. Receipt: `work/eve-idle1/skeleton-candidates.json`. The ATOOL and ESAP IoStore listings also contain no matching skeleton asset. Next inspect legacy pak payloads and source track compatibility before deciding how to resolve the added indices.
