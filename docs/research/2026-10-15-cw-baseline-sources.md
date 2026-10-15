# Commander White baseline sources

The retained namespace baseline is accounted for: 1,622 file hashes connect
172 editable source assets, preserved compiled materials and textures, and the
rig cook to all 413 baseline payloads. The baseline links to the previously
verified [installed repair chain](2026-10-15-cw-repair-lineage.md).
No game files, settings or saves changed during this check.

## Source and assembly checks

| Group | Assets | Payloads | Check |
| --- | ---: | ---: | --- |
| Preserved material instances | 80 | 160 | Original extracted inputs, relocation evidence and accepted stage hashes |
| Preserved compiled material parents | 2 | 4 | Original inputs and accepted relocated stage hashes |
| Textures | 78 | 227 | Original inputs, all accepted outputs, fresh exact relocation replay |
| Cooked rig/physics/normal and facial mesh assets | 9 | 18 | Original editable sources, migrated editor files and actual cook outputs |
| Preserved compiled B2/B3 body graphs | 2 | 4 | Accepted stage, original inputs and fresh exact relocation replay |
| Total outfit baseline | 171 | 413 | Exact union, no duplicate or unexplained payloads |

All 172 selected original editable assets and their migrated files still match
their recorded hashes. That selection also contains the shared base skeleton;
it does not mean all 172 editor files were cooked into this outfit. The
12-package cook includes that shared skeleton and two graphs whose compiled
originals were intentionally retained instead of their editor outputs.

The material assembly's 391 payloads survive unchanged in the complete rig
assembly. The final unique-container baseline has the same 413 payloads.
Its actual `.pak`, `.utoc` and `.ucas` hashes match the archived receipt.
The separate repair-chain receipt pins this baseline and the currently installed
trio, covering subsequent hair, ankle, belt and permanent walking overlays.

The original CW and retired locomotion source containers are verified from
`Content/CSS-namespace-trial1/backup`, including their recorded sizes and
hashes. Their historical installed paths now contain different files or no
longer exist. The audit records those relocations explicitly.

## Reproduction and limits

Five texture-era tool/source files have advanced since that receipt: the
relocation executable and Rust source, AssetReadback executable and C# source,
and the texture migration Python recipe. Their historical hashes are retained
as historical facts, not asserted to match the current tools.

Using the retained current relocation executable and original requests freshly
reproduces all 227 texture payloads and all four compiled body-graph payloads
byte-for-byte. All 80 replayed assets also report passing unchanged and inverse
round trips. The two compiled graphs preserve the original property names
which the editor cook had changed. This replay does not rerun AssetReadback.

The original 172-package decoded comparison and two full mesh comparisons
remain pinned historical evidence. No fresh UE cook, whole-outfit Blender
rebuild, gameplay test or broader visual acceptance is claimed. The accepted
editable baseline and compiled native inputs are retained reproduction sources;
recreating the outfit from its earliest Blender scene is not an added gate.

## Receipts and cleanup

- Recipe: `tools/diagnostics/verify_cw_baseline_sources.py`.
- Passing evidence: `work/three-outfit-rig-physics/cw-baseline-sources-20261015-pass5/`.
- Prior attempts remain as small failure records: obsolete tool hashes, a
  list-versus-count schema assumption, and incorrectly applying the historical
  backup lookup to the current installed trio. None changed the game or sources.
- The [cleanup prelist](../development/cleanup-2026-10-15-cw-source-replay.md)
  authorized removal of the 231 exact duplicate replay files after verification.
  `cleanup-plan.json` records every removed path/hash; requests, logs and results
  remain. Removed logical size: 978,946,512 bytes.
- `df -h` before and after cleanup reports root 95G available and theark 49G
  available. Rounded values did not change; the logical byte count is not a
  claim of equivalent physical space recovery.

CW's baseline and later accepted repair sources are now linked to the installed
package. Coordinated final release artifacts and remaining runtime/lifecycle
requirements remain open.
