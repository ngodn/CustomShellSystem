# Proxima Slayer Armor compatibility, October 22

CSS 1.0.2 already recognizes this armor as a Proxima non-CSS replacement.
Its skeleton and shader checks pass, but the supplied archive is incomplete:
four referenced texture packages are missing. This prevents a clean asset
compatibility result. No CSS release change is needed for recognition.

Input: `/home/eins0fx/Downloads/Proxima - Slayer Armor 571 1.0 2026-10-19T12-10Z vE77wqoFN.rar`.
SHA-256: `65fa072e2e820a4b5d5a60b73fa1fcda337bb9283e99576c6497c77fce5aa3d7`.
Game build: 26073152. Evidence: `work/slayer-26073152/`.
The original archive and extracted files remain byte-identical and uninstalled.
The game stayed closed; no desktop input or focus change was used.

## Package and CSS checks

The archive contains two independent PAK/UTOC/UCAS sets:

| Component | Packages | Current-game overrides |
| --- | ---: | --- |
| ProximaSlayer_P | 29 | Proxima's SK_Shell_KnightLady_V04 and PA_Shell_KnightLady |
| ProximaSlayerSword_P | 21 | Tarsus_Sword_StaticMesh and SM_GiantSword_AltSkin |

Both containers pass Retoc integrity verification. Export headers identify one
skeletal mesh, two static meshes, one physics asset, 32 materials and 14 textures.
The weapon is the author's separate Great Martyr's Blade replacement, including
its alternate mesh. It correctly does not create a CSS character choice.

The production CSS scanner creates `css.mod.proximaslayer_p`, with one Proxima
variant, and reports no recognition or shared-asset conflict errors. Effective
package ownership selects the four intended replacements above the base game.
There are no equal-priority Slayer overlaps with the current installed mod mount.
No shared human skeleton or animation blueprint is replaced. The armor references
the current native human skeleton and Proxima post-process blueprint.

Slayer and the separately supplied Phoenix archive replace the same Proxima
mesh. They cannot provide independently selectable versions of that same asset
when mounted together. CSS recognition does not isolate competing package
overrides. Use one Proxima replacement at a time.

## Missing textures

The strict public-import audit finds 19 unresolved references across 17 materials.
These resolve to four missing package IDs, independently matched to their paths
with the game's package-name hash. All four are absent from the supplied archive,
the updated base inventory and the retained pre-update inventory.

| Missing texture | Package ID | Materials referencing it |
| --- | --- | --- |
| `/Game/Sparta/Characters/Shells/KnightLady/Art/Slayer/SlayerArmure_R` | ad37ed15a2e121df | MI_SlayerArmure |
| `/Game/Materials_Frabric/Textures/Fabric_carpet_01/Fabric_carpet_01_metallic` | a60baf8c7218acdc | Both MI_SlayerCapeCheveux materials |
| `/Game/Materials_Frabric/Textures/Fabric_carpet_01/Fabric_carpet_01_normal` | c111f7d0362a84db | Both MI_SlayerCapeCheveux materials |
| `/Game/Sparta/Weapons/Player/SlayerArme/SlayerArme_ORM` | 73c92f1d92974909 | MI_SlayerArme1 through MI_SlayerArme14 |

Full material readback also shows 19 null entries in their cached runtime texture
references. These are not merely unused names found in package headers. The three
affected armor slots cover 54,626 of its 68,066 triangles. The affected weapon
slots cover 176 of 2,433 triangles in each weapon mesh. These counts describe
sections using incomplete materials, not a measured amount of invisible geometry.
The actual fallback appearance needs an in-game check.

All 32 materials parse successfully and carry 68 inline SM5/SM6 shader maps.
They do not depend on the missing shared shader hashes that caused the earlier
Eve update failure. Every supplied texture's cooked mip payload reads and hashes
successfully. Passing shader checks does not supply the missing textures.

Armor alone against the base game reproduces five unresolved imports; weapon
alone reproduces 14. The combined current CSS/mod mount gives the same 19.
This rules out a dependency on the other component and does not establish an
October-update regression. The author needs to supply the missing textures or
correct the material references. The included armor ORM and weapon R textures
are not evidence of equivalent channel packing, so no guessed substitutions
were made or added to CSS Shared Assets.

## Rig and render data

The armor has 71,503 vertices, 68,066 triangles, 17 material slots and one render
LOD. All active slots resolve to material assets. Its 166-bone mesh preserves all
95 native Proxima bone names and their parent relationships. Their maximum local
bind-translation component difference is 0.000654 cm. Bone transforms are finite,
rotation norms pass, and cooked skin weights sum to one within 0.00001.
All 20 physics-body bones and the 19 constraints' bone endpoints exist in the mesh.
Two cloth assets are embedded; cloth simulation was not exercised.

Each weapon mesh has 3,566 vertices, 2,433 triangles, 15 material slots and one
render LOD. All active slots resolve. These checks do not establish animation,
weapon grip, cloth behavior, frame rate or gameplay acceptance. The included
readme already notes occasional hair behavior problems and excessive armor glow
in dark areas; those are author disclosures, not newly reproduced findings.

## Evidence

- `input.json`: input hashes and extracted-file manifest.
- `armor-files.txt`, `weapon-files.txt`, `exports.json`, `base-overrides.json`:
  content and actual replacement coverage.
- `recognition.json`: production scanner and generated CSS choice.
- `appearance-combined.json`, `appearance-armor-base.json`,
  `appearance-weapon-base.json`: effective dependencies and unresolved imports.
- `shaders.json`, `materials.json`, `textures.json`: shader maps, cached texture
  references and all supplied mip payloads.
- `skin/`, `base-skin/`, `readback.json`: rig, skin, physics and weapon evidence.
- `missing-paths.json`, `summarize.py`, `summary.json`: package-ID confirmation,
  inventory checks, active-section counts and final assertions.

CSS 1.0.2 and its release archive remain unchanged. The missing-texture finding
belongs to this third-party package. A complete source/archive and a later
in-game visual check are needed before marking it fully compatible.
