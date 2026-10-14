# Native frame caps and Peony timing

Nine stationary Peony captures pass at requested caps of 30, 60 and the original
999. An independent verifier checks all 4,151 raw records, live body updates and
restoration. Actual cadence was about 29.95 Hz at cap 30 and 53.48 to 54.73 Hz at
the higher caps. This does not establish sustained 60 Hz or high-rate coverage.
No production core or cooked assets changed.

Evidence: `work/three-outfit-rig-physics/frame-cap-20261015/`.
Capture: `peony/`. Installed core:
`5f04a31900c8e323a27f6ed5650649595015b6dea6600f6682db582dc8fcd619`.

## Preserving console priority

The [initial preflight](2026-10-15-dodge-preflight.md) withheld console assignments
because restoring a value alone would not restore its original priority.
The shipped game exposes a separate reflected function:
`USpartaGameUserSettings::SetFrameRateLimit_Always(float NewLimitFPS)`.
Live metadata and static disassembly establish its signature and path before
the first setter call.

The checked game executable has SHA-256
`363322022a0fdfd0cf87c494619da91e0985e6435d105a37def8a89cb3e29922`.
Its native wrapper at static VA `0x145c4de10` writes the always-limit field,
queries the effective limit, and reaches the engine setter through vtable slot
`0x318`. The setter at `0x1445cd180` preserves the existing console priority,
except for constructor-priority promotion to scalability. This matches the
local UE 5.6.1 `UEngine::SetMaxFPS` implementation. The inspected path does not
call SaveSettings or apply unrelated graphics settings.

`read_frame_cap_flags.py` verifies the executable hash, process lifetime,
mapped image, engine setter slot and the exact flag-getter instructions before
reading flags. Its `/proc/PID/mem` descriptor is read-only. The original and
every subsequent observation are `0x01000000`, SetByScalability. It refuses the
constructor-priority case. No native function is called through a raw pointer;
the setter uses the established game-thread reflection bridge.

See Epic's [console-variable priority documentation](https://dev.epicgames.com/documentation/unreal-engine/console-variables-cplusplus-in-unreal-engine)
and [Lyra settings overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-settings-in-unreal-engine).
Those explain the upstream concepts. The game's actual implementation is
established by the retained metadata, disassembly and runtime reads, not by
assuming an unchanged Lyra implementation.

## Measurements

Each capture records ten seconds after one second of profiler warmup. All nine
end for duration, with no recorded CSS exception. The table reports callback
cadence and elapsed CSS callback time, in milliseconds.

| Cap | Run | Records | Tick Hz | Interval median | p95 | p99 | CSS median | CSS p99 |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 30 | 1 | 300 | 29.95 | 33.4002 | 34.9856 | 35.8663 | 0.27125 | 1.4001 |
| 30 | 2 | 300 | 29.95 | 33.3749 | 35.2759 | 35.8649 | 0.26230 | 1.5797 |
| 30 | 3 | 301 | 29.94 | 33.3774 | 34.9278 | 36.0127 | 0.26990 | 1.5175 |
| 60 | 1 | 536 | 53.48 | 19.01035 | 22.0248 | 23.0237 | 0.21580 | 0.7004 |
| 60 | 2 | 537 | 53.59 | 19.0445 | 21.5115 | 22.5033 | 0.21900 | 0.6593 |
| 60 | 3 | 542 | 54.06 | 18.34475 | 21.3169 | 22.7792 | 0.21750 | 0.9424 |
| 999 | 1 | 546 | 54.53 | 18.4914 | 21.9825 | 23.3878 | 0.20380 | 0.7114 |
| 999 | 2 | 540 | 53.86 | 19.0927 | 22.6070 | 23.8965 | 0.20145 | 0.7549 |
| 999 | 3 | 549 | 54.73 | 17.3921 | 22.4597 | 23.4451 | 0.19880 | 0.8021 |

The supplied engine delta is not engine CPU duration. Callback timings can
include thread preemption. Animation workers, cloth, GPU and presentation are
not isolated. These sequential captures have no matched baseline/candidate
pair. The cap-60 shortfall is observed, but its cause is not established. It
must not be attributed to the outfit or CSS from these measurements alone.

## Body and restoration checks

Before/after rig reads at each cap retain identical public region settings,
matching rig inputs, limits and maximum travel. Total solver steps advance by
5,595, 5,643 and 5,665 respectively. `ResetCount` remains 2 throughout. Every
sampled float is finite. This is a boundary check during a stationary idle, not
a continuous contact or amplitude assessment.

The native cap setter receives 30, 60, 999 and a final restoration to 999. The
four graphics console values, their original cap priority, CSS JSON, CINE,
appearance, position/facing, camera and prior desktop focus restore. The
GameUserSettings.ini hash is unchanged. No gameplay input, raw memory write,
save request or core reload occurs.

`tools/diagnostics/verify_capped_cost.py` independently recomputes every timing
summary from raw rows, checks contexts and rig preservation, and rejects changed
priority, stalled-rig and nonfinite-rig controls. Run:

```sh
python tools/diagnostics/verify_capped_cost.py work/three-outfit-rig-physics/frame-cap-20261015/peony
```

The retained capture recipes and `requests.jsonl` record the executed commands
and reflected calls. `engine-set-max-fps.asm`, address-qualified setter/getter
disassemblies, `engine-slot.json`, `cvar-getter.json`,
`live-settings-readonly.json` and `cvar-flags-before.json` retain the native
path evidence. No offsets from this build are assumed portable to another.

Other outfits, moving playback, matched cost comparisons and actual sustained
60/higher-rate integration remain open. The narrower result establishes a
reversible native frame-cap mechanism and Peony's sampled stability under the
recorded cadence. Root and theark remain at 96G and 49G free (`df -h`).
