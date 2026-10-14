# Source replay cleanup, October 14

Removal list recorded before deletion:

- `work/three-outfit-rig-physics/eve-source-remaining-20261014/blackpearl-full-export/candidate.mesh.json`:
  regenerated duplicate of the retained accepted Black Pearl interchange.
  Remove only after the exporter exits successfully, the complete comparison
  passes and its SHA-256 matches the retained original. Keep original inputs,
  regenerated audit, commands, logs, exit status and comparison receipt.

No editable source libraries, accepted imports, cooked assets, installed files,
rollback packages or other sessions' files are included. Record the exact path,
hash, logical size and `df -h` before/after in the adjacent `cleanup.json`.

Completed: removed the one 108,976,873-byte duplicate after successful replay
and byte-identical comparison. The original has the same SHA-256. Immediate
free space was root 95G and theark 14G before/after deletion; the settled reading
is root 95G/theark 15G, matching the start of this source work. No other files
were removed. Newly recovered source libraries and small failure logs remain.
