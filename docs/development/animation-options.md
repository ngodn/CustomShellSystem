# Mod-provided animation options

2026-09-21. Native data and profile support is implemented on the v1.0.0 branch.
Playback, expanded LOCOMOTION controls and cooked/live acceptance are still in
development. Do not publish a package relying on these fields yet.

The UI calls the game choice **Default**. The existing walk setting keeps its
saved `normal` value for compatibility with old state files and profiles.
The new per-outfit choices reserve `original` for Default; it is never a mod
option ID. Existing feminine walk saves continue to load unchanged.

## Manifest

An outfit may define `animations`. A variant may replace individual slots;
omitted slots inherit from the outfit, and an empty array disables an
inherited slot. A missing outfit or variant never borrows another one's
animations. IDs are unique within a slot. Display order follows the manifest.

```json
{
  "animations": {
    "idle": [
      {
        "id": "relaxed",
        "name": "Relaxed",
        "clip": "/Game/CSS/Eve/AN_Idle.AN_Idle",
        "hide_weapons": true
      },
      {
        "id": "armed",
        "name": "Armed",
        "by_weapon": {
          "Weapon.MartyrsBlade": "/Game/CSS/Eve/AN_Blade.AN_Blade"
        }
      }
    ],
    "walk": [
      {"id": "eve", "name": "Eve", "blend_space": "/Game/CSS/Eve/BS_Walk.BS_Walk"}
    ],
    "jog": [
      {"id": "eve", "name": "Eve", "blend_space": "/Game/CSS/Eve/BS_Jog.BS_Jog"}
    ],
    "sprint": [
      {"id": "eve", "name": "Eve", "blend_space": "/Game/CSS/Eve/BS_Sprint.BS_Sprint"}
    ],
    "beacon": [
      {
        "id": "graceful",
        "name": "Graceful",
        "depart": "/Game/CSS/Eve/AN_Kneel.AN_Kneel",
        "arrive": "/Game/CSS/Eve/AN_Rise.AN_Rise"
      }
    ]
  }
}
```

These are example paths, not a claim that those release assets exist. Use
short `/Game/CSS/` paths for new authored packages. The parser also accepts
valid existing `/Game/` references. It checks path syntax, not installed asset
availability, class, skeleton or actual animation quality.

Idle stays one visible option regardless of weapon. `by_weapon` uses exact
game `Weapon.*` tags, such as the documented `Weapon.MartyrsBlade`, rather than
localized labels or arbitrary skeleton bone indices. Exact matches override
an optional common `clip`. Without either an exact match or common clip,
the resolver returns Default. It never prefix-matches another weapon or
chooses the first map entry. An unarmed idle uses one common clip with
`hide_weapons: true`; weapon-specific hidden-idle combinations are rejected.
The runtime must restore weapon visibility before combat, aiming, a state
change, outfit removal or cancellation. Metadata alone never hides a weapon.

Movement slots name BlendSpaces, consistent with the game's existing
`ActiveBlendSpace` override and direction/speed inputs. Individual source
sequences still need authoring into a suitable directional/speed blend space.
The graph must preserve speed, foot cadence, start/stop transitions and combat
overlays. A single forward sequence is not automatically suitable for strafing.
Epic describes movement speed/direction as blend-space inputs in
[locomotion-based blending](https://dev.epicgames.com/documentation/en-us/unreal-engine/locomotion-based-blending-in-unreal-engine?application_version=5.6).
Exact runtime compatibility must follow the pinned 5.6.1 game and local engine
evidence, not an assumption from the generic example.

Beacon requires a departure and arrival sequence as one option. Mod metadata
cannot replace notify timing, gameplay activation, montage slots or ability
ownership. Adapt the pair to the game-owned event schedule described in
[the source investigation](eve-animation-sources.md#mortal-shell-integration-findings).
No arbitrary game montage pointer or user-supplied event schedule is accepted.

The reader rejects unknown slot/option fields, duplicate or reserved IDs,
invalid paths, invalid weapon maps, missing beacon halves, unsupported mixed
fields and more than 64 options per slot. This makes spelling errors visible
instead of silently selecting a different behavior. Existing installed-package
isolation still rejects only the malformed package; loose authoring catalogs
remain strict.

## Saved choices and profiles

`animation_choices` is keyed by outfit ID, then variant ID, then slot:

```json
{
  "animation_choices": {
    "eins0fx.seduxtress": {
      "black_pearl": {
        "idle": "relaxed",
        "walk": "original",
        "jog": "eve",
        "sprint": "eve",
        "beacon": "graceful"
      }
    }
  }
}
```

State and named profiles preserve the entire map. Saving and loading a profile
copies it alongside the existing selections and legacy walk setting. Legacy
files without this block start with no custom choices. IDs for uninstalled
mods or removed options survive a save round trip, but resolution falls back
to Default until that exact option is available again. Choices never migrate
to a different outfit or variant merely because an ID matches.

Runtime precedence to implement: an explicit slot choice belongs to the
current cosmetic outfit/variant; Default must suppress a custom override for
that slot. An absent choice retains legacy walk behavior where applicable.
`AnimationChoices::find` distinguishes absence from an explicit `original`;
do not use the convenience fallback from `get` to make that precedence decision.
Fallback must not accidentally hide weapons or retain a previous override.
The five controls should use the existing shared scrollable selector and
contextual footer. Only offer a custom option once the runtime can execute it.

## Validation and remaining integration

`css_animation_tests` covers actual catalog loading, outfit/variant inheritance,
explicit empty slots, exact weapon selection, missing-mod fallback, hidden-idle
constraints, malformed package data, state/profile round trips and old saves.
The existing `css_data` suite checks the prior catalog and state contracts.
Neither suite proves engine playback or physical controller behavior.

Evidence is in `work/anim5/`. Focused animation/data/control/startup suites and
the Windows `css_core` build pass. Source labels, contextual action text and
the walk-restored message now say Default. The on-disk save value stays
`normal`. The running game has not received this revision.

Next implement asset/class/skeleton checks, safe animation ownership and
restoration, gait transitions and idle weapon visibility, then connect the
LOCOMOTION selector. Beacon needs the verified live arrival/cancel route.
Cooked Eve blend spaces, accepted contacts and in-world motion review remain
required. The fitted animation candidates are still isolated in `AnimLab`.
