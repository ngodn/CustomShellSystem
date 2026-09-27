# Loaded combat probe 01

Status: built, host-tested and installed on 27 September 2026 after the user confirmed the game was closed. The first live capture completed successfully. This is the first live discovery probe. Existing cooked exports and CSS/CSSX patterns supply candidate names, but do not establish that these CCS reads work in this game session.

## Scope

The separate `CCS_DISCOVERY_PROBE` build starts idle. F7 starts a capture; pressing it again while running cancels. It reads the current controller, pawn, ASC and `ActivatableAbilities.Items`, then captures one granted ability per tick, including already-existing instances, selector class arrays, montage references, skeleton references, play rates and instancing policy when available.

It also records the reflected signatures of the native montage task and Asset Registry `GetAssetsByClass`. It does not invoke those interfaces. An absent interface is recorded as unavailable, not assumed compatible. No attack hook, asset loading, asset construction, stat changes, root changes or menu attachment occurs. The generated 109-move snapshot is not opened in this build.

The probe validates reflected property kinds, offsets, sizes, struct and array layouts, bounded counts and live object identity. It stops when the player or grant list changes. Each raw grant pointer stored between ticks is only an identity token; the current array is re-read before accessing a grant.

## Limits

A capture has a 15-second timeout, at most 512 steps, 256 grants, 16 entries per selector array, and 8 instances per replication group. The record queue is bounded by the existing writer (1024 records, 64 KiB per record). Logs carry session/run identifiers and start/end records.

The 2 ms step ceiling is a measured stop condition after a step returns. It cannot preempt a reflected lookup or guarantee a hitch-free first capture. No FPS or runtime-safety claim follows from compilation or host tests. Start the first capture while stationary in a safe location. Avoid changing weapons, shells or levels until it ends. An incomplete capture, error, changed identity or timing overrun is evidence to investigate, not a reason to retry automatically.

This pass covers loaded player grants only. It does not enumerate unloaded moves, validate swap compatibility, read all Tarstone effects, identify installed game content, or implement patch-driven cache invalidation. Signature presence alone does not prove Asset Registry query marshalling. Those are subsequent probes after this one is verified.

## Build and installation

From the repository root:

```sh
python3 CCS/tools/ccs.py build-probe
```

This writes only `CCS/build/windows-probe`. The ordinary `build` action explicitly disables the probe. Both disable the experimental menu, and CMake refuses a combined menu/probe configuration.

Installed with fresh stopped-playing confirmation. Future installations still require fresh confirmation and a closed game:

```sh
python3 CCS/tools/ccs.py stage --probe --confirm-game-stopped
```

The staging tool checks game processes and the pinned installed UE4SS DLL before building, then checks processes again before replacement. Existing user presets are preserved. After launching and loading the player, F7 starts the capture. The capture writes `Mods/CCS/logs/discovery.jsonl` through the background writer.

After the capture finishes, summarize its actual output:

```sh
python3 CCS/work/takeover/probe_report.py /mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Binaries/Win64/ue4ss/Mods/CCS/logs/discovery.jsonl
```

The report uses the latest capture, verifies its grant counts and terminal record, and lists observed selector and montage paths. Missing output, malformed records, gaps and partial/error captures cannot become a verified-complete report. A complete report means the bounded reads completed; it does not mean every interface or move is usable.

## Verification

- Windows default and probe variants compile with C++23 and the pinned UE4SS SDK using `/W4 /EHsc /permissive-`.
- Host CTest covers writer failure/flush behavior, runtime storage/catalog contracts, probe cancellation/timeout/limits, staging guards and report integrity.
- Host tests also run with address and undefined-behavior sanitizers.
- Installed loader/core hashes and the installed UE4SS pin were independently verified after staging.
- Live results are recorded below.

## Live result, 27 September 2026

Session `751102387548`, run `1`, completed all 137 grants in 140 steps across about 2.281 seconds. The maximum recorded capture step was 1,807 microseconds. No probe error records were emitted, and the independently parsed start/end counts agree.

Four granted Clockwork Scythe selectors yielded 16 distinct attack-class references. Their live arrays include three combo stages per selector, running references in the hold selectors, and one additional finisher in each ordinary selector. This proves the runtime reader can follow this player's currently granted selector links without opening the extracted catalog. It does not prove discovery of other weapons or enemies.

The whole grant set referenced 26 distinct montages, including non-attack actions. These must not all become selectable combat moves simply because the player owns their abilities.

Both probed interfaces were present. The montage task exposed 8 parameters in a 56-byte frame. `GetAssetsByClass` exposed 4 parameters in a 34-byte frame; its `ClassPathName` occupies 16 bytes. The SDK wrapper instead marshals an 8-byte `FName ClassName` and puts output at offset 8. Do not use that wrapper for this game. The live output is at offset 16. Future registry queries must validate and use runtime reflection, not these numeric offsets.

Raw capture and derived reports are retained under `CCS/work/discovery-probe-01/`. Fourteen of the referenced classes are present in the research snapshot and their montage paths match. The two running-attack classes are absent from that snapshot. This is a coverage gap, not a measured montage disagreement. A snapshot comparison is research evidence only. Live values retain priority when they disagree. Further controls are needed for weapon changes, travel/cancellation, direct tag semantics and registry queries. This timing observation is not an FPS benchmark.
