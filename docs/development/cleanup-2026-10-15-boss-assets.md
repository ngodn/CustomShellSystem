# Boss asset staging cleanup

Before removal, October 15:

- `build/shared-boss-appearances1`: discard the first incomplete candidate and
  its duplicate stage/readback. Its verifier incorrectly required byte-identical
  raw cooked Skeleton headers after IoStore reconstruction. All four headers
  retain package/export identity but lose the editor-only IsAsset flag; export
  bodies will remain byte-checked and full decoded objects compared in the
  corrected recipe. No installation occurred. Original cooked rigs, accepted
  trial, baseline shared pack and source recipe are retained. Reuse this output
  directory for the corrected candidate.

No other work folders or backups are included in this removal.

After successful package readback and live installation, remove only these
regenerable subdirectories from `build/shared-boss-appearances1`:

- `stage`: duplicate extracted shared payloads and relocated boss payloads;
  sources, relocation manifest, recipe and accepted container remain.
- `readback`: duplicate verification extraction; hashes and decoded comparisons
  remain in `verified.json` and `decoded.json`.
- `metadata-readback`: duplicate metadata unpack; the source metadata, candidate
  pak and successful roundtrip result remain.

Keep `metadata`, `pack`, the executed recipe, receipts, live checks and
`installation`. Keep the old shared/trial containers in the game Content
rollback directory, outside mounted Paks.
