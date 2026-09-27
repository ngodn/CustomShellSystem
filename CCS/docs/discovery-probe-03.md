# Native montage-task probe 03

Status: compiled locally, not installed or observed live. This candidate uses the [loader-owned native hook service](native-hook-service.md). It records task requests without changing parameters, calling the task, loading assets or editing abilities/montages.

F7 arms a capture and the optional frame profiler together. A second press cancels the task capture. The probe waits up to five seconds for the current player, retrying at 250 ms intervals. It checks the already-loaded `AbilityTask_PlayMontageAndWaitWithNotifies` function before registering a pre-hook: native/nondelegate flags, eight parameters, bounded frame size, seven input names, input directions, nonoverlapping storage and exact property kinds. Ability and montage inputs must declare the expected engine classes. All offsets come from reflection. The interface record includes the observed offsets, sizes, function path and declared classes.

Each callback checks the live function identity, FFrame node and locals, then the captured world/controller/pawn/ASC/weapon-item/shell identities. It accepts an ability only when its physical Outer chain reaches that pawn or ASC within eight links. CDOs and archetypes are rejected. This is **physical ancestry only**. It does not prove ASC grant membership, ActorInfo avatar, attack role, combo slot or replacement compatibility. Player-owned nonattack montage tasks may appear. Captured paths are observations, never selectable moves.

All retained identities require positive object-array serials. Cold setup uses the [uncached serial initializer](object-identity-runtime.md); callbacks only read existing identities and warm reflection-cache entries. Missing ability/montage identities are skipped and counted separately as `identity_skipped`. A new per-execution instance may not yet have an engine weak serial at the task's pre-hook, so live coverage must be checked rather than assuming all player calls will be retained. No callback initializes a serial through ProcessEvent.

Schema 2 also records `CurrentEventData.EventTag` and `bIsActive` from the owning ability at the callback. Before registration, reflection checks the GameplayAbility, GameplayEventData and GameplayTag owners, field bounds and property kinds. Weak identities guard the owners throughout capture. The callback reads only the nested FName and bool, without copying an owning event struct or calling an ability getter. A missing event tag may be valid. An inactive ability's retained event may be historical. Even a tag on an active ability is an observation, not proof of a unique slot.

The callback copies object identities, three names, scalar inputs and a timestamp into a fixed 64-row buffer. It performs no text formatting, JSON serialization or file writes. The game-thread tick formats at most four rows per pass and queues them to the background writer. It stops after 64 accepted records, 30 seconds, cancellation, a generation change, an invalid contract or a measured callback over 2 ms. The timing check runs after the callback returns; it cannot preempt work or guarantee frame time. Idle captures retain no hook.

Removal is retried at 250 ms intervals when the SDK defers physical unregistration. The probe retains its token and reflected property pointers until removal succeeds; normal core unload refuses during that interval. Output failure disables further captures and still attempts removal. Completion is reported only after the terminal record has drained through the worker, without a game-thread flush wait. Missing output or an abrupt shutdown cannot establish a successful capture.

## Build and inspect

```sh
python3 CCS/tools/ccs.py build-attack
python3 CCS/work/takeover/attack_report.py path/to/attack-calls.jsonl
```

The candidate is `CCS/build/windows-attack-profile`. It includes diagnostic profiling, disables the menu and registry/discovery probe, and writes `logs/attack-calls.jsonl` under the installed CCS directory when eventually used. Normal builds explicitly disable the attack-probe option.

The report verifier accepts schemas 1 and 2, checks the latest capture, bounded sequential calls, finite inputs, physical ownership labels, native signature, terminal counters, timing and physical removal. Schema 2 additionally requires a valid event layout, bounded tag string and typed active flag. Its active-tag list excludes empty/None and inactive observations. A complete capture with zero calls does not prove the call site. Expired ability/montage references, missing or nonpositive serials, and invalid indexes cannot prove an observed live reference. `combat_verified` remains false even for a complete trace. Host tests check report integrity and loader callback lifetime; they do not execute the Unreal hook.

The local candidate and verification hashes are recorded under `CCS/work/attack-call-probe`. Live registration, FFrame layout, calls, travel/weapon/shell cancellation, removal, coexistence and timing remain pending. Both loader and core must be installed together, after a fresh stopped-playing confirmation and with the game closed. The previously installed Probe 01 is unchanged while the user rests.

An independent read through the already-installed CSSX dev bridge confirmed that the current weapon item is Clockwork Scythe, the shell is Genessa, and one previously captured light-selector instance's `GetAvatarActorFromActorInfo` returns the current pawn. The getter's single eight-byte return signature was checked before calling it. Raw request/response IDs and object handles are retained in `CCS/work/attack-call-probe/player-reference.json`. This supports the next ownership check; it does not run Probe 03, observe a native montage call or prove every ability's ownership. No game settings, equipment or combat data were changed.

## Evidence needed before a swap

A later read-only CSSX capture resolved four current Scythe selector instances and 15 referenced primary ability classes through native spec-handle getters. The handle list remained at 137 entries across the capture; these separate queries were not atomic. The normal light selector references the same A3_Finisher class at combo position three and in AdditionalAttacks. The getter returned class defaults, with `bIsInstance=false`, for all 15 referenced attacks. Their A3_Finisher and B3_Finisher defaults contain only generic Primary/Melee.Weapon tags, without a finisher tag. The separately found A3_Finisher CDO agrees. Raw acknowledgements and observations are retained in `CCS/work/ability-ownership/slot-reference.json`. This contradicts using the extracted fixture's finisher tags or class names as live role authority, and supports recording event-time context before choosing a routing rule.

A follow-up read of each returned ability's `InstancingPolicy` gave value 2 for all 15 attacks. The retained UE 5.6.1 engine header declares value 2 as `InstancedPerExecution`. `GetGameplayAbilityFromSpecHandle` attempts `GetPrimaryInstance()` and otherwise returns the CDO, so this getter result does not prove the absence of execution instances. Capture-time execution instances and their tags still need native observations. Raw policy query IDs are retained in `CCS/work/ability-ownership/instancing-reference.json`.

Compare actual task calls with observed player grants and selector relationships. Confirm ActorInfo ownership and the attack's instancing policy, active weapon/shell generations, combo stage and slot. Verify montage skeleton/slot/notifies, payload and motion-warp requirements. Then validate a single controlled replacement and its interruption, death, travel, disable and restore paths. Probe 03 does not authorize or enable any of those writes.

## Live result 1, 27 September 2026 14:21 (schema 3)

Installed after a fresh stopped-playing confirmation with the game closed (loader `0952592390ba…`, core `ccs_core-1.0.0-4d5cd89924c2.dll`). The user pressed F7 in the world and performed several light and heavy attacks inside the 30 second window.

- Registration succeeded: the interface record shows the native function, 8 parameters in a 56 byte frame, the event-context layout (tag offset 232, active offset 904) and the current pawn and weapon item. No error record.
- The hook stayed registered for the full 30 s (`reason: duration`), then removed physically (`hook_removed: true`, `state: complete`).
- **Zero callbacks**: `seen 0, recorded 0, skipped 0, failures 0, maximum_callback_us 0`. Not a filter result: the callback never entered `observe` at all.
- Frame profiler over the same window (`work/attack-call-probe/run1-timing/`): 1,743 frames, loader callback mean 27 us, p99 35 us, max 2.0 ms; core mean 22 us; engine frame mean 17.2 ms. No CCS-attributed hitch.

Raw trace: `work/attack-call-probe/run1-attack-calls.jsonl`.

### Why the call was missed: evidence gathered offline

- Cooked bytecode of `GA_PlayMontageBase` (`work/movesets/export2/…GA_PlayMontageBase.uasset.json`, statement 632) calls the factory through `EX_CallMath`, and `GA_AttackBase_Melee` inherits that path (`GA_AttackBase_Melee -> GA_AttackBase -> GA_PlayMontageBase`). The task is then started with `GameplayTask:ReadyForActivation` through `EX_FinalFunction`.
- UE 5.6.1 `ScriptCore.cpp`: `execCallMathFunction` calls `Function->GetNativeFunc()` directly, and `CallFunction`'s native branch calls `Function->Invoke` without setting `Stack.CurrentNativeFunction`. Both read the UFunction's `Func` pointer.
- Pinned SDK `Unreal/src/UFunction.cpp:78-86`: `RegisterPreHook` swaps `Func` for `Internal::UnrealScriptFunctionHook`. `UFunctionStructs.cpp:161-176`: the thunk identifies the function from `CurrentNativeFunction` or, failing that, the pointer eight bytes behind the bytecode cursor, which is exactly the pointer `EX_CallMath` and `EX_FinalFunction` just read. On paper both call paths reach the hook; CSS's walk-speed pre-hook (`native/src/walk_override.inl:157`, native `SetMaxSpeedAdjustment`) proves the `EX_FinalFunction` path in production.
- No UE4SS warning ("no function map entry") was logged during the window, so the thunk either never ran for this function or the loader's own lambda dropped every call (wrong-thread check, root/serial identity check). Run 1 cannot distinguish these because the end record did not include the loader's `CcsHookStats`.

## Schema 4 (run 2 candidate)

Same read-only design with three loader-owned native pre-hooks instead of one, so a single capture separates the hypotheses:

| Slot | Function | Call opcode in the attack path | Purpose |
| --- | --- | --- | --- |
| task_factory | `AbilityTask_PlayMontageAndWaitWithNotifies:PlayMontageAndWaitWithNotifies` | `EX_CallMath` (static) | run 1's target, unchanged frame reads |
| ready_for_activation | `GameplayTask:ReadyForActivation` | `EX_FinalFunction` (context = the task) | alternative capture: reads `MontageToPlay`, `Rate`, `StartSection`, `StartTime`, `bStopWhenAbilityEnds`, `InstanceName`, `Ability` from the task object through validated reflected offsets; filtered to the montage task class and player-owned abilities |
| control_getter | `SpartaGameplayAbility:GetSpartaCharacterFromActorInfo` | `EX_FinalFunction` (27 call sites in `GA_AttackBase_Melee`) | counter only; proves whether any native pre-hook fires during attacks |

Each call row carries `source`. The interface and end records add `hooks` (per slot: function path, `func_before`/`func_after` pointer and `func_changed`, seen, matched, registered) and `host_stats` (loader `CcsHookStats`: calls, wrong_thread, failures, slots, running). Readings: `func_changed false` means the SDK did not swap `Func`; `host_stats.calls 0` with `control_seen 0` means the thunk never dispatched to CCS; `wrong_thread > 0` means the loader's thread binding is wrong; `control_seen > 0` with `task_factory.seen 0` means `EX_CallMath` calls bypass the swapped pointer on this build; `ready_for_activation.matched > 0` gives the montage data through the alternative point either way.

Report: `work/takeover/attack_report.py` accepts schema 4 and prints `sources`, `hooks`, `host_stats`, `control_seen` and `func_swapped`; 37 tests cover it.

## Live result 2, 27 September 2026 14:38 (schema 4)

Staged after a fresh stopped-playing confirmation with the game closed (loader unchanged `0952592390ba…`, core `ccs_core-1.0.0-0af6c12a37cc.dll`). F7 in the world, light attacks with the Clockwork Scythe for 30 s. Raw trace `work/attack-call-probe/run2-attack-calls.jsonl`, verifier output `run2-report.json`, profiler `run2-timing/`.

| Hook | seen | matched (player montage rows) | reading |
| --- | --- | --- | --- |
| task_factory (`EX_CallMath`) | 8 | 8 | every player attack montage reached the pre-hook with its frame readable |
| ready_for_activation (`EX_FinalFunction`) | 28 | 8 | same 8 montages read from the task object; 20 other GameplayTasks skipped by class/owner filter |
| control_getter | 2,423 | n/a | about 80 native pre-hook dispatches per second at 9 us worst case |

Loader `CcsHookStats` at the end: calls 2,459, wrong_thread 0, failures 0, slots 0 after removal. `Func` was swapped on all three functions (`func_changed true`) and the UE4SS log shows no "no function map entry" warning. Capture verified complete by `attack_report.py` (`capture_complete true`, `player_outer_calls_observed true`).

What the 8 rows show (all `rate 1.0`, section `Default`, event tag `None`, `bIsActive true`, fresh per-execution instance each time):

| # | Ability instance class | Montage |
| --- | --- | --- |
| 0 | `GA_Player_Attack_ClockworkScythe_A1_Hold_C` | `A_Shared_Attacks_Scythe_A_02_Hold_V2_Montage` |
| 1 | `GA_Player_Attack_ClockworkScythe_A1_C` | `A_Shared_Attacks_Scythe_A_02_V2_Montage` |
| 2 | `…A1_Hold_C` | `…A_02_Hold_V2_Montage` |
| 3 | `…A1_C` | `…A_02_V2_Montage` |
| 4 | `…A2_Hold_C` | `A_Shared_Attacks_Scythe_AA_Hold_V2_Montage` |
| 5 | `…A2_C` | `A_Shared_Attacks_Scythe_AA_02_Montage` |
| 6 | `…A3_Hold_C` | `A_Shared_Attacks_Scythe_AAA_03_Hold_60fps_V2_Montage` |
| 7 | `…A3_Finisher_C` | `A_Shared_Attacks_Scythe_AAA_03_Finisher_Montage` |

So a light press activates the stage's `_Hold` ability first (the wind-up montage) and the release activates the stage attack itself; the third stage of the normal light chain is the `A3_Finisher` class, matching the selector reference read earlier. Ability instances are `InstancedPerExecution`; 12 of 16 object observations had no engine weak serial at the callback (`unretained_calls 12`), so a swap must key on the ability **class** (or the selector slot), never on a retained instance.

### Conclusion for the swap design

- The native pre-hook on `PlayMontageAndWaitWithNotifies` is the swap point. The pinned SDK's thunk (`UFunctionStructs.cpp:205-283`) evaluates the caller's parameters into a fresh frame with a null code pointer, runs pre-callbacks, then calls the original with that frame, so the native reads `MontageToPlay` and `Rate` from the locals the pre-hook can overwrite. This is the same mechanism as CSS's walk-speed rewrite, now confirmed on the attack path.
- Cost model: one callback per swing (about 10 us) plus one per GameplayTask activation if the ReadyForActivation hook is kept; no per-frame work. The control hook shows even 80 dispatches per second are negligible.
- Run 1's zero remains unexplained (same loader, same registration path, no log warning). Treat any future capture with `seen 0` while `control_seen` is also 0 as "hook path not entered" and investigate before trusting a swap build.
