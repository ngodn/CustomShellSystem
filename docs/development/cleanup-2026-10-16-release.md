# October 16 release cleanup

Before cleanup, root has 93 GB available and theark has 49 GB available (`df -h`).

- `/tmp/css-v100-release`: remove the temporary clean tagged checkout and its regenerable compiler outputs after the final ZIP passes the release verifier. The annotated tag preserves the source.
- `dist/tagged-v1.0.0`: remove the empty staging directory after moving its final ZIP and checksum into `dist/v1.0.0`.
- The previous 3 MB CSS runtime candidate is retained under `dist/withdrawn/css-v1.0.0-candidate-20261016`, with its checksum and status receipt, for the recorded fresh-start evidence.

No asset authoring work, unrelated files or existing media will be removed.

Cleanup completed. Final disk usage:

```text
Filesystem                                             Size  Used Avail Use% Mounted on
/dev/mapper/root                                       930G  813G   93G  90% /
/dev/mapper/luks-3f6913da-3db9-4a8c-836d-20a0c58688be  932G  879G   49G  95% /run/media/eins0fx/theark
```
