# Eve original material input recovery, October 14

Only the new root-drive `build/eve-material-source-recovery1` is included.
Recover the 745 originally recorded files from the retained original Eve
rollback package. Verify the package trio before extraction and every recovered
file against its original material/texture migration hash.

After the extractor exits successfully:

- Replace recovered files with absolute links to existing accepted payloads only when their SHA-256 values match exactly. This avoids keeping another copy of unchanged texture/export bytes. Retain unique original headers.
- Reject unexpected extraction files. Do not delete unknown outputs or change the retained rollback package.

Write the exact replacement list with source/target hashes and logical bytes
before unlinking duplicates. Recheck every resulting path afterward. Keep the
recipe, source receipt, commands, logs and unique original inputs. Restore the
missing `eve-material-namespace1/original` path as a link only after validation.

No installed files, saved settings, original archives, editor assets or other
project directories are changed. Record `df -h` before/after. Logical bytes
removed are not a measurement of physical space reclaimed.
