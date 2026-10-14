# Eve repaired-mesh and material source chain, October 14

The combined candidate now has a checked source chain for all twelve repaired
meshes and the 273 older material/texture packages it preserves. No game files
changed and no new cook was run. This closes a concrete source-path gap; full
release provenance and runtime acceptance remain separate.

## Repaired mesh sources

`build/eve-surface-package1/source-chain.json` identifies each saved final editor
mesh, its hash, cook inputs/configuration, recorded command, successful exit,
shipping allowlist and the matching `.uasset`/`.uexp` payloads in the combined
candidate. All twelve final source files are present and match their fresh
reload receipts. Their hashes also occur in the successful cook input records.
All 71 distinct editor dependency packages remain available with consistent
hashes across the five final-path projects.

The material/rig dependencies in those cook projects are dependencies only.
The shipping allowlist contains the repaired meshes. Packaging takes the other
assets from their accepted providers; it must not replace preserved native
materials with the editable binding proxies emitted by a mesh cook.

## Restored original material inputs

The first check found the original extraction directory absent. The historical
`cleanup-eve-enospc1.json` records its deliberate removal as reproducible data.
The retained trio in the game's `Content/CSS-namespace-trial1/backup/` still
matches all three hashes recorded by the original material migration.

`tools/diagnostics/recover_eve_material_sources.py` extracts exactly the required
272 original packages. Every one of their 745 files matches its historical hash.
The old `eve-material-namespace1/original` path now links to
`build/eve-material-source-recovery1/original` on root. This restores the material
and texture inputs, not every asset that once occupied that extraction folder.

Only 272 unique original headers, totaling 514,512 bytes, need new storage.
The other 473 files are absolute links to existing accepted payload sources,
after exact hash comparisons. Their 886,932,283 duplicate logical bytes were
removed under the [prewritten recovery cleanup list](../development/cleanup-2026-10-14-eve-material-source-recovery.md).
Both original package and installed files remain untouched.

## Current writer replay

A second check found that the original material receipt pins an older header
writer. The historical hash remains recorded; it was not replaced with a claim
that the binary stayed unchanged. The current writer has SHA-256
`64aaebcabc565dc5046c8e05590decadb4573d1ab21fc5c438da5e69c89cfdde`.

`tools/diagnostics/replay_eve_material_sources.py` replays the original requests
for 68 material instances and 204 textures into a separate stage. It keeps the
original path map and input files. The writer checks unchanged and inverse
header roundtrips, then all 745 generated files match the accepted candidate
payloads byte for byte. This establishes compatibility for these requests with
the current writer. No shader or texture pixels were reauthored.

After verification, the [prelisted replay cleanup](../development/cleanup-2026-10-14-eve-material-replay.md)
removed 887,446,730 duplicate logical bytes. Requests, reports, logs, tool/source
hashes and comparison records remain under `build/eve-material-source-recovery1/replay`.
The extra authored hair parent is accounted for separately: its four editable
material/texture inputs, original renderer configuration and successful cook/
reload records are present and unchanged. Its two shipping payloads match the
verified older material assembly and the combined candidate.

## Evidence and limits

The final source-chain check pins 1,408 retained input paths, verifies the full
1,111-payload source inventory remains available, and links the twelve repaired
mesh cooks plus all 273 older material/texture packages to the combined build.
Earlier failed checks and the successful intermediate mesh/native-material check
are archived alongside the final report. No accepted candidate file changed.

Immediate `df -h` before/after recovery and replay cleanup:

| Filesystem | Before | After |
| --- | --- | --- |
| Root | 92G available | 92G available |
| theark | 67G available | 67G available |

These rounded readings do not establish physical bytes reclaimed. All newly
retained unique headers and receipts live on root; no full project copy was made.

Re-run the source-chain check with a new output filename:

```sh
python3 tools/diagnostics/audit_eve_surface_source_chain.py \
  --candidate build/eve-surface-package1 \
  --output build/eve-surface-package1/source-chain-recheck.json
```

The recovery and replay recipes deliberately reject existing output directories.
Their recorded inputs/results are retained, so normal verification uses the
source-chain check rather than regenerating duplicate extraction trees.

This evidence does not establish fresh game rendering, live material parameter
execution, simulation, performance or a sunlight-noise fix. The unchanged Peony
and remaining rig/source chains retain their earlier evidence and still belong
to the full release audit. Nothing is installed from the combined candidate.
