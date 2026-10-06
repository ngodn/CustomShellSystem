# Perf Tuner work queue

## 2026-10-06: Codex takeover

Owner requested implementation, UI, UX and wording improvements. Apply antislop while
editing. Resolution changes are allowed only in a separately requested diagnostic.

Implemented: restore retries and unload protection, cached console handle, verified
resolution diagnostic, available Stop action, shorter native menu copy, separate Tests,
manual experimental animation, p99 rejection, combined-loss rollback, scan time budget,
release packaging. C++23 and the existing Python 3.14 tools are retained.

Verified: all 11 host suites pass; 121 Perf Tuner checks; release and dev Windows DLL builds.
Tests cover cancelled measurements, world loss, rejected writes, cleanup retries,
unchanged resolution during Tune, p99 regression, and a losing combined set.
Catalog checks also reject known picture controls previously accepted by the parser.

Owner approved installation and live testing. Completed: extension-only installation,
rendered Tune/Tests/confirmation/readings/Stop inspection through native key/action
handlers, full Tune, automatic rollback on p99 regression, cancelled Tune and an on-window
manual test, independent restoration of all 26 watched values, idle and scan cost capture.
The core and loader binaries stayed unchanged. Tuner left off after acceptance.
Original core selector restored. Both player config files stayed byte-identical. Own
screenshots, temporary core/DLL copies and generated reports cleaned; existing reports
restored from the snapshot. Local measurement JSON and rollback backups remain under
the ignored work/perf-tuner-takeover-20261006 directory.

The live pass exposed hidden label/button values in the native host. Fixed in the tuner
model, with regression checks. Also stopped displaying unverified legacy resolution
verdicts. Details and limits are in findings.md and ui-checks.md.

Remaining: physical controller and mouse input, other-platform measurements, experimental
animation combat/shadow testing. The shared core's "Reversible while the menu owns it"
subtitle could use clearer wording in a future core update; no core change was installed.

The resolution diagnostic is optional and separately requested. Experimental animation
needs combat/shadow testing before inclusion in automatic Tune. Windows, AMD, Intel and
Steam Deck need live measurements; do not generalize this machine's results.

Version remains 1.0.0. The owner requested committing the completed takeover work
on 2026-10-06. CSSX and Cheat Menu updates are already committed separately;
their local release tags do not include these remaining Perf Tuner changes.
No Perf Tuner tag or release is requested.
