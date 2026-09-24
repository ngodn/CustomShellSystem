# CSSX extension performance standard (UE 5.6.1 + C++)

Rules for anything that runs inside CSSX (the core and every extension, native or
Lua). `performance.md` is the investigation log; this is the forward-looking
standard. It is grounded in how Unreal actually spends time, not in general API
theory.

## The cost model (why these rules exist)

An extension reaches the game only through the host bridge, and every bridge op is
two expensive things stacked:

1. **UE reflection.** Resolving a function or property by name
   (`GetFunctionByNameInChain`, `GetPropertyByNameInChain`) walks the class chain and
   hashes an `FName` at each level, and invoking a `UFunction` goes through
   `ProcessEvent`, which sets up a script frame and copies parameters. Unreal's own
   C++ guidance is to cache these: "instead of repeatedly calling `GetClass()` or
   `FindFunction()`, store results" and "reserve game-thread tick code for efficient
   pointer manipulation," not repeated reflected lookups.
2. **Heap allocation.** Each bridge op marshals JSON in and out, which allocates.
   Allocation feeds the garbage collector, and GC "can easily eat up a large chunk of
   your frame time and cause stuttering during gameplay" (Epic community C++ guide).

So a tick that issues many reflected bridge ops pays a chain-walk, a `ProcessEvent`,
and several allocations per op. That is the entire measured cost of the Cheat Menu
tick; nothing else in CSSX is close.

## The rules

1. **Cache every reflected handle; never resolve by name in a hot path.** The core
   caches resolved `UFunction*` and `FProperty*` per (class, name) with read-only
   serial validation (`engine.cpp`). Extensions must keep their own object handles
   across ticks the same way. This is the single most important UE C++ hot-path rule.

2. **Cache invariants; do not re-reflect data that cannot change during a session.**
   Shell definitions, class layouts, CDOs, asset paths are fixed for the process.
   Resolve them once and keep them. Re-fetching an invariant because an unrelated
   thing (the pawn) changed is the mistake `refresh_shells` made: a definition read
   per shell (up to 128 reflected calls) on every pawn change, a ~40 ms hitch. It now
   reads the catalog once per process.

3. **Cut the number of `ProcessEvent`/reflected calls per tick.** Each call is a
   script-frame setup, so fewer coarse calls beat many fine ones. Read a whole struct
   once rather than field by field; ask for a count before decoding a 110-entry list.
   The tick should be pointer manipulation over cached handles, not a burst of
   reflected calls.

4. **No heap allocation on the per-frame path.** The JSON marshal allocates, so a
   per-frame bridge loop is a per-frame allocation loop that pressures the GC. Reuse
   buffers, hold results, and keep hot data on the native side where no marshal
   happens.

5. **Event- or change-driven, not polling. Disable per-frame work you do not need.**
   Re-apply an effect only when its input changed (pawn identity, a setting), the same
   discipline as disabling `Tick` on an actor that has nothing to do. Poll only the
   cheap signal that tells you whether the expensive work is needed.

6. **Hot per-frame work lives native in the core, cached; the bridge is for coarse,
   occasional operations.** If something must run every frame, it belongs in the core
   with cached reflection, exposed to extensions as a finished result, never as a
   per-frame inner loop of marshaled reflected calls.

7. **Never touch the disk on the game thread.** Status, logs and frame data go through
   the background writer with `durable=false`; only an explicit user Apply may fsync.
   A synchronous write is an fsync stall under Proton while the game streams from the
   same disk.

8. **Budget every feature and measure it.** A periodic cheat should cost well under
   0.1 ms per tick at 10 Hz; any single tick over ~2 ms is a hitch that must be moved
   or justified. "Within noise on the median" is not enough: p99 and the worst single
   frame are what the player feels.

## How to verify

- `cssx.py request '{"op":"frame.stats","seconds":20}'`: per-phase cost (core_tick,
  extensions, hud, menu) with p99, max, and per-hitch attribution of the core's own
  microseconds inside each hitch frame.
- `cssx.py request '{"op":"library"}'`: per-extension `tick_us`/`tick_calls`, so one
  extension can be blamed. A healthy extension is single-digit microseconds per tick
  with no frame over ~2 ms.

## Sources

- Landelare, "Unreal C++ speedrun": cache `GetClass()`/`FindFunction()` results;
  reserve tick code for efficient pointer manipulation.
- Tom Looman, "Unreal Engine C++ Complete Guide": GC eats frame time and stutters;
  minimize `UObject` creation in loops; object pooling.
- Epic, game-thread CPU performance guidance: reflection and allocation on the game
  thread are the usual per-frame costs.
