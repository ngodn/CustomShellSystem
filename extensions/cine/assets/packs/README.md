# CINE pack builder

From the CINE root, run:

```sh
python3 assets/packs/build_pack.py PhotoOrbit
python3 assets/packs/build_pack.py TheyKnowNotWhatTheyDo
```

The remaining pack names are `IrisOut`, `SomethingAintRight`, `MarkOfTheBeast`,
`PoseGlide`, `Turntable`, and `Walks`. Membership follows P1/P2 in
`../../docs/PHOTO-MODE-GOAL.md`. Host Python is 3.14; the authoring script runs
in UE 5.6.1's bundled Python. There are no pip dependencies.

The builder uses the local Linux UE 5.6.1 editor, the existing rootless Docker
daemon and `unholy-wine:11.7` image, Windows UE 5.6.1, retoc, and GameDump.
Paths are declared at the top of `build_pack.py`. It reads the game's global
IoStore metadata and old September usmap, and never writes to the game.
It does not start the Docker daemon, download tools, or install packs.

Every build gets a fresh `work/packs/<Pack>/run-<UTC>/` directory. The builder:

1. Validates preset identities and stereo PCM WAV files, and records input hashes.
2. Writes the canonical `stage/rows.json` and input metadata.
3. Authors a DataTable and one SoundWave per audio ID in a sandboxed Linux editor.
4. Starts a fresh editor process, reloads every asset, and checks row bytes and audio metadata.
5. Cooks in a fresh Windows project under `/home` using the Wine container.
6. Copies only the pack's namespace, including all cooked bulk files, and runs retoc.
7. Reads every package back with GameDump and compares the rows and audio properties.
8. Converts the trio back to legacy packages and compares every export and bulk file's SHA-256.
9. Writes `receipt.json` only after all checks pass, then retains the trio and evidence.

The output folder `run-<UTC>/CINE_<Pack>_eins0fx_P/` contains only the three
shipping files. The receipt records their sizes and hashes. Temporary Wine
prefixes, derived-data caches and intermediate directories are deleted after
verification. Authored assets, cooked packages, dumps and logs are retained.
Failures leave `failure.json` and logs in the attempt directory.

The pack lock prevents concurrent instances of this builder. Linux editor
stages have private writable directories, a read-only system mount, no network,
and a six-core limit. They can run alongside other projects' editor commandlets.
A pack with an existing receipt is not overwritten. Preserve or move that
receipt before intentionally rebuilding. Failed attempts do not prevent retries.

Extra rows can be added as `data/<Pack>/recipe.<id>.json` or
`data/<Pack>/style.<id>.json`. Each file must be a JSON object. Its file stem
becomes the row name, and its complete UTF-8 text becomes `DevComment`.
Case-insensitive row collisions are rejected. No extra rows are supplied here.

Audio IDs keep hyphens, such as `profane-proxima`. Object names replace hyphens
with underscores, such as `SW_profane_proxima`. The manifest's `audio.asset`
is the exact object path. Its duration comes from WAV frame count divided by
sample rate. SoundWaves use Bink quality 80, ForceInline loading, no loop, and
no SoundClass. The runtime must assign the game's Music class override.

This pipeline verifies assets and containers offline. It does not establish
runtime mounting, music volume behavior, seeking accuracy, or video muxing.

After all eight builds, run `python3 -B assets/packs/audit_packs.py`. It checks
the receipts against current inputs and output hashes, compares every recovered
export/bulk payload, reads encoded Bink frame counts and sample rates, and runs
deliberate-corruption controls against each dump. Results go to
`work/packs/validation.json` and each build's `readback-controls.json`.

For one completed build, run
`python3 -B assets/packs/check_readback.py work/packs/<Pack>/run-<UTC>`.
