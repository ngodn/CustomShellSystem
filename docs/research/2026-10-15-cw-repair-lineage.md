# Commander White repair package lineage

The installed CW trio matches the accepted permanent-walk package. Its retained
assembly records connect the namespace baseline, hair strands, ankle correction,
belt, accepted walking trial and permanent walking paths. The final path change
preserves all 445 other payloads exactly.

Evidence: `work/three-outfit-rig-physics/cw-repair-lineage-20261015-pass3/verified.json`.
Recipe: `tools/diagnostics/verify_cw_repair_lineage.py`, Python 3.14.7.
The run exits zero, verifies 578 retained files and rechecks them unchanged before
recording success. No game files, user settings or accepted assets changed.

## Assembly changes

Counts below are header/export/bulk files, not Unreal package counts.

| Stage | Total | Added | Removed | Changed headers | Changed exports |
| --- | ---: | ---: | ---: | ---: | ---: |
| Namespace baseline | 413 | | | | |
| Hair strands | 429 | 16 | 0 | 0 | 0 |
| Ankle modes | 429 | 0 | 0 | 17 | 1 |
| Belt at permanent paths | 445 | 16 | 0 | 1 | 0 |
| Accepted walking trial | 463 | 18 | 0 | 8 | 0 |
| Permanent walking paths | 463 | 18 | 18 | 0 | 0 |

The one replaced export is `Skeletons/SKEL_Strands1.uexp` from the ankle-mode
correction. Other differences in existing files occur in reconstructed headers;
their paths are retained in the receipt. They are not claimed byte-identical or
independently semantically compared by this audit. Existing semantic/readback
checks remain separate evidence. Every retained stage's actual CW containers
match its recorded hashes, and successive recorded binary inputs match their
predecessors.

The initial hair stage used newer metadata than the namespace binary baseline.
Its original metadata pak survives in the hair installation rollback and matches
the hair recipe's input hash. The later `display-name41` metadata-only update
also connects exactly between belt and walking. Neither is treated as an
unexplained binary change. Historical shared-package hashes are not asserted to
be the current shared package.

## Sources and cooked outputs

The retained hair cook's 134 inputs, final belt cook's 140 inputs, belt
finalization's 138 inputs and walking cook's 35 inputs all match. These counts
overlap. Successful source-preserving cook receipts are retained for hair, belt
and walking. The hair and belt cooks each supply eight roots and 16 payloads;
their actual cooked files match their package and assembly tables. All their
export data remains unchanged in the final assembly except the separately
accounted ankle skeleton update.

For walking, the nine current editable assets and 25 protected inputs match the
fresh-reload record. The 14 retained directional reference captures match their
hashes. The final cook's 18 payloads match both the standalone stage and combined
assembly. Recomputing the archived decoded comparison shows exact equality to
the accepted trial after the declared package/object renames: eight sequences,
their compressed animation digests and 395-entry reference poses, plus one
blend space. This does not rerun those editor captures or game movement.

Removing exactly the 18 study-path files from the accepted trial and adding the
18 permanent-path files reconstructs the final 463-entry table exactly. The
actual installed files match:

| File | SHA-256 |
| --- | --- |
| `.pak` | `e5be4cd9952ebe0fbaa8f85480bfe624caba1d73ff0103d81540c98597f433a9` |
| `.utoc` | `114aa8ca5d593de74d2f26d4d91726db894f5133c99fd9f0bfd0a348623a422c` |
| `.ucas` | `630a69e1af0603f2219f3df499cb0b735079b0dec40b67b63c95954199ffd687` |

The files remain under `MortalShell2/Content/Paks/~mods/CSS_CommanderWhite_eins0fx_P/`.
This is the existing `0.0.6-dev` metadata, not the planned stable `0.1.0` archive.

## Limits and retained failures

The first audit stopped because it incorrectly expected the initial metadata pak
to equal the older namespace pak. The corrected comparison uses the original
hair-installation backup. The second stopped because removal paths are relative
to `MortalShell2`, while the payload table includes that prefix. The third run
uses the explicit path conversion and passes. Both failures and their exact
recipes remain; neither changed game files or generated a candidate package.

This closes retained provenance for these accepted repair layers and the final
walking integration. It does not claim a new cook, full original Blender source
reconstruction, runtime contact/performance result or stable release acceptance.
Original base-asset provenance, remaining lifecycle checks and coordinated
stable packaging retain their own requirements. Only small audit records were
created, with no duplicate meshes, cooks or containers to clean up.
