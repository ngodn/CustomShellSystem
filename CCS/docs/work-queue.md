# CCS work queue

## Steering during a swing, 28 September 2026 (core 82e68d75c1c6, after the alpha)

MS2 lets the stick steer the wind-up of a swing, closes the gap to the target for you and lets
movement cancel the recovery. All three are notify states on every player attack montage, read
from the blueprints with `work/release-hardening/kismet_dump.py`:

- `ANS_RotateToFaceTarget` is the turn window (Axe and Dagger light 0 to 0.48 s, Martyr's Blade
  heavy 0 to 1.5 s, Hammer heavy 0 to 0.68 s; it closes where the first hit lands). Its tick
  checks `Game.State.Player` and takes a player branch: turn to the lock-on target, else to the
  targeting component's direction, else the last stick input, or camera forward
  (`InvalidTargetFacePolicy`). Turn rate is the weapon's `FaceToTargetInterpSpeed` when
  `bUseWeaponInterpSpeed`, else the character's. It writes through
  `SetOverrideManualUserRotation` on the movement component. The AI branch never runs on the
  player, so the notify is safe on enemy montages too.
- `ANS_SpartaMotionWarping_Translation` and `_Rotation` warp root motion to
  `WT_DesiredEndLocation` and `WT_DesiredRotation_Target`, both set in `GA_SpartaBase` (the base
  of every attack ability, the player's included). `ANS_MotionWarpToFaceTarget` and
  `ANS_MW_Attacker` are AI-only (they warp onto the attacker, `WT_LastAttacker`).
- `ANS_InterruptWithMovement` on the recovery tail lets the stick cancel the rest of the montage.

What changed (`src/core/combat.cpp`):

- The cleaned copy of an enemy montage no longer drops `RotateToFaceTarget` or the game's own
  warps. It drops `MotionWarpToFaceTarget`, `MW_Attacker`, `ReinitializeWarpTargets`,
  `AlignHeightToWarpReference` and the rest of the old list.
- The hold carry became a window carry: with "Move's own" feel, a replacement that lacks a turn
  window gets the original's `RotateToFaceTarget` rows appended, opening where the original's
  did (scaled to the clip) and closing where the replacement's first hit window ends (or at the
  same fraction of the clip when it has no hit window). Hold rows keep their absolute timing as
  before. One clone per original, cached in `Slot::carries` (was `holds`); the log line is
  "CCS windows carried: A -> B: hold x to y s, turn x to y s". `MontageFacts` gained `turn`,
  `Slot` gained `play_turn`, `first_hit_span` returns the first hit window's begin and end.
- "Game's" feel is unchanged: the original's rows already stay on the transplant.

Armor option (same day, core 80ee120c6b56). Getting hit cancels a swapped move exactly as it
cancels the player's own attacks: the game's hit reaction interrupts any attack unless the
character carries `State.HyperArmor`, which `ANS_HyperArmor` grants through `GE_HyperArmor`
(infinite duration, one stack, removed on NotifyEnd; `GE_HyperArmorIgnoreImmunity` strips it for
the attacks that break armor). Player montages author it on the Martyr's Blade heavies
(`ANS_HyperArmor_C`, no required tag) and on the Axe and Dagger lights only through the
Unyielding upgrade (`ANS_AddGameplayEffectConditional`). Enemy montages keep their own rows on
the cleaned copy (the Sicario NeverEndingCombo has three windows, 1.79 to 3.70 s, 4.51 to 8.31 s
and 8.86 to 11.85 s) but the first 1.8 s and the gaps have none. New per-slot row "Armor":
"Hyper armor (cheat)" (the default, at the user's request) or "Move's own". With the cheat, whatever the slot plays
(replacement, transplant or carry clone) goes through one more clone with the donor row from
`AM_Shells_MartyrsBlade_B1` appended over the whole length (`Combat::build_armor`, cached per
source in `Slot::armors`, donor kept referenced and reloaded after a world change). The row
copy shared by the carries and the armor is `Combat::append_rows`. Saved in settings and
presets as `armor`.

Steer option (same day, core fe516af5b49f). Long enemy moves bring their own short turn windows
(the Sicario combo has ten, all with the notify's defaults: target sampled once, fixed interp
speed) and no movement cancel, so the player is locked for the whole clip. New per-slot row
"Steer": "Whole move" (default) or "Move's own", shown with "Move's own" feel only ("Game's"
feel already carries the weapon's rows). The armor clone became the player-feel overlay
(`Combat::build_overlay`, cache `Slot::overlays`, donor rows `donor_armor_`, `donor_turn_`,
`donor_cancel_` from `AM_Shells_MartyrsBlade_B1`): the donor's `ANS_RotateToFaceTarget` (weapon
interp speed, continuous target) from the first frame to the last hit's end, then the donor's
`ANS_InterruptWithMovement` from there to the end so the stick cancels the recovery the way it
cancels the player's own attacks. A move without hit windows steers to the end and keeps its
recovery. The move's own turn rows stay and tick alongside. Saved as `steer`. Changing Armor or
Steer drops the slot's overlay cache. The user chose "steer all the way, cancel the tail" over
"cancel any time after the first hit".

Weapon state carry (same day, core 6506f3c04dcb). The Axatana joins into the axe for heavies
and splits for lights through instant notifies on its own montages: `AN_Axatana_Transform_ToKatanas`
at 0.03 to 0.05 s on the light `_Hold` montages and the katanas running attack,
`AN_Axatana_Transform_ToAxe` at 0.2 to 0.4 s on the axe hold and running attack and inside the
0.5 s companion transform clips. The notify's `CommitTransformation` removes the weapon from the
slot and swaps the item stacks, so it is a real item swap, not a mesh toggle. With "Move's own"
feel the replacement lacked the notify and the joined axe stayed in hand on the next light.
`Combat::row_kind` now classifies rows as Hold, Turn or State (`Transform_To`,
`SetWeaponEquipState`, `HandleWeaponsEquipState`, which also cover fists on Gragu and the seal
parries) and the carry clone takes the original's State rows at their absolute time, instant
rows staying instant. `MontageFacts::state`, `Slot::play_state`. Enemy cleaned copies still drop
the enemy's own equip-state notifies.

Elemental trigger carry (same day, core 96717f77f7d0). Audit of the 185 player attack montages
for rows a "Move's own" replacement loses: besides the carried hold, turn and weapon-state rows,
the only gameplay row is `AN_TriggerElementalMechanic`, which the charged montages fire a few
frames after each hit (`Event.TriggerElementalMechanic` with mechanic, slot and spawn data;
`GA_ElementalMechanicHandler` reads only that payload). That is how elemental Tarstones fire
on charged attacks. It is now `RowKind::Mechanic` and goes with the replacement's hit of the
same rank; a replacement with fewer hits keeps only the original's last-hit triggers on its own
last hit. `hit_begins` merges the paired weapons' two rows per strike. Row kinds are now a
bitmask (`MontageFacts::kinds`, `Slot::play_kinds`, `bit(RowKind)`). Left alone on purpose:
`CSAnimNotifyState_SetCameraState`, camera shakes, ground impact and audio (cosmetic),
`AnimNotify_PlayMontageNotify` names such as Activate (consumed by Tarstone weapon abilities and
plunges, not by the slot attack abilities), the Scythe's `ANS_ClockworkScytheChain` and
`ANS_ChainsawHitCheck` (the Clockwork Tarstone's chain hits, a known limit).

Alpha.2 hardening (same day, core 64487718d553). Two reviewers over the new code and the rest:
- settings.json was written synchronously on the game thread on every row change, and a held
  direction key changes a row every 90 ms. `Core::save_settings_or_log` now only collects the
  state and marks it dirty; `Core::flush_settings` writes once 400 ms after the last change
  (from the status phase of the tick) and on `Core::stop` (core swap, unload).
- `Core::model` copied the whole candidate list into all thirteen slot controls on every rebuild
  (every event increments the model revision). The list now lives once on the customize section
  as `candidates`; each slot control carries `hint` (the assigned move's status) instead of
  `options`. The menu reads the section's list (`build_slots`, `Menu::act` pick/activate).
- `ensure_donor` could do a blocking asset load from inside the swing hook the first time a slot
  wanted armor or steering (and again after a world change). The donor now loads from
  `load_pending` in the tick as soon as any assigned slot wants it; the hook only uses it once
  `donor_ready()`, and a swing before that plays without the overlay.
- A slot whose montage stopped answering cleared only its game-feel cache on reload; carries and
  overlays (all built from the lost montage) are now released too, with their references dropped.
- Switching Feel also drops the overlay cache (an overlay built without steering could otherwise
  be served after a switch when the transplant fell back to the replacement).
- The focused setting's text moved out of the scrolling rows into a fixed 230 px box under them.

Still to verify live: an enemy move with "Move's own" feel turning with the stick during the
wind-up; whether the enemy's own translation warp overshoots on long lunges (if it does, add
"SpartaMotionWarping_Translation" back to the drop list, or gate it per slot).

## Release hardening, 28 September 2026 (commit 1ccf3c8, core e4465f86f757)

Four reviews before the alpha (hot path, menu, core and runtime, and a comparison with the CSS
and CSSX sources). None found a crash or memory-safety defect. What changed:

- Hold mechanics, read from the blueprints (`work/release-hardening/kismet_dump.py` over the
  CUE4Parse exports): the handler's NotifyBegin needs the unlock tag, the `Input.Attack.<x>.Hold`
  tag and the resolve cost, then `BPC_Player_ComboCounter::StartHoldAttack` sets the montage to
  the holding play rate and starts two timers, minimum (0.5 s) and maximum (1.05 s). Release
  inside the window (NotifyTick without the input tag) or the window's end calls
  `StopHoldAttack`: held time within the thresholds fires the success event (normal-first:
  the `_Hold` selector; hold-first: none, the montage just continues), otherwise the fail event
  (hold-first: the normal cut). The maximum timer fires the success on its own. So the carried
  window keeps the original's absolute start (commit 463aca1); scaling it to the replacement's
  first hit delayed the charge by 0.4 s on the Axatana and a normal hold hit the fail path.
- Charged attacks are Tarstones: the Acolyte's Stone (light, `Melee_LightHoldAttack`) and the
  Unwieldy Stone (heavy, `Melee_HeavyHoldAttack`) apply `GE_Unlock_Attack_Hold_Light` / `_Heavy`,
  which grant `Character.Unlocked.HoldAttack.*`; the charge also costs Resolve. The user's character had neither (verified live
  through `HasMatchingGameplayTag` on the pawn with a control tag), which is why every long
  press ended in the normal attack, in vanilla too. Commit 163c32d: locked hold abilities are
  left alone, the LC/HC hint and the status line say the slot waits for the upgrade,
  `status.json` has `combat.hold_unlocked`, and the attack trace carries timestamps.
- Settings: "Charged attacks", Need a Tarstone or Always (cheat). Always applies the two unlock
  effects to the player's ability component (`Combat::sync_hold_cheat`, `BP_ApplyGameplayEffectToSelf`
  with `MakeEffectContext`), re-applies on a new component, removes them on switch-off and in
  `Combat::stop`. Verified live through two core swaps: tags appear and the heavy one goes away
  again (light stayed because the user's Acolyte's Stone was equipped). Saved as
  `charged_attacks_without_tarstone`.
- Classifier coverage: 165 `GA_Player_*` classes in the export; the Duality Stone doubles now
  map to their step. Still unmapped on purpose: the Scythe Grinder, Fists Smert memory steps,
  the plunging attack, executions, and the abstract bases.
- Tick phases fail on their own (`Core::phase`): player lookup, combat, asset scan, menu,
  status. A failure is logged when its message changes, retried after a second, and the phase
  is switched off after sixty failures in a row. Before, any exception in `Core::tick` stopped
  the whole mod for the session (the loader's `dispatch_failed_` still exists as the last resort).
- 5.6 liveness (`engine::item_alive`, Unreachable or Garbage) now also guards the serial
  initializer and the loader's hook host; the SDK's `IsValid(false)` reads bit 29 as PendingKill.
- The player controller is asked for at most four times a second (`player_context` caches the
  world and controller); the pawn and ability component are still read every frame.
- `Call::param` matches by FName (`FNAME_Find`), no string per parameter. The asset scan
  compares the class FName. The hook resolves the notify layout once (`notify_layout`) and keeps
  per-montage facts (companion clip, hold handler) and a per-slot `play_hold` flag, so a swing
  costs no property lookup and no name text.
- Menu: one cached candidate list shared by all thirteen slots (`candidate_options`, rebuilt
  when the scan changes it); the four reflected calls in tick are guarded; the tab and page
  pairing is checked once a second and the tab re-attaches through `detach()`; the search text
  is read every frame only while typing; attach and warm-up failures log once; `act("run")`
  no longer copies the model. Presets are listed every ten seconds while the tab is open.
- The log rotates at four megabytes (`ccs.jsonl.1`). `status.json` carries `timing` per phase
  (average and worst microseconds since the last write). Measured with the Player Menu closed:
  about 45 microseconds a tick in total; the menu warm-up build is one 23 ms frame.

Not done, noted for later: `runtime/persistence.cpp` and `content_monitor.cpp` are compiled
into the static runtime but unused by the core (the linker drops them); `menu_layout.hpp`,
`input_edges.hpp` and `visible_window.hpp` are exercised only by tests; `settings.json` still
carries unused fields (`startup_preset`, `preserve_weapon_mesh`, `show_hud_notification`,
`attack_speed_scale`, `damage_scale`). The first swing per original montage still builds its
clone inside the hook (watch `maximum_callback_us`). Duality Stone double variants
(`_A1_Double` and so on) match no slot and play untouched. The presets folder was removed at
the user's request.

## State on 28 September 2026, night (v1.0.0-alpha.1)

Core `ccs_core-1.0.0-alpha.1-8208485836d6.dll` is live (commit 40b9144). Thirteen slots (the
sprint pair S+L and S+H joined on the 28th); the sidearm slot R is parked.

- Long presses now charge with "Move's own" feel. Hadern's-style weapons decide a charge inside
  a hold-handler notify state on the normal swing (`ANS_HoldAttackHandler`, window about 0.24 to
  0.52 s, hold 0.5 to 1.05 s at play rate 0.2, fires `Event.Attack.Selector.<x>.Hold`); hold-first
  weapons run `ANS_HAH_*` on the `_Hold` montage with a fail event that starts the cut. A
  replacement without that notify could never charge or fall back. `build_hold_carry` clones the
  replacement and appends the original's handler rows with the window scaled first hit to first
  hit (else length to length), links re-pointed, absolute link method. Cached per original in
  `Slot::holds` (four deep), logged once as "CCS hold window carried". The handler requires
  `Character.Unlocked.HoldAttack.Light/Heavy` on the character; whether the current save has it
  is unverified, so test a hold with no slot assigned first.
- `status.json` `combat.recent` lists the last 24 player attacks the hook saw with the slot and
  what happened (swapped, nothing assigned, no slot, companion clip, rig). An empty slot logs once:
  "CCS attack seen: <class> is slot <S>, nothing assigned". Use this to read the L2 then H2 report.
- Sidearm slot parked behind `sidearm_slot_enabled = false` (`ccs_types.hpp`): the classifier
  never returns R, the tile is hidden (`hidden` on the control, grid rows six wide), presets skip
  the R entry. Everything stays in the code for later.
- Crash retest this evening, nothing reproduced: sidearm draw, stow and slingshot fire pass
  through untouched; a Genessa astral double running the player's own sword abilities was skipped
  by the owner guard on every swing; five sprint-light swaps with the cleaned Sicario copy, zero
  failures. Death into the Harbinger or Stray Genessa is still untested by the user.
- The working tree shows `CCS/presets/*.json` and one enemy icon deleted (not by the tool;
  `ccs.py` only copies presets). Restore with `git checkout -- CCS/presets` if that was not meant.

## State on 27 September 2026, evening (v1.0.0-alpha.1)

The product build is live and verified in play: the native page (CSSX recipe, CSS icons and
banner) drives the combat engine through the montage-task pre-hook, and swapped light attacks
were seen in the user's recording. The loader switches cores live from `core.json`. Eleven
slots (L1 L2 L3 LF LC, H1 H2 H3 HF HC, R). Candidates: player moves (catalog.json), enemy moves
on the human rig (enemy-catalog.json), sidearm fire (ranged-catalog.json), plus new montages
from the running game's Asset Registry. Search field, grouped headers, tile icons, details
window, presets and settings pages all work. `ccs.py release` writes the player ZIP.

Latest changes (this evening):

- Registry scan filters by the montage's `Skeleton` tag before offering a candidate, through
  `AssetRegistryHelpers.GetTagValue`. Live result: 264 creature-rig montages dropped, 187 real
  new candidates, no untagged montage. Rule in `src/core/rig.hpp`: the game's human skeleton,
  any skeleton under `/Game/CSS/`, or the exact skeleton of the body worn now (the live pawn
  reported `/Game/CSS/Shared/SKEL_Base`). The same rule guards the load step.
- The scan also queries `/Game/CSS`; it returned zero rows because pak mods are not merged into
  the game's Asset Registry. CSS package animations can therefore only be offered through the
  catalogs, not discovered.
- `CCS_VERSION` now carries the full string (`1.0.0-alpha.1`); `project()` had been
  overwriting it with the numeric part, so status.json and the About row said 1.0.0.
- Release packaging: `python3 CCS/tools/ccs.py release` builds and writes
  `dist/ccs-v<version>/MSII-CCS-v<version>.zip` (extracts to `ue4ss/Mods/`) with
  `CCS/release.json` checksums, a `.sha256` beside it, and a member check that refuses logs,
  runtime state, presets, settings, PDBs and probe files. Player text in `packaging/`.

Later the same evening:

- Search shortcut: `IA_Menu_Confirm_Tertiary_Press` (the third face button and its keyboard
  key) focuses the search field; the prompt strip shows "Search the list". Live key names are
  read from the game's mapping when the menu opens; confirm the glyph in play.
- Presets: choosing one in the Preset row applies it (Load does too). Moves resolve by id or
  montage path, misfits are skipped and counted in the status line, every slot is set, and the
  Customize page rebuilds. The four Gemini presets were fiction (montage paths that do not
  exist) and are removed from the repo; the copies in `Mods/CCS/presets/` still exist until
  the user deletes them. `tools/preset_check.py` validates and lists;
  `docs/presets-agent-prompt.md` briefs an agent to author fifteen real presets.
- Damage facts (exports): each hit-check notify inside a montage owns its own hit payload, so
  a swapped swing uses the replacement's multiplier, poise, reaction and timing with the
  equipped weapon's base damage; enemy payloads resolve their base from character data,
  unverified for the player.

Per-slot settings (evening, after the user's screenshot): each slot's list now starts with three
rows, "Speed", "Hit damage" and "Weapon in hand", reached with Up/Down like any candidate; Space
cycles, A/D or left/right adjust, and they save into settings.json (`slot_tuning`) and into
each preset entry (`speed`, `hit_damage`, `weapon`). The global Attack speed slider is gone.

- Hit damage "weapon": in the pre-hook, the slot's original montage's first hit-check payload
  is copied field by field (multiplier, poise, break, reaction, effects, tags) onto every
  hit-check payload of the replacement montage, with a backup restored on clear, mode change
  or core stop (`Combat::apply_weapon_payload`). Timing and trace stay the replacement's.
- Weapon in hand "move": the held weapon actor's `SM_Weapon` static mesh is set to the move's
  weapon mesh at swap time and put back (mesh and materials) once `Montage_IsPlaying` reports
  the montage over, polled only while a swap is shown. Mesh table in
  `Combat::weapon_mesh_path` (player primaries from the WP_ blueprints, enemy static meshes
  from the icon agent's list; skeletal enemy weapons such as the Grisha bow have none).
- Audio counters: native pre-hooks on `SpartaWeaponComponent_Melee:StartWhooshFX` and
  `SpartaCharacterVoxComponent:OnCharacterAttack` count calls into status.json
  (`whoosh_calls`, `vox_calls`). The research (`ANS_WeaponFX`, `AN_Vocalization`, WaveWeaver
  audio) says replacement montages do carry whoosh notifies that resolve against the player's
  own weapon, so silence means the notifies are not running; a swing with and without a swap
  will show it in the counters.

Night of 27 September, verified in play (hundreds of swaps, zero failures, nothing dropped):

- **Keep-alive.** On 5.6 the collector takes roots from a private index set, so the SDK's root
  flag protected nothing and unreferenced enemy montages died within seconds (logged as root and
  unreachable). Everything CCS loads now sits in `UWorld::ExtraReferencedObjects`; a world change
  reloads the slots. Liveness reads 5.6's flag layout (bit 29 is RefCounted).
- **Game feel.** A runtime clone of the slot's own montage carries the move's animation
  (segments rebuilt through the engine allocator, `FAnimSegment::bValid` set by hand, time-warped
  so the first hit aligns, links re-pointed, outer = the original). Default feel is Move's own.
- **Focus model.** Slots (default), list, settings; R3 or Tab enters the settings; prompts and
  highlight follow the focus; search keyboard only. Window: fixed-height description under the
  rows, control guide in the page footer, status centred under the tiles, focus frame on rows.
- **Sprint slots** S+L and S+H (running attacks, 16-move catalog from the ability exports).
- Companion clips (transform, equip, draw, stow) an ability plays around the attack are left
  alone by name; an exact catalog match blocked every swing after a fresh start because the
  attack montage varies with the shell.
- Per-slot defaults: 1x, Move's own feel, Move's own damage, My weapon.

28 September, crashes reported (death into Harbinger or Stray Genessa; aim then fire):

- The game's crash records are all "GPU crash" (device removed); the dumps show the game thread
  waiting on the renderer and no mod code executing, so the damage is data submitted earlier.
  The log lines that ended each crashed session were the Genessa sever animation and the
  Slingshot sidearm attack, both left alone by CCS.
- Hardening shipped: no swap while the body worn is not on a human-family rig (Harbinger and
  creature shells); every built segment checked for finite sane rates; enemy and unverified
  montages play through a cleaned copy without AI notifies (motion warping, rotate to target,
  warp re-init, equip state, AI events, mesh offset), logged as "cleaned copy of ..."; swap
  lines carry feel, damage, visual, speed and rig; payload copies and weapon mesh swaps are
  logged. The slot validator is removed (any move in any slot; skeleton is the only gate).
- Crash analysis tooling: `work/crash-analysis/tools/bin/minidump-stackwalk` (rust-minidump),
  dumps under the Proton prefix `compatdata/2584270/.../MortalShell2/Saved/Crashes`.

## Open items

- Confirm the two crash scenarios no longer reproduce; if one does, the log now shows the
  exact swap, feel, visual and rig state before it.
- L2 playing H2's move: reproduce with only H2 assigned; the "CCS swap:" line names the class
  and slot the game actually called.

- After a full game restart, one swing to confirm the swaps run on the fresh world (a silent
  skip streak was seen once right after a restart, before the companion-clip rule).

- Sound on swapped swings: read `whoosh_calls`/`vox_calls` after a vanilla swing and after a
  swapped one. If swapped swings never count, drive the whoosh from CCS at the hit window
  (`StartWhooshFX`/`StopWhooshAudio` on the held weapon's melee component) and broadcast the
  vocal through the vox component; if they count but stay silent, prime the source weapon's
  sound banks (`USpartaSoundBank::PrimeBank`) at slot load.

0. Live check of the three per-slot settings (built, not yet seen in play): speed on a swapped
   and an unswapped slot, hit damage "weapon" on an enemy move (damage numbers), weapon in hand
   "move" on the Hammer L3 (mesh appears for the swing, own weapon returns, materials intact,
   interrupt and death restore it). Katana and Axe & Dagger left-hand meshes are not swapped.

1. Play test of the R slot with a sidearm move, melee slots in a real fight, preset round trip,
   search, tile icons and the 11th tile's layout. Slot R still holds the Batman shoot montage
   the user assigned before the filter existed; it reports the rig error until cleared.
2. Catalog gaps: nine player abilities are unresolved in `catalog.json` (AxeDagger and Katana
   double attacks, Scythe A3/B3 finishers, Martyr's Blade B3 hold), so those moves cannot be
   chosen. Three enemy-catalog entries are absent from the registry (`MS1_TwinSisters` crossbow
   montages); they show as "Not in this game version". Confirm whether the cooked files exist
   in the paks or drop them from the catalog.
3. Performance comparison, CCS on versus off, in the same fight and menu scenarios, with the
   optional frame profiler (`docs/frame-profiling.md`). No FPS claim has been recorded yet.
4. Enemy weapon icons: produced by another agent from `docs/enemy-icons-agent-prompt.md` into
   `assets/enemy-icons/`; staging and the release pick them up automatically.
5. Tarstones as a real feature (the placeholder group was removed).
6. Menu build cost: last build 7 ms, worst 19 ms on a full rebuild. Acceptable for a page that
   rebuilds only on model changes; revisit if it shows in the profiler.

The CSSX source tree still carries an uncommitted CCS-aware attach change
(`extensions/core/src/core/menu.cpp`) from the Codex session. CCS attaches last precisely so
CSSX never needs it; it is not part of CCS and should be dropped or shipped separately.

## Takeover history (Codex session, 27 September morning)

Current priority: finish the verified runtime foundation before enabling combat or the menu.

## Completed during takeover

- Reviewed both Gemini implementation commits against CCS docs, CSSX, CSS, and the retained SDK.
- Reproduced and fixed writer flush deadlock and early completion.
- Removed the embedded move list and then removed the extracted snapshot from runtime initialization. A bounded loaded-selector reader now publishes observations in the experimental menu; eligibility checks remain unfinished. Snapshot slot mapping now uses exported hold flags and finisher tags, never class-name suffixes.
- Added portable runtime, catalog and staging tests.
- Added ABI size checks, callback lifetime protection, passive initialization and CPU-only destruction.
- Removed per-tick blocking montage loads. Combat availability is reported as false.
- Corrected root ownership and removed the path-keyed weak asset cache.
- Added bounded file reads, transactional settings parsing, schema and filename validation, atomic file replacement and preset origin round trips.
- Recorded the latest screenshot and kept the unfinished menu behind an explicit build option.
- Implemented paired tab/page permutations, construction rollback, menu generation checks and retry backoff. CSSX source resolves its own updated paired index when opening. These changes are not installed or live verified.

## Next implementation steps

Current step: [loaded combat probe 01](discovery-probe-01.md) is prepared; installed with fresh stopped-playing confirmation; the first live capture verified 137 grants and four Clockwork Scythe selectors. [Probe 02](discovery-probe-02.md) prepares runtime tags and narrow Asset Registry controls in a separate build; live verification remains pending. Continue implementing [live runtime discovery](runtime-discovery.md). The generated catalog is a research snapshot/test fixture, not the runtime authority. Read current selector/ability/item relationships, discover unloaded candidates through the validated Asset Registry, and invalidate metadata caches after content patches. This is an immediate correction to the current design, not a separate later feature.

1. **Done 27 September 14:38.** [Probe 03](discovery-probe-03.md) run 2 observed every player attack montage at the native `PlayMontageAndWaitWithNotifies` pre-hook and at `GameplayTask:ReadyForActivation`, with the loader's hook counters clean and 9 us worst-case callback. The pre-hook frame rewrite is the confirmed swap mechanism. Former text: Probe the native montage-task call site in game. The [read-only probe 03](discovery-probe-03.md) is compiled locally, with bounded capture, reflected layout checks and deferred-removal handling. Its physical Outer filter does not prove ASC grants, ActorInfo or attack roles. Live registration, frame layout, observed calls, removal and timing remain to be verified. The [loader-owned native pre-hook service](native-hook-service.md) guards thread and callback lifetimes.
2. Resolve ability, weapon, pawn and CharacterId generations. Map selector stages to slots without object-array scans. Confirm instancing policy, double-attack variants, weapon transformations, Harbinger/severing and shell changes.
3. **Next.** Validate one player-to-player montage swap through a pre-hook rewrite of `MontageToPlay` (and optionally `Rate`) keyed on the owning ability class, with the mapping read from a JSON file under `Mods/CCS` so the test can change without restaging. Cover original capture, owner identity, interruption, death, travel, reset and disable. Avoid shared montage/notifies edits.
4. Extract enemy montages and skeleton references. Check skeleton compatibility, animation slot tracks, motion warp targets, hit trace weapons, payload sources and combo/hold windows per move. Add only verified moves; never whitelist an entire archetype by name.
5. Resolve Tarstone ownership, levels, weapon compatibility, unlock requirements and live effect semantics. Preserve the base game's Resolve costs and restore every override. Catalog stats alone do not grant effects.
6. Rebuild the UI using the [latest screenshot](ui-reference.md), CSSX widget pooling and bounded work. Fix initial attach, Inventory, Tarstones, Map, optional CSS, CCS, optional CSSX ordering, native focus/input coexistence, open/close, travel/reopen and empty states. Replace static sample rows, values and inspection cards. Apply UI checks during implementation under the user's subsequent autonomous-work instruction.
7. The [asynchronous file foundation](persistence-runtime.md) handles bounded preset/settings requests and results. The [preset name field and save/delete confirmations](preset-menu-runtime.md) are implemented with native UMG primitives and explicit worker-side replacement protection. Validate keyboard/gamepad/mouse focus, short clicks, native input coexistence and final widget styling. Complete verified current-weapon vanilla reset. Keep disk operations out of navigation and combat.
8. The optional [diagnostic profiler](frame-profiling.md) now records loader cadence/dispatch and core phase costs with bounded buffers and background reporting. Compare CCS disabled/enabled against CSS/CSSX coexisting in the same combat, travel and menu scenarios. Record mean, p95, p99, maximum and attributed hitches before making performance claims.
9. Add safe core switching, release packaging, runtime pin reporting and cold-start verification. Stage only after the user explicitly confirms they stopped playing.

New notes during takeover: avoid Gemini's hard-coded content, use the latest screenshot in Pictures, and dynamically read current game data with patch-aware validation. All are incorporated above.

Tab-order correction from the user: CCS follows CSS and precedes CSSX when present. With CCS alone, place it immediately after Map. Apply this to insertion, reopen, travel and coexistence checks.

Autonomous-work instruction: the user asked to continue while they nap, without keyboard assistance. A goal tracks the complete takeover. Continue code, tests and available read-only evidence; do not stage or reload a running game. Synthetic desktop F7 input has not produced a capture, so it is not a verified control path. Existing CSSX `menu.open` requests can open the native menu; returned success is checked separately from visible/menu diagnostics.

Tab hosting foundation: `engine::reorder` now validates permutations, keeps children rooted during reparenting, restores HorizontalBox slot styles and checks the resulting order. CCS orders paired pages and buttons together, preserves the selected page, retains widgets across menu closure, and checks menu/controller replacement every 500 ms. Offline tests cover all 984 permutations across optional CSS/CSSX combinations, reopening and foreign tab pairing. Eight host suites pass normally and under ASan/UBSan. Experimental CCS menu and CSSX development builds compile. Native runtime behavior still requires stopped-game installation and a later live check.

Snapshot correction: exported `is_hold` and finisher gameplay tags replace class-name guesses within the offline fixture only. The fixture has 107 classified moves, with two Scythe additional attacks excluded from finisher mapping because their exports lack the required tags. Missing and conflicting roles stay unresolved. The parser validates extraction provenance without pretending a fixed build string detects the running installation. Later current-game queries found no finisher tag on the returned Scythe A3/B3 finisher class defaults or the separately checked A3 CDO, so fixture classification must never supply live slot authority.

Performance reference note from the user: compare CCS directly with the current lightweight CSS and CSSX implementations. Keep their useful patterns: guarded reflection caches, retaining widgets, bounded creation per build, mapped input/device filtering, asynchronous file writers and publication of completed CPU results. Verify each borrowed pattern against CCS lifetime and combat requirements before using it.

Input foundation: [live mapped menu input](menu-input-runtime.md) now dispatches native action roles using current applied keys, bounded reflection and tested edge/repeat handling. No fixed key fallback or numeric button lookup remains in the input dispatcher. Text prompts, device filtering, typing, mouse and native focus/trigger behavior remain pending.

Rendering foundation: [retained menu pools](menu-pooling.md) now bound creation, retain screens/cells, update changed values and limit browser/preset rows. Fake sample weapon/enemy rows and inspection stats are removed. Slot-filter views avoid record copies during navigation. Final native widgets, live item data, device glyphs and pointer/focus behavior remain unfinished. Centered uniform aspect-ratio fitting and naming/overwrite UI are implemented but still need live checks.

Loaded-data foundation: [current player selector observations](loaded-moves-runtime.md) now run only on the visible CCS page, read one grant per step, validate world/player/item/shell and grant identities, and publish CPU-only immutable snapshots. Runtime startup and staging no longer use the extracted catalog. Observations have zero combat eligibility until role/ownership/compatibility checks pass. The new reader has not run in the game; full discovery, instance overrides, transformed weapon state, Tarstone ownership and patch identity remain unfinished.

Timing foundation: separate diagnostic builds arm 30-second/16,384-frame captures through F7. Raw CSV and summary JSON retain failed samples, phase masks, mean/p95/p99/maximum and preceding/current callback association for large intervals. Sorting and file publication happen on the worker. Normal builds explicitly disable the diagnostic option. Live baseline/coexistence comparisons and profiler-overhead measurement remain pending; no FPS guarantee is claimed.

Native hook foundation: a typed host capability is available; the old untyped ABI field remains unused. Native-only pre-hooks keep loader closures permanent, use guarded function identities and acquired roots, clear core function/user pointers before unregistration, and report deferred removal. Passive/menu/discovery builds register no native attack hooks. Probe 03 adds an explicitly armed observation consumer, not a replacement. It is not installed. Next priority is live call-site and ownership evidence, then the verified player-only swap and full restore lifecycle.

Probe 03 foundation: a fixed 64-row callback buffer, four-row tick drain, 30-second window, 250 ms retry and output-drain checks follow CSS/CSSX's bounded work and asynchronous publication patterns. Trace verification rejects missing terminal records, sequence gaps, bad counters, cleanup failure, timing overruns and unsupported compatibility claims. Ten host suites pass normally and under ASan/UBSan, including 27 Python tests. Default and diagnostic attack Windows builds compile. These checks do not establish a live engine contract or FPS result.

Failure-path performance: loader logging now queues bounded writes to a worker. Unexpected core tick/hotkey failures pause normal dispatch, log once and retry game-thread cleanup at 250 ms intervals. The API pointer remains available for shutdown, and unload continues to require quiescence. Engine lifetime/cleanup behavior still needs live verification.

Preset flow: the experimental menu now has a stable name editor and save/delete confirmation buttons. The editor shares the four-widget page creation budget; typed names are read only when saving. First writes cannot overwrite existing presets, and the worker returns a conflict for explicit confirmation. Unique, exclusively-created temporary files prevent simultaneous writers sharing a scratch file. Host tests verify one winner among eight concurrent creators, unchanged bytes after rejection, confirmed overwrite and cleanup. Live read-only CSSX queries verified the native primitive signatures and caught the lowercase `content` parameter in `Button.AddChild`. UI rendering, focus, click-through and Windows publication behavior remain unverified; no UI release or FPS result is claimed.

Aspect-ratio correction: page controls and fonts now share one centered fit scale from current native local geometry, replacing separate width/height stretching. Eleven host suites pass normally and under ASan/UBSan, including the new layout invariants across nine page shapes. These mathematical checks do not establish rendered readability, DPI behavior or native input correctness.

Installation change foundation: the experimental menu now uses a bounded background metadata monitor, clears loaded observations after inspection failure and latches restart after executable/package-manifest changes. Request/poll never wait for disk inspection, and stop waits for worker quiescence without joining the scan on the game thread. Twelve host suites pass normally and under ASan/UBSan. The same portable reader observed 84 actual installation records without changing game files. Mounted content, executable/container digests, strong cache identity, Windows live behavior and performance comparison remain pending. See [content monitoring](content-monitor.md).

Instance discovery correction: the reader now captures native spec handles and both instance lists, prefers live selector/ability overrides, preserves duplicate grants/execution instances, and reads/verifies one candidate per tick. Default fallback records remain explicitly unverified metadata. Completed snapshots stay visible during refresh with a refreshing label and are discarded after detected identity changes or failures. A fresh CSSX read-only query proved the current Scythe light selector resolves from spec handle 128 to its instance and current ASC/pawn. This does not prove attack-task calls or combat replacement. The rewritten reader is not installed; weapon transformations, full event-time ownership, slot role/compatibility and live timing remain pending.

Slot evidence correction: a second read-only capture verified four current Scythe selector instances and 15 referenced ability classes. The normal light selector uses the same A3_Finisher class in combo position three and AdditionalAttacks, while current finisher ability tags do not identify a finisher role. Probe 03 schema 2 now records the owning ability's current event tag and active flag with prevalidated reflected layout and guarded owners. These facts may help distinguish activation contexts after a native capture, but no event tag, class name or fixture entry currently grants combat eligibility. The candidate remains uninstalled; actual calls and event-time ASC/ActorInfo/selector ownership still need verification.

Attack instancing evidence: all 15 current Scythe attack defaults report InstancedPerExecution (native enum value 2). The spec-handle getter returned defaults for those attacks, while the selectors resolved to instances. Execution instances must be observed at the native call, not inferred from that getter's primary-instance fallback. Current live execution tags remain unknown. Raw read-only queries are retained in `CCS/work/ability-ownership/instancing-reference.json`.

Runtime fact extraction: LoadedMoves now preserves separate selector/attack instancing policies, optional hold flags and direct asset tags, plus dynamic tags from the particular ASC grant. Missing values remain unknown. Containers are bounded to 64 direct tags and 1,024 bytes per name, sorted/deduplicated, and rechecked with candidate/grant identity before publication. No facts assign a slot or eligibility. Native execution and timing of this extension remain pending.

Lifetime foundation: [object identity](object-identity-runtime.md) now requires positive serials, initializes them through a dedicated uncached engine conversion, guards cached UFunctions as well as owners, and keeps Call parameter pointers in an instance-owned fixed array. Probe callbacks use only warm object fields and existing serials, with skipped identities counted. Initial grant capture now runs one grant per tick. The initializer's two parameter sizes were checked through a read-only live description; the revised initializer/cache/GC behavior and coverage of newly created execution instances remain unverified until native deployment.
