# New native character coverage on build 26073152

CSS's wearable roster grows from 74 to 78 variants. The new entries are:

| Entry | Source | CSS animation path |
| --- | --- | --- |
| Tarsus, the Frostbound Martyr | New `Sparta/Characters/Bosses/Tarsus/SK_Tarsus` model | Native human skeleton |
| Halberd Bro | New `SK_HalberdBro` model | Native human skeleton |
| Twin Sword Bro | New `SK_TwinSwordBro` model | Native human skeleton |
| Laughing Flayer | Existing Sicario mesh with four native miniboss materials | Existing Sicario animation path |

Tarsus and Laughing Flayer use names read from the updated character data.
The two ordinary enemy labels follow their internal asset names. The older
MS1 Tarsus adaptation keeps its existing selection ID and remains available.
No shared assets or rig allowlists change for these additions.

## Discovery and verification

The retained pre-update file inventory was compared case-insensitively with
the current base-game inventory. Export headers were indexed for all 4,066
added packages, independent of filename conventions. They contain four new
skeletal meshes: these three characters and the Smoldering Mace weapon. No
additional NPC character mesh appears in that delta.

The three new character meshes each use `SKEL_Human_Skeleton`, have 153 bones
and four render LODs. Every bone name and parent matches the current shared
human skeleton. Bind transforms are finite with normalized rotations, and
cooked skin weights sum to one within 0.0001. Their native post-process
graphs retain their corrective pose drivers. No bind pose or animation data
was rewritten. These offline checks establish the existing animation path;
moving poses, proportions and weapon handling still need in-game acceptance.

The new models' 17 unique materials and Laughing Flayer's four material
overrides pass shader checks. The complete roster passes 1,452 dependency
packages, 77 unique meshes and active material-slot coverage for all 78
variants. Two additional existing ghost overrides also pass shader checks.

Non-CSS recognition now covers 55 character folders and 9,366 packages.
Production scanner/synthesis fixtures for the three new folders fail with
the previous table and pass with the updated table. All 31 native host suites
pass. Existing roster IDs, material overrides and boss scale factors remain.

## Other new actor definitions

| Actor | Finding |
| --- | --- |
| Rekindled Offspring | Reuses the already supported Offspring body with actor-owned fire effects. The existing appearance covers the body; CSS does not recreate its fire actor or combat behavior. |
| Halberd Sheep | Reuses a 72-bone sheep rig with a separate skeleton and different limb chains. Requires a dedicated adaptation before player locomotion can be supported. |
| Laughing Flayer | Inherits `BP_Sicario`, whose component uses `SK_Sicario`. The added appearance retains the four native dissolve-material assignments. Combat phase transitions and enemy abilities are not part of an appearance swap. |
| Solomon ghost | A player ability's visual actor using the existing Solomon body and summon effects. It is not a new NPC body or boss. |

Previously deferred multipart and nonhuman bosses remain deferred for the
reasons in [the boss adaptation audit](2026-10-15-boss-appearances.md).

## Reproduction

Evidence is under `work/npc-update-26073152/`:

- `added-exports.json`: class-index scan of every added package, using
  `AssetReadback --export-index CONTAINERS MAPPING FILE_LIST OUTPUT`.
- `native-definitions.json`, `extra-actors.json`, `variant-details.json` and
  `flayer-parents.json`: native names, inheritance, mesh and material assignments.
- `new-mesh-audit.json`, `rig-packages.json`, `rig-checks.json` and each
  `SK_*/skin-audit.json`: rig, bind, skin-weight and native pose-driver evidence.
- `new-shaders.json`, `override-shaders.json`, `final-roster-audit.json`,
  `material-slots.json`: final asset and material validation.
- `recognition-before.log`, `recognition-after.log`, `host-tests.log`: scanner
  regression and native test results.

`packaging/npc-roster-additions.json` preserves the verified new entries for
`tools/build_npc_roster.py`. Regenerate the recognition table afterward with
`tools/build_replacement_targets.py --files work/non-css-26073152/base-files.txt
--game-build 26073152`. Runtime delivery remains unpublished CSS 1.0.2.
