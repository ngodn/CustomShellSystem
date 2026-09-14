# Outfit release checks

Run these checks for the package you will distribute. Keep the project, original inputs and audit logs locally. Put the tested CSS version, game build, variant names and known limitations in the release notes.

## Offline checks

1. Run `css_project.py check`, then `build`, or the converter and `css_package.py verify` directly.
2. Confirm every intended mesh variant is listed, with correct author, thumbnail and labels.
3. Verify every color recipe uses the outfit's stable ID, real material slots and effective parameter names. Check all variant-only resources, not just the first variant.
4. Produce the ZIP with `css_package.py zip`. Confirm one `_P` folder containing exactly its matching trio.
5. Keep personal state, caches, loose development catalogs, engine/game packages, source archives, screenshots and DLLs out of the outfit ZIP.

The converter validates inverse relocation, export identity, IoStore integrity and payload round trips. The package/ZIP verifier checks hashes and archive contents. These checks do not validate skin weights, shader appearance or game logic.

## In-game checks

Install with the game closed. Use a backed-up test setup and remove the ordinary replacement version while checking the CSS version independently. Launch with the intended CSS runtime.

| Check | Pass condition |
| --- | --- |
| Discovery and cache | One entry, correct author portrait; a fresh package cache rebuilds |
| Every mesh variant | Correct geometry/materials, no missing dependencies or unintended base replacements |
| Original | Restores the variant's authored materials and the player's original appearance when requested |
| Every color control | Only intended regions change; normals, opacity and unrelated effects remain correct |
| Palettes and templates | Colors survive save/reload, variant changes and template selection without stale textures |
| Animation | Idle, walking, running, dodge and attacks preserve hands, feet and garment fit |
| Sidearms | Draw, aim, fire, release, stow and change sidearm in Inventory |
| Secondary motion | Cloth, hair and body motion remain stable in gameplay and character preview |
| Transitions | Inventory open/close, shell changes, damage, death, beacon cancel and travel recover correctly |
| Larger collections | Scrolling, selection, thumbnails and colors work alongside other CSS packages |

Do not require unlocking every gameplay shell merely to test cosmetics. Test the gameplay shells you claim to support, because animations, attachments and proportions can differ even on a shared skeleton.

## Troubleshooting

| Symptom | First check |
| --- | --- |
| No wardrobe entry | Correct CSS runtime, matching trio under `MortalShell2/Content/Paks/~mods/`, restart after installation, embedded manifest |
| Duplicate outfit error | Two packages or a loose development catalog use the same stable ID |
| Different skeleton refused | Mesh references a duplicate/imported skeleton instead of the player's actual game Skeleton asset |
| Checkerboard or wrong sections | Mesh material defaults versus component overrides; missing materials/shader resources |
| Slider value changes but image does not | Parameter spelling, slot, association, static shader branch and mask UVs |
| Color leaks onto skin or face | Overlapping masks, wrong atlas or a recipe carried over from another variant |
| Floating feet, clipping or bad elbows | Geometry, bind transforms and weights; packaging is not a fitting operation |
| Works until reload/travel | Missing dependency, stale material/state interaction or runtime recovery issue; capture logs before resetting |
| A source conversion fails | Inspect the saved converter log and original asset paths; do not suppress errors or guess import repairs |

When reporting a runtime problem, include CSS version, game build, UE4SS version, other installed mods, the exact package filenames and the steps that trigger it. `CSS.log` and `runtime/status.json` help distinguish discovery, compatibility and lifecycle problems. Keep private save files out of public bug reports unless deliberately shared.
