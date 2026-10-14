# Eve material recipe replay cleanup, October 14

Only `build/eve-material-source-recovery1/replay/stage` is included.
It will contain new outputs from replaying the recorded 68 material-instance
and 204 texture migration requests with the current header writer.

- Remove the replay stage after both writer processes succeed, the file set is exact, and all 745 outputs match the accepted combined candidate's retained payload sources byte for byte. These are duplicate outputs that can be regenerated from the retained original inputs and requests.

Before removal, write a list of exact output paths, matching retained sources,
hashes and logical sizes. Keep the original inputs, both requests, writer
identity/source, logs, reports and comparison receipt. Verify retained sources
after removal and record `df -h` before/after. Do not remove failed or unexplained
outputs under this list. No game files, editor assets or other work are included.
