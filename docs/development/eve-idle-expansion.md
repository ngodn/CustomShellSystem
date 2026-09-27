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
