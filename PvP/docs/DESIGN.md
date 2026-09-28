# PvP for Mortal Shell II: design notes

Status: brainstorm, 28 September 2026. Nothing here is built. This is the starting point for the
first design session, written so the reasoning does not have to be redone.

Layout, decided already: `PvP/pvp-mod` (the client mod, native UE4SS like CCS), `PvP/pvp-server`
(the match server anyone can host), `PvP/docs` (this and what follows).

## The one road that works

Mortal Shell II ships the engine's network stack, but none of its gameplay is written to
replicate. The player pawn, the attack abilities, the combo counter and the hit checks are local
blueprints with no server calls. "Turning on multiplayer" is not available, and a mod cannot
rewrite the game's actor classes to replicate.

What a mod can do is what the co-op mods for other single-player games do (Subnautica's Nitrox is
the reference): every client keeps running its own single-player world, and the mod spawns a
proxy pawn for each other player and drives it from the network. The game hands us most of that
proxy.

- **The proxy pawn exists.** The Genessa trainer is an NPC that fights with the player's own
  weapons and has attack abilities for them (`GA_GenessaTrainer_BlackNeedle_B1..B3` and so on).
  Spawned with its AI off, it is a Sparta character with a weapons component, hit checks, poise,
  hit reactions and a lock-on target box. When it swings, the game's own hit pipeline damages
  the local player. Nothing to fake.
- **Driving it is what CCS knows.** The proxy plays montages. CCS already loads, cleans and plays
  montages on the human rig. One message per swing carries montage id, start time, rate and the
  CCS carry state, and the proxy plays exactly what the remote player's game played.
- **Looks are what CSS knows.** A look descriptor (package, variant, dye, spring settings) goes
  over the wire and CSS applies it to the proxy pawn instead of the player pawn. One new CSS
  entry point: apply a look to an arbitrary character actor.
- **Arenas are what traverse and the cheat menu know.** Teleport both players to one landing
  area, clear the area's enemies, pin the rules (health, stamina, Resolve, healing).

## What is hard

1. **Who decides damage.** Each client's game decides the damage its own player takes, from the
   proxy's hit checks landing locally. Simple and responsive; exploitable, since a modified
   client can refuse damage. Recommendation: accept it for a community mod and let the server
   sanity-check totals (damage per hit within the weapon's range, hits per second within reason)
   rather than build authority into the server.
2. **Paired animations.** Parry, riposte and grabs are paired montages that need both actors in
   lockstep. Locally the game runs the pair on attacker and victim. Across the wire one side has
   to be told "you were riposted, play the victim montage now" and snap into place. This will
   feel wrong first and take the longest.
3. **Latency at melee range.** A 20 to 30 Hz snapshot stream with interpolation is fine for
   movement. Hit windows are 60 to 150 ms wide, about one round trip on a bad day. Client-side
   hit detection (my proxy hit me, in my world) avoids the round trip; that is why point 1 has to
   be client-side.
4. **The game's systems around a non-AI target.** Lock-on, camera, the combo counter's target
   updates, the Harbinger transition, deaths and respawns. Each needs a small hook.

## Architecture

```
 player A's game                    match server                    player B's game
 ┌──────────────┐   snapshots +    ┌──────────────┐   snapshots +   ┌──────────────┐
 │ pvp-mod      │ ───────────────► │ room          │ ──────────────► │ pvp-mod      │
 │  proxy of B  │ ◄─────────────── │ relay         │ ◄────────────── │  proxy of A  │
 │  own player  │     events       │ rules, results│     events      │  own player  │
 └──────────────┘                  └──────┬───────┘                  └──────────────┘
        ▲                                 │ fan-out                          ▲
        │ reactions                       ▼                                  │
        │                          spectators (proxies of A and B)           │
        │                                 │                                  │
        └──────────── ladder service (Steam id, leaderboard, history) ◄──────┘
```

- **pvp-mod** (native, UE4SS): the proxy pawns, the sync loop, the PvP menu tab (rooms, arenas,
  rules, ready), the spectator view, the HUD feed for reactions.
- **pvp-server** (one small static binary, Go or Rust): rooms, relay of the players' streams,
  spectator fan-out, coarse rule enforcement, match results. Community-hosted like a Minecraft
  server; the author runs the official one. A server list is a later feature.
- **Ladder service** (web): accounts through Steam id, leaderboard, match history, the
  reaction feed. The match server posts results to it. Optional until step 4.

### Protocol

- Transport: UDP with a light reliability layer (ENet-style): unreliable sequenced for
  snapshots, reliable ordered for events. WebSocket over TCP would work for a prototype but
  head-of-line blocking shows at melee range.
- **Snapshot** (20 to 30 Hz per player): transform, velocity, montage id + position + rate,
  gameplay tags that matter (attacking, blocking, staggered, dead), health, stamina, Resolve.
- **Events** (reliable): swing started (montage, rate, CCS carry state), hit landed (payload id,
  damage, poise, reaction), parry, riposte with victim montage, death, rematch, chat, reaction.
- **Room setup**: look descriptor (CSS), combat set (CCS), rules (cheat menu), arena (landing
  area name).
- Spectators receive the same streams and run two proxies in their own world. Their comments and
  reactions travel on the reliable channel and show as a HUD feed on the players' screens.

## Dependencies and the APIs to add

| Mod | What PvP needs from it |
|---|---|
| CCS | montage load and clean as a library; play a montage on a given pawn; serialise and apply a combat set |
| CSS | export the current look as a descriptor; apply a descriptor to a given pawn |
| CSSX cheat menu | set health, stamina, Resolve and healing on a pawn; despawn the enemies of an area |
| CSSX traverse | teleport to a named landing area and report when streaming is done, with a guard against firing mid-transition (the 28 September save corruption) |

These are extension points, not rewrites. CCS keeps working without PvP; PvP declares the three
mods as dependencies and checks their versions at start.

## Sequence

Each step is something you can play.

1. **Ghost.** Record your own fight (transform, montages, look) to a file, then play it back as
   a trainer-proxy in your world. No network. Proves the proxy, the montage driving, the look
   apply and the hit pipeline, and becomes the test rig for everything after.
2. **Two players, one room.** The match server as a bare relay, LAN or one hosted box, two
   clients, one arena. First real duel.
3. **Paired animations and rules.** Parry and riposte across the wire, the cheat menu rules,
   deaths and rematch.
4. **Spectators and the ladder.** Fan-out, reactions, results, leaderboard page.
5. **Public server list and polish.**

Honest sizing: step 1 is weeks on top of what exists, step 2 a couple of months, steps 3 and 4
the long tail.

## Decisions to make first

1. **Damage stays client-side?** Recommendation: yes, for feel and scope, with server sanity
   checks. The alternative (server-authoritative hits) means the server simulates hit windows it
   cannot see and adds a round trip to every hit.
2. **Duels only, or also 2v2 and free-for-all?** Changes the proxy count, the arena rules and
   the HUD, not the architecture. Recommendation: duels first, keep the room model N-player.
3. **Which arenas.** Boss rooms and open areas with a clear boundary. Needs the traverse landing
   area names and a despawn rule per area.

## Open questions to answer with probes, not guesses

- Does the trainer NPC accept every player weapon, or only the ones it has abilities for? If not,
  a plain Sparta character with a weapons component and CCS-driven montages is the fallback.
- Can hit checks on the proxy be driven from a montage we start directly, without its ability?
  CCS's hook works at the ability task; the proxy has no input abilities.
- How does lock-on pick targets: tag, class, or component? The proxy needs whatever the game
  checks.
- What does the game do when the local player is riposted by an actor that did not parry in the
  local world? This decides the paired-animation approach.
- Steam networking (SDR relays through the Steamworks API) versus our own UDP: Steam would give
  NAT traversal for free but ties hosting to Steam sessions.
