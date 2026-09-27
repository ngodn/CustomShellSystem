# Mortal Shell II combat: web research as of 27 September 2026

Purpose: feed the Custom Combat System (CCS) mod with an accurate picture of how the game plays right now, what players want changed, and how other mods already change it. This document extends `../../docs/ms2-combat-system.md` (state as of 15 September, with game-data numbers) and does not repeat it. Where a number is already in that document it is referenced, not restated.

Evidence classes used throughout:

- **Official**: Steam news posts by Cold Symmetry, the Balance Patch 1 forum post, developer replies on Steam.
- **Game data**: numbers from the installed build, taken from the prior document, quoted only when the web disagrees with them.
- **Guide**: Fextralife, Game8, GameRant, GameSpot, KosGames, NerdsChalk, mortalshell2.org, mp1st, PCGamesN, reviews.
- **Community**: Steam discussion threads and reviews, Nexus mod pages (a mod page is good evidence of what a value does, because the mod works).
- **Unreliable**: pages that turned out to be generic or fabricated. Called out explicitly.

Single-source claims are marked **(single source)**. Conflicts are marked **(conflict)** with the winner stated. Raw per-source extracts with every URL live in `../work/web-research/`:

- `01-patches-after-15-sep.md`, `02-mods-nexus-cheat.md`, `03-community-threads.md`, `04-weapons-movesets.md`, `05-core-mechanics.md`, `06-engine-facts.md`.

## Changes since 15 September

Two official posts landed after the previous document, and neither changes balance.

| Date (UTC) | Post | Console version | Combat-relevant content |
| --- | --- | --- | --- |
| 14 Sep 19:05 | "New Hotfix out now" (the "15 Sep hotfix" in the prior doc) | 1.000.010 | Colossus Stone ability can be cancelled mid-execution; Lazlo Temperament infinite glitch closed; Proxima stuck in Biosampler fixed; burn and stasis hazard values corrected; NPCs no longer damageable by sidearms. |
| 24 Sep 22:20 | "Hotfix Live Now" | 1.000.011 | Slayer Seal unresponsive bug fixed; Clockwork Scythe elemental effects were applying incorrectly, now fixed; Genessa's Faithful Doubles work with Lost Clotstone; final boss fire ring and The Warden jump timing tweaked; Adaptive Difficulty now shows tiers with new icons; Low Latency Mode option (NVIDIA Reflex, AMD Anti-Lag 2) added; VSync tooltip fixed with Frame Generation. |

Nothing newer exists as of 27 September (Steam news hub checked). "Bring my Ova Back to me" trophy remains broken and is promised for the next update, so another hotfix is coming. No balance patch has been announced.

Version map (mp1st, PS5 numbering): Week 1 = 1.000.008, 5 Sep update = 1.000.009, 14 Sep = 1.000.010, 24 Sep = 1.000.011. Steam build IDs: Week 1 25005568, 5 Sep 25133113 (mortalshell2.org, SteamDB blocked to fetch).

Other things that changed in the mod scene or community since 15 Sep: Omni Seal mod (23 Sep) merges Guard, Harden and Slayer Punch onto one seal; the "enemy design still feels cheap" thread is still active; a "Unable to dodge forward" thread appeared on 27 Sep (title only, no replies yet).

## 1. Patch history and renamed or new mechanics

### 1.1 Confirmed patch list

| Date | Name | Version / build | Source |
| --- | --- | --- | --- |
| 18 to 19 Aug | Hotfix 1.0 / 2.0 (PS5 1.000.006) | | Game8 patch list (prior doc) |
| 20 Aug | Balance Patch 1 | none published | Steam forum post (official) |
| 26 Aug | Hotfix 2 (multi-platform): crash, Slayer Seal, riposte, entitlement fixes | | mortalshell2.org |
| 29 Aug | The Week 1 Update | Steam 25005568, console 1.000.008 | Steam news (official) |
| 1 Sep | PC Hotfix / "Hotfix 3": shades, FSR map, Citadel Annex spider, enemies not reacting, collision, lens flare | about 1 GB | mortalshell2.org, mortalshell2.online |
| 5 Sep | "New Update Out Now" (Adaptive Difficulty) | Steam 25133113, console 1.000.009 | Steam news (official) |
| 14 Sep | "New Hotfix out now" | 1.000.010 | Steam news (official) |
| 24 Sep | "Hotfix Live Now" | 1.000.011 | Steam news (official) |

Extra Balance Patch 1 detail not in the prior doc (mortalshell2.online full text): enemy trims Cultist (mace) -21.1% max health, Caerinid spider -33.3%, Infested Stalker -52.4%, Rusted Knave -20.8% plus a hit-detection fix on its overhead; Lost Child -10% damage / -15% health; Monolith -19% damage / -10% health with attack retiming. Week 1 extra detail: "many recoveries end sooner" for ripostes, "calculation rewrite fixed ripostes applying twice or not at all", Zmey tail grab no longer 100-0s both bars, Hexapod arena smaller with higher splash damage, Warden and Urrig got new ice attacks, Urrig's unparryable spin now has a warning, fewer enemies in the busiest areas. 5 Sep: damage numbers display up to 9,999 instead of 999.

Caution: mortalshell2.org re-dates its update pages (every entry showed "Sep 25, 2026" on fetch), so use it for content, not dates.

### 1.2 Adaptive Difficulty (5 Sep, tiers visible since 24 Sep)

Official description: optional, off by default, under Game > Game Settings. While winning, enemy damage, health and break resistance rise above Standard with no stated ceiling ("they will keep climbing for as long as you keep winning", PCGamesN paraphrase of the notes). While dying repeatedly, those values fall, and a one-shot protection stops a single hit from killing a full-health character. Adjustments decay back toward Standard over time. Night Mode is only ever pushed harder, never eased. The 24 Sep hotfix added "tier" UI and new indicator icons, which confirms discrete internal tiers; thresholds, weights and per-tier multipliers are unpublished (all outlets say so). gamers4.life quotes the in-game text: "Difficulty is auto-adjusted based on your record. In general, the more times you die, the challenge is temporarily reduced."

### 1.3 Other current mechanics

- **Fragile Tarstones** (Week 1): Support-slot stones with durability that break into Tarcores; Justiciar's and Gloombound Stones were converted to Fragile with stronger passives and on-break effects; drops, dungeon rewards and Merrick stock added; 5 Sep fixed Tarcore grants on break. (Official, fandomwire, gamer.org.)
- **Mether's Severance** (Week 1): 5,000 coins at Merrick, starting stock 3, refreshes at milestones; Zhirelle severs the bond, resets the shell, refunds all Glimpses; Genessa's bonding Glimpses cannot be refunded (GameRant, single source for the Genessa exception). 14 Sep made it usable at the Genessa trainer; 24 Sep fixed the gamepad cancel soft lock.
- **NG+ "New Purpose"**: after Zmey, choose "Send Ova" at the Gloom Siphon in Marrow Keep; there is no confirmation prompt (KeenGamer). Carries over level, Shell Points, bonds, all shells, gear with upgrades, currencies, map reveal, Mether's Pulse; resets Ova, gates, dungeons, beacons (two kept), quests, chests; duplicate gear becomes materials. Guides say enemies gain health, damage and aggression; Steam players in the NG+ thread say enemies feel the same and only gate bosses are a bit harder, with Night Mode as the real difficulty layer **(conflict, unresolved; the player reports are post-release and more specific)**. A search snippet claims scaling stops at NG+7 / "Journey 8" (source not reachable, **unverified**).
- **Difficulty layers**: no difficulty menu. Standard, Slayer Seal (easier, disables achievements), Night Mode via Gloombound Flame at Marrow Keep (reversible), Adaptive Difficulty, NG+. (AltChar 17 Aug, Game8.)

## 2. Per-weapon movesets as players describe them

No community frame data exists for MS2 as of today. The only page claiming per-weapon frame data (shell2hub.com, dated 1 July 2026, pre-release) lists Mortal Shell 1 weapons and a "two-segment Resolve" system and is fabricated; its "10-frame input buffer" claim on a sister page has the same problem. mortalshell2.org says outright that it excludes damage and frame data because nothing verified exists. The best moveset descriptions are KosGames (18 Aug, with a companion video) and GameRant (20 Aug). Fextralife's eight weapon pages contain no moveset detail at all (checked 27 Sep, last edits 20 Aug to 3 Sep). Everything below is qualitative unless stated.

| Weapon | Light chain | Heavy chain | Running light / heavy | Ability | Pace / reach (mortalshell2.org) | GameRant tier | Patches and quirks |
| --- | --- | --- | --- | --- | --- | --- | --- |
| The Iconoclast | one-two (third hit added by Zealot's Stone) | thrust into overhead; heavy "has built-in poke" and flows into light | jump attack / golf swing launcher | Plummet Strike | Balanced / Medium | A, "most balanced" | Beta feedback: lights registered off the visible blade. No post-release bug reports found. |
| Axe & Dagger | multi-hit, stacks status fast | "standard dual-wield heavies" | multi-hit / leap with good closing distance | Deadly Flurry | Fast / Short to medium | A, "third fastest", "worse Axatana" | Running attack animation improved 5 Sep. Two weapon actors; Damage Multiplier mod excludes it for that reason. |
| Veteran's Battle Axe | poke into combo; second light weak, use light-heavy-light | overhead, spin, golf swing | like Iconoclast / "star of the show", two big swings, staggers most large enemies | none named anywhere | Heavy / Medium | A | Heavy weapon poise and damage pass in Week 1. |
| Great Martyr's Blade | thrust, big swing, thrust ("extremely awkward") | heavy swing then enhanced heavy ("slower than molasses") | big golf swing / spin into thrust, one-shots basic enemies | Spiral Surge | Slow / Long | B | +20% (Balance Patch 1, did not ship), buffed again Week 1; only melee Frost (Warden's Stone). Called underpowered vs Axe & Dagger in the 18 Aug bug thread (pre-buff). |
| Obsidian Hammer | big vertical "bonks", extremely slow | sweeping hits with enhanced final hit | jump attack, "quick strike" / big slam | Heavy Stomps | Slow / Medium | C (pre-Week-1) | Full balance pass Week 1; Colossus Stone cancellable since 14 Sep. |
| Axatana | twin blades "one-two-one-two-one-two, very fast" | morphs to axe, single-target then AoE; author rarely uses heavies | multi-hit whirlwind / two big hits | Morph Attack | Hybrid / Medium | S | Tracking improved Balance Patch 1; morph gains i-frames and Fragile Week 1; Duality Stone doubles lights. |
| Black Needle | thrust, thrust, spin, thrust (4); "commit for under a second" | sweep x4 then thrust (5), slow, "awkward pathing" | quick stab / heavier thrust | Needle Storm | Fast / Long | S, "fastest attacking weapon" | Tracking improved Balance Patch 1. |
| Clockwork Scythe | sweep, sweep, big sweep (3), best AoE | standard heavies, extremely slow, little AoE | quick sweep / heavy sweep; both running attacks are two-hit combos that lock out parry and dodge | Clockwork Chainsaw | Moderate / Long | B | Tracking improved Balance Patch 1; elemental effects applied incorrectly until 24 Sep. |

Cross-weapon facts from the web:

- Hit counts above come from KosGames alone **(single source)** except the Scythe's three-sweep light (GameRant agrees) and the Battle Axe running heavy (both agree).
- Plunge attack: Light Attack while falling, no fall damage (tutorial text). Charged attacks exist only through Acolyte's Stone and Unwieldy Stone; no guide documents their timing; beta feedback said the charged heavy's range is shorter than its animation.
- Running attacks are separate montages: two mods reuse "the weapon's native running attack" as a dodge attack.
- Damage per hit is not published by any guide. Steam players attribute hit-to-hit variance to crits (Auspicious 18% chance, Headsman's +100% damage, Grudge every 5th) and to enemy weakness to light, heavy or ranged damage. "This game infuriatingly hides half its numbers" (Steam, 28 Aug).
- Speed is an attribute: the Faster Attacks and Customize Your Game Difficulty mods scale `AttackSpeedBonus` and report that attacks, shell abilities and follow-ups all speed up while enemies do not.

## 3. Core mechanics as the community currently understands them

**Stamina and commitment.** No stamina. Attacks cannot be cancelled into guard or dodge in vanilla ("each strike animation is long and can't be canceled out of", Gamecritics, 8 Sep). Harden is the one exception: it can be pressed during attacks, dodges, after a missed parry and mid-air (Fextralife), it pauses the current attack, and when broken by a hit "your current attack quickly resumes" (in-game text). A Steam tester: "Hardening doesn't cancel animations at all, but it can pause almost every moveset." The Guard-cancel (mod 146) and Dodge-cancel (mod 230) mods prove the transitions are only tag-gated: a Lua script can force them and the game accepts it.

**Dodge attacks and input queue.** No dodge attack in vanilla. Dodge Attack Alternative (mod 263) had to build a "pseudo input/action queueing" so the attack fires the moment the dodge allows it, which implies no native attack buffer during a dodge. No trustworthy measurement of the general input buffer exists. Parry inputs are reported as sometimes dropped (Steam, 22 Aug), with frame generation blamed for desync; Low Latency Mode shipped 24 Sep in response to input-lag complaints.

**Dodge and i-frames.** Dodges grant i-frames (consensus, no frame counts published). Red-circle attacks and grabs cannot be blocked, parried or hardened (Outerhaven, 22 Jun; Steam grab thread) and grabs "track while you're dodging" so the advice is to dodge on the lunge, not on the symbol. Beta feedback wanted more dodge distance; Better Dash (pak) shortens dash travel time 0.5 to 0.41 s for 1.2x distance, so distance is velocity times a travel-time value in a cooked asset **(single source, the mod author)**. Per-shell dashes exist in data (Tiel Shadow Dash, Genessa dash fixes in Week 1) but nobody has compared their i-frames.

**Resolve.** Gained only from melee hits; the per-hit amount varies by attack and weapon but no guide gives numbers. `MaxResolve 100` and `BaseResolveGain 0.1` are the live knobs (Easy Mode mod doubles and triples them). Riposte gives no Resolve except under the Slayer Seal, which the 29 Aug riposte thread calls the core flaw: "everything else revolves around using and restoring resolve." Smert's Faith doubles gain; Devout Stone 1.5 to 2.0x.

**Break and riposte after Week 1.** Riposte damage scales with enemy health, i-frames last the full animation, recoveries were shortened, and status effects apply during the enemy's get-up since 5 Sep. Measured by players: 255 to 200 on Grisha hunters after the change (Arisu), 200 to 3,600 crit on Sir Isaac with Eredrim (another user) **(conflict, both plausible: health scaling helps on bosses, hurts on fodder)**. Recurring complaints: riposte weaker than light attacks on trash, no Resolve return, the parry seal locks the whole animation even on a successful parry, and multi-hit enemy strings punish a successful parry (Krom, top-rated Steam review).

**Parry, guard, harden timing.** No official numbers. The one community measurement (Steam, "Am I losing my mind, or is the parry window actually delayed?"): Infinite Seal parry has about 5 frames of wind-up (about 0.08 s at 60 fps) before it is active; Untarnished guard shows none within a 2-frame test; Vatra's harden none within 3 frames. This agrees with game data (Untarnished perfect window 0.36 s with 0.6 s cooldown, Vatra perfect window 0.25 s, Infinite Seal window widened by the Medium/Long unlock effects and by the 5 Sep consecutive-parry change). The Parry Indicator mods (130, 218) read the enemy attack montage's hit-check window live and apply a seal-specific start delay and window length, which is now the accepted model. mortalshellii.wiki's "0.2 s before contact", "four second harden cooldown" and "three second riposte window" are generic guesses (**unreliable**). Steam Deck HQ: the parry window is "a little larger than a lot of other games" and fine at a locked 30 fps.

**Hit-stop and stagger.** No source measures hit-stop. Reviews say hits have weight; Hardcore Gamer notes player attacks "can get interrupted while attacking enemies" (enemy hits stagger you mid-swing; player Poise is 5 in data). Beta feedback: light attacks lack the inertia their animations promise. Enemy poise and break resistance numbers are not public; Adaptive Difficulty scales break resistance; a generic guide's "bosses need about 3x posture damage" is unverified.

**Lock-on and camera.** Q on keyboard, R3/RS on pad; auto lock-on is on by default; Free Aim is recommended for sidearms; Target Steering settings exist (Game8). Complaints: attack magnetism pulls you and enemies "half way across the map" (Krom), lock-on camera zooms too close and snags on geometry (Camera is a Boss thread, dev smithbodie asked whether the auto-adjust camera setting helped; users said barely), fisheye effect when severed.

**Crits.** No base crit chance without Tarstones (Steam, captain-carry guide). Harbinger levels 1 to 30 raise Health, Shell Health, Base Damage, Critical Chance and Shell Points (Game8, Fextralife, matches `DT_PlayerExperience`); one Steam reply says levels raise crit damage **(conflict, game data wins: Critical Chance)**. Steam consensus: weapon levels feel weak, progression is Tarstones and shell skills, "most enemies don't change their health pools or damage values over the course of the game."

**Harbinger and Shell Revive.** Severed Harbinger has its own small health bar and no shell ability; melee damage fills the revive meter (bottom right), then the Shell Ability input (F / triangle) revives the shell at low health; up to 3 revives before a beacon rest, each needing more damage; higher Bond tier fills faster (Fextralife, GamesRadar, GameSpot). Genessa becomes Stray Genessa instead of severing.

## 4. What players want tuned (controls the mod should expose)

Grouped by theme, with the thread or review that raises it. Everything here is community opinion, not official.

1. **Riposte economy**: restore Resolve on riposte for every seal, not just Slayer; minimum damage floors by enemy class (fodder one-shot, elites 50%, bosses 25%); scale with player level and weapon instead of enemy health; per-weapon riposte effects. (Steam "The new riposte", 29 Aug; reviews by TheDementedSalad, Krom.)
2. **Defensive animation lock**: successful parry should not lock the full animation; multi-hit strings should not punish a correct parry; harden and guard should be able to interrupt attacks. (Krom review; mods 146, 147, 230 with 38 to 81 endorsements each.)
3. **Timing windows and start-up**: parry wind-up of about 5 frames feels delayed; some enemy attacks have near-zero start-up; wider or configurable windows are the most-endorsed mod category (mod 15, 81 endorsements). (Steam parry delay thread; bug thread 18 Aug.)
4. **Dodge**: longer distance, attacks and grabs tracking through dodges, dodge-cancel out of attacks, dodge attacks, "unable to dodge forward" (27 Sep). (Beta criticism thread; grab thread; mods 22, 228, 230, 263.)
5. **Speed and weight**: two camps. One wants faster player attacks (Faster Attacks +75% default, Faster Attack Speed 1.2x); the other wants more heft and inertia, less "stiff and sticky" movement, and less floaty locomotion and camera (beta threads). Heavy weapons (Hammer, Martyr's Blade) still read as too slow for their payoff even after Week 1 (GameRant tiers, bug thread).
6. **Lock-on and camera**: magnetism strength, camera distance and FOV, snag on geometry, close zoom while locked, severed fisheye. (Krom; Camera is a Boss; mods 10, 39.)
7. **Hitboxes and hit registration**: big enemies hit when visibly missing; player lights register off the blade (Iconoclast); "invisible backstab whiffs constantly" (FinalBoss). Not something a tuning mod fixes, but a hit-window or hitbox scale control would let players compensate.
8. **Input latency**: frame generation desync, dropped parry inputs; partly addressed by Low Latency Mode (24 Sep). A control for the buffer window would be new territory since vanilla does not appear to expose one.
9. **Knockback and stagger**: weak mobs push too hard, players knocked off ledges through barriers; want configurable knockback (already an ability field: KnockbackStrength normal and perfect). (Bug thread 18 Aug.)
10. **Difficulty**: bosses easier than trash mobs (FinalBoss); NG+ barely harder (Steam NG+ thread); Adaptive Difficulty has no published thresholds and players want to see and set its tiers; Night Mode is the only real hard mode. A direct enemy health / damage / break-resistance multiplier is what the mod scene has not shipped yet (Customize Your Game Difficulty is player-side only).
11. **Transparency**: show the hidden numbers (crit chance, resolve per hit, break values); Health and Resolve Numbers mod has 49 endorsements.

## 5. Existing mods that touch combat

All on Nexus Mods (game id `mortalshell2`, 272 mods). UE4SS for MortalShell2 (mods 5 and 45, 82 endorsements) is the common loader; DML (mod 4) loads Blueprint LogicMods without DLL hooks. CSS / CSSX and the UE4SS Cheat Menu (mod 20) are the user's own and are context only; a third-party "Hide Seal and Sidearm Toggle" (mod 528) is built on CSSX.

| Mod (id) | Author, date | Technique | What it changes |
| --- | --- | --- | --- |
| Custom Parry Perfect Guard and Harden Timing (15) | Allki1869, v1.5 28 Aug, 81 endorsements | UE4SS Lua, Ctrl+F3 UI | Parry window, parry activation delay, parry speed multiplier; perfect-guard window, activation delay, invulnerability, cooldown, Continuous Perfect Guard; perfect-harden window. Presets incl. "Extreme" 30 s windows. Steps 0.001 s / 0.01x. |
| Easier Perfect Guard And Interrupt Attack (147) | native C++ UE4SS | reimplementation of 15 | Same windows plus guard/parry/harden interrupt any attack; no parry speed. (Page behind captcha, details from listing.) |
| Guard out of any attack (146) | Grimpil, v3 27 Aug, 38 endorsements | UE4SS Lua | Guard cancels any attack mid-combo, all weapons; snap only when actually cancelling. |
| Dodge out of any attack (230) | Grimpil | UE4SS Lua | Dodge interrupts an ordinary attack instantly; distance, i-frames and roll gap untouched. |
| Dodge Attack Alternative (263) | zWind14, 4 Sep | UE4SS Lua | Light/heavy during a dodge fires the running attack via a pseudo input queue; includes Smert Fight Stance. |
| Dodge Attack (228) | | UE4SS | Attacks in a window after a dodge use the running attack. |
| Better Dash (22) | AeonGreyh, 17 Aug, 86 endorsements, 3,604 downloads | pak/ucas/utoc override | Dash travel time 0.5 to 0.41 s, 1.2x distance. |
| Faster Attacks (124) | Grimpil, 23 Aug | UE4SS Lua | Player attack speed stat, default +75%, config line; enemies unchanged. |
| Faster Attack Speed (77) | | | 1.2x attack speed. |
| Damage Multiplier (143) | AeonGreyh, 24 Aug, 500 downloads | UE4SS Lua, live config | Per-weapon damage for 16 weapon actors (default 2x); Axe & Dagger excluded (two actors). Frame drops in shell memories. |
| Customize Your Game Difficulty (49) | kingsyu, 19 Aug, 980 downloads | UE4SS Lua editing attributes and GameplayEffects | MaxResolve, Poise, MaxPoise, BreakResistance, resistances, CriticalChance, CriticalBonus, BaseResolveGain/Lost, AttackSpeedBonus, BaseDamageReduction, heal charges and amount, Gloom/Gold multipliers, RequiredActivationTime, PerfectBlockInvulnerabilityDuration, KnockbackStrength, status durations and damage. Player only. |
| Simple Easy Mode (38) | kingsyu, 18 Aug, 97 endorsements, 3,164 downloads | pak override of player attribute defaults | Resolve 200, flasks 30 x 100, BaseResolveGain 0.3, 50% damage reduction, +50% attack speed, 2x Gloom and Gold. |
| Combat Assistant (64) | | UE4SS | Auto perfect guard and auto dodge, red-mark-only dodge option. |
| Parry Indicator (130) / Mk2 (218) | fakekey2k 27 Aug / Jarol 6 Sep | C++ / Lua UE4SS | Shows the true hit-check window from the enemy attack animation with seal-specific delay; Mk2 uses the game's own tutorial indicator plus rumble. |
| Omni Seal (527) | SergeyHorn, 23 to 24 Sep | UE4SS | Guard, Perfect Guard, Harden, Slayer Punch and Slayer passives on one seal. Proves seal abilities can be granted independently of the item. |
| FOV ultrawide HUD (10); Immersive Sprint and Camera tweaks (39) | k4sh; WinterElfeas | UE4SS, ini | FOV, 3-axis camera distance, console; per-state camera values with hot reload. |
| Health and Resolve Numbers (7); Enemy Health Hide (521) | Allki1869; Suffskin | UE4SS | HUD numbers; hide enemy bars. |
| Free Upgrade (518) | RifzzzXD | | Removes Tarforge costs. |
| Cheat Engine table (FearlessRevolution t=40441) | to v7 | CE 7.7 | Infinite ammo and heals, zero melee ability cost, instant reload, level and Tarstone XP editors, teleport. (Direct fetch blocked; from search summary.) |

Gap analysis for CCS: nobody ships enemy-side tuning (enemy health, damage, break resistance, poise, attack speed), riposte Resolve return, riposte damage rules, lock-on magnetism strength, hit-stop, or a per-shell dash/i-frame editor. Those are open ground.

## 6. Engine facts relevant to combat modding

- **Engine version**: the installed build string is `MortalShell2-5.6.1-0+++Sparta-Depot+Main+CL93241` (game data, prior doc). DSOGaming's performance analysis (24 Aug) says "Powered by Unreal Engine 5.2" **(conflict, game data wins; no press source says 5.6)**. Lumen and Nanite are in use; the Hardware Ray Tracing toggle "does not appear to work" (DSOGaming); DLSS 4, FSR 3, DLSS Frame Generation and Multi Frame Generation are supported (WCCFTech); Reflex and Anti-Lag 2 since 24 Sep.
- **Gameplay Ability System**: confirmed by the GA_/GE_ classes in game data and by the Customize Your Game Difficulty mod, which edits GameplayEffect parameters and the attribute names listed in section 5 through UE4SS Lua. `AttackSpeedBonus` is a real attribute and is what the speed mods scale ("the game's own attack speed stat rather than forcing animations to play faster").
- **Motion matching locomotion**: the game ships an `ABPL_Locomotion_MotionMatching` layer (referenced in `docs/development/animation-playback.md` and `docs/development/eve-animation-sources.md`), so the PoseSearch plugin is cooked in; weapons also carry per-weapon locomotion layers (prior doc). No developer interview covers the animation stack; the only Cold Symmetry engine interviews are for MS1 (Unreal developer interview, Inside Unreal March 2022). The AMA of 14 Aug (prior notes) covers design intent (no stamina, Resolve central, weapons not tied to shells) and nothing about implementation.
- **Attack montages and hit windows**: enemy attack montages carry the hit-check window as animation data readable at runtime (Parry Indicator descriptions). Attack, guard and dodge transitions are tag-gated rather than hard-locked, since Lua mods cancel them freely.
- **Dash**: distance comes from a travel-time value in a cooked asset (Better Dash pak override). **Weapon damage**: `BaseDamage` on the `WP_` actor is writable live; Axe & Dagger uses two actors.
- **Single player only**: no multiplayer mode exists (Wikipedia, store pages), so no replication concerns; no explicit developer statement about networking was found.
- **Frame rate and timing**: players report frame generation desyncs inputs and disabling it improves parry timing; the official fix direction was a latency mode, not a timing change. No source shows timing windows scaling with frame rate.

## Sources

Reliability: A = official or game data; B = established guide or measured community test; C = opinion, tier list or single-source guide; D = unreliable or fabricated.

| Source | Publisher | Date | Reliability |
| --- | --- | --- | --- |
| https://store.steampowered.com/news/app/2584270/view/675132895796397463 | Cold Symmetry, Steam news ("Hotfix Live Now") | 24 Sep 2026 | A |
| https://store.steampowered.com/news/app/2584270/view/703279123228787784 | Cold Symmetry, Steam news ("New Hotfix out now") | 14 Sep 2026 | A |
| https://store.steampowered.com/news/app/2584270/view/690893589804221773 | Cold Symmetry, Steam news (5 Sep update) | 5 Sep 2026 | A |
| https://store.steampowered.com/news/app/2584270/view/690892955941077484 | Cold Symmetry, Steam news (Week 1) | 29 Aug 2026 | A |
| https://steamcommunity.com/app/2584270/discussions/0/582805931178489108/ | Cold Symmetry, Steam forum (Balance Patch 1) | 20 Aug 2026 | A |
| https://store.steampowered.com/news/app/2584270 | Steam news hub (list checked for newer posts) | 27 Sep 2026 | A |
| https://mp1st.com/title-updates-and-patches/mortal-shell-2-update-1-000-011new-hotfix-september-24 | mp1st, Alex Co | 24 Sep 2026 | B |
| https://mp1st.com/title-updates-and-patches/mortal-shell-2-update-1-000-009-adds-adaptive-difficulty-balance-changes | mp1st | 6 Sep 2026 | B |
| https://mortalshell2.online/patch-notes/ | mortalshell2.online (full patch text mirror) | fetched 27 Sep 2026 | B |
| https://mortalshell2.org/updates/ and /updates/hotfix-3/ and /updates/adaptive-difficulty-update/ | mortalshell2.org (dates unreliable, content matches official) | fetched 27 Sep 2026 | B |
| https://mortalshell2.org/tools/equipment-database/ | mortalshell2.org (beta build 24459107 UI text, pace and reach) | verified 30 Aug 2026 | B |
| https://www.pcgamesn.com/mortal-shell-2/update-adaptive-difficulty | PCGamesN | ~6 Sep 2026 | B |
| https://game8.co/games/Mortal-Shell-2/archives/616319 | Game8 patch list | fetched via search, page blocked | B |
| https://game8.co/games/Mortal-Shell-2/archives/615713 | Game8 lock-on guide | search summary only | B |
| https://mortalshell2.wiki.fextralife.com/Patch_Notes, /Combat, /Shell_Revive, /Harbinger, /New_Game_Plus and the eight weapon pages | Fextralife | 9 Jul to 3 Sep 2026 | B (thin on movesets) |
| https://kosgames.com/mortal-shell-2-weapon-guide-movesets-infusions-and-locations-57252/ | KosGames, James (with video https://www.youtube.com/watch?v=EkkgzG9Jy30) | 18 Aug 2026 | B, single source for hit counts |
| https://gamerant.com/mortal-shell-2-best-weapons-tier-list/ | GameRant, Hamza Haq | 20 Aug 2026 | C |
| https://gamerant.com/mortal-shell-2-new-game-plus/ | GameRant, William Parks | 17 Aug 2026 | B |
| https://www.keengamer.com/articles/guides/mortal-shell-2-new-game-plus-what-carries-over-and-what-resets/ | KeenGamer, Rafly Wibowo | 21 Aug 2026 | B |
| https://www.altchar.com/guides/mortal-shell-2-how-to-change-difficulty-aQjO17R4OLU7 | AltChar, Asmir Kovacevic | 17 Aug 2026 | B |
| https://www.gamespot.com/articles/mortal-shell-2-tips-beginners-guide/ | GameSpot, Jason Rodriguez | 17 Aug 2026 | B |
| https://nerdschalk.com/all-mortal-shell-2-weapons-and-how-they-work | NerdsChalk | 21 Aug 2026 | C |
| https://gamers4.life/mortal-shell-2/database/mechanics/ | gamers4.life (in-game text dump) | 15 Sep 2026 | B |
| https://www.theouterhaven.net/mortal-shell-ii-red-attacks-guide/ | The Outerhaven, Keith Mitchell | 22 Jun 2026 (beta) | C |
| https://www.noobfeed.com/articles/mortal-shell-2-eredrim-heavy-stagger-build | NoobFeed, Mash Rahman | 26 Aug 2026 | C |
| https://finalboss.io/mortal-shell-2-review-resolve-makes-combat-sing-until-it-doesnt | FinalBoss.io, Lan Di, review | 20 Aug, updated 3 Sep 2026 | C |
| https://gamecritics.com/kkoteski/mortal-shell-2-review/ | Gamecritics, Konstantin Koteski | 8 Sep 2026 | C |
| https://hardcoregamer.com/mortal-shell-ii-review/ | Hardcore Gamer, Cory Wells | 17 Aug 2026 | C |
| https://steamdeckhq.com/game-reviews/mortal-shell-2/ | Steam Deck HQ, Noah Kupetsky | 17 Aug 2026 | C |
| https://www.dsogaming.com/pc-performance-analyses/mortal-shell-2-pc-performance-analysis/ | DSOGaming, John Papadopoulos | 24 Aug 2026 | B for rendering, D for the "UE 5.2" claim |
| https://wccftech.com/how-to/mortal-shell-ii-pc-performance-analysis-tuning-guide-how-to-get-best-experience-on-pc/ | WCCFTech, Sebastian Castellanos | 28 Aug 2026 | B |
| https://steamcommunity.com/app/2584270/discussions/0/582806239606474230/ | Steam, parry delay measurement thread | Aug 2026 | B (measured) |
| https://steamcommunity.com/app/2584270/discussions/0/581680955258934330/ | Steam, "The new riposte, not quite the buff it needed" | 29 Aug 2026 | B (measured numbers), C (opinion) |
| https://steamcommunity.com/app/2584270/discussions/0/582806239606512918/ | Steam, "Inconsistent Parrying or Input Delay?" | 22 Aug 2026 | C |
| https://steamcommunity.com/app/2584270/discussions/3/582805931178342787/ | Steam bug report, "Current gameplay issues and physics bugs" (dev reply) | 18 Aug 2026 | B |
| https://steamcommunity.com/app/2584270/discussions/0/582805931178353642/ | Steam, "Camera is a Boss." (dev reply) | 19 Aug 2026 | B |
| https://steamcommunity.com/app/2584270/discussions/0/418424310691219614/ | Steam, "grab attacks" | 18 Aug 2026 | C |
| https://steamcommunity.com/app/2584270/discussions/0/582806523877574646/ | Steam, "what the hell is with the damage in this game?" | 25 Aug 2026 | C |
| https://steamcommunity.com/app/2584270/discussions/0/582805931178364166/ | Steam, "Character progression" | 19 Aug 2026 | C |
| https://steamcommunity.com/app/2584270/discussions/0/582805931178553029/ | Steam, "enemy design still feels cheap" | 21 Aug 2026, active 27 Sep | C |
| https://steamcommunity.com/app/2584270/discussions/0/582805931178521161/ | Steam, "New Game Plus" | Aug 2026 | C |
| https://steamcommunity.com/app/2584270/discussions/2/564785528365608657/ , /563659002336016702/ , /563659002336074076/ , /563659002335987338/ | Steam open beta feedback threads | Jun 2026 (beta build) | C |
| https://steamcommunity.com/app/2584270/reviews/?browsefilter=toprated | Steam top-rated reviews (Krom, Murthag, alexander1, TheDementedSalad, MrPouros) | 19 to 24 Aug 2026 | C |
| https://www.nexusmods.com/mortalshell2/mods/15 , /146 , /147 , /230 , /263 , /228 , /22 , /124 , /77 , /143 , /49 , /38 , /64 , /130 , /218 , /527 , /10 , /39 , /7 , /521 , /518 , /5 , /45 , /4 , /20 , /528 | Nexus Mods pages (mods 147, 230, 228, 64, 77 behind captcha; details from listing) | 17 Aug to 24 Sep 2026 | B for what each mod exposes |
| https://www.nexusmods.com/mortalshell2/mods/top | Nexus top-30 listing | fetched 27 Sep 2026 | B |
| https://fearlessrevolution.com/viewtopic.php?t=40441 | FearlessRevolution Cheat Engine table thread | 2026 (blocked, search summary) | C |
| https://shell2hub.com/articles/parry-resolve-guide.html and /controller-settings-guide.html | shell2hub | 1 and 19 Jul 2026 | D (MS1 content presented as MS2 frame data) |
| https://www.mortalshellii.wiki/combat/combat-guide/ | mortalshellii.wiki | 9 Aug 2026 | D (generic numbers, no method) |
| https://www.xmodhub.com/info/guides/mortal-shell-2-trainer-cheats-guide/ | xmodhub | 2026 | D (advertises "infinite stamina") |
| https://www.reddit.com/r/soulslikes/comments/1vo101d/mortal_shell_ii_ama_with_cold_symmetry/ | Cold Symmetry AMA (from prior notes; Reddit blocked today) | 14 Aug 2026 | A for intent |
| ../../docs/ms2-combat-system.md and ../../docs/development/animation-playback.md | local game-data documents | 15 and 21 Sep 2026 | A |

Access notes for whoever refreshes this: Steam news bodies need `r.jina.ai`; Fextralife and Nexus need `r.jina.ai` and Nexus still serves a captcha on some pages; SteamDB, PCGamingWiki, Game8, Reddit (including old.reddit) and FearlessRevolution refused direct and proxied fetches on 27 Sep; YouTube pages return no description.
