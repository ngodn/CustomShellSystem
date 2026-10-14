# Eve supporting rig replay cleanup, October 14

Planned before generation or removal:

- `build/eve-supporting-rig-source-check1/stage/ABP_Secondary.uasset` and `.uexp`: remove these two duplicate replay outputs only after both match the retained accepted graph and current candidate payload hashes.
- Remove the empty replay `stage` directory after the exact two-file check.

Keep original compiled inputs, editable rig sources, retained cooks, current
candidate, installed files and rollback untouched. Keep the request, writer
report, command, exit, log, verification and cleanup receipts. No other paths
are covered by this removal list. This replay should produce about 114 KB and
does not copy a project or container.
