# Eve authoring cleanup, 2026-09-28

Scope: `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work` and `work`, as requested by the user. Their combined apparent size was approximately 60 GB. Distinct Blender revisions, fitted assets, scripts, exports, releases and rollback files were preserved.

Removed regenerable `gemini-work/ddc`, `work/nextgen-audit/ddc` and Python `__pycache__` directories, totaling 3.00 GiB of logical file data. No Eve editor/build process was using these caches. The active Commander White editor had its own project and DDC path, which were not changed.

Identified 180 groups of byte-identical files larger than 1 MiB using SHA-256. Replaced 1,536 duplicate copies, representing 6.63 GiB of logical data, with independent copy-on-write filesystem clones. Every original path and its contents remain available. This is not hard-link deduplication: later writes to one copy do not modify another. Verified copy-on-write isolation before proceeding, checked each input and clone hash, preserved file attributes and replaced each destination atomically. Distinct Blender revisions were not removed.

Logical byte counts do not equal physical disk reclamation because of compression, existing sharing and concurrent work. The final observed free space and delta are recorded in `work/eve-cleanup/summary.json`.

Evidence retained under `work/eve-cleanup/`:

- `duplicates.json`: hash groups and original paths.
- `caches.json`, `cache-result.json`: removed cache paths and byte counts.
- `dedup-journal.jsonl`: each cloned path, retained source and expected hash.
- `dedup-result.json`, `summary.json`: counts and disk observations.

## Follow-up: superseded Blender revisions

The user then requested cleanup of old Blender revisions as well. Archived 27 superseded fitting experiments and automatic backups from `work` and `gemini-work` into the Eve authoring directory's `archives/blend-history1/revisions.tar.zst`. Their 12,670,576,686 bytes compress to 4,366,607,142 bytes, a net logical saving of 7.73 GiB after removing the loose originals.

Every archive member was streamed back and checked against its original SHA-256. The loose originals and eight protected bases were rehashed before removal. Current bases remain loose, including HeelSupportsV45C, HeelSupportsV45B, ArmRestV44B2, StockingUVV40, NextGen_WithSourceDrivers, the authoring master, Gemini Variants_Fixed and Christmas_Fitted. Other distinct sources and useful revisions remain available.

The archive directory contains the full manifest, retained-base hashes, verification result, deletion record and individual-file restoration instructions. Historical scripts referencing retired revisions require those files restored first. Archive SHA-256: `37ae8235aba29bbfd1fc0caa134a939ee585d1614d5d621de54f31ceebdd4160`.

Observed filesystem free space after this pass: 43,320,418,304 bytes (40.35 GiB). This is an observation, not an exact attribution of physical savings, because other work is running concurrently.
