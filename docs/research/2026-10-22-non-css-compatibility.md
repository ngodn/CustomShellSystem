# Non-CSS and native appearance audit, October 22

Game build: 26073152. Evidence: `work/non-css-26073152/`. This extends the
[outfit shader investigation](2026-10-22-update-26073152.md). The user confirmed
CSS 1.0.2 has not been uploaded, so the recognition correction belongs in 1.0.2.
No public release, game launch, desktop focus change or input injection is part
of this audit.

## Recognition fix

The old lookup table omitted the Shopkeeper folder used by Merrick and several
new assets. Rebuilding against the updated base containers adds 317 package IDs,
removes 76 obsolete BallistaHead IDs and covers 52 folders / 9,117 packages.
The 74 shared conflict entries are unchanged. The source inventory hash and
Steam build are recorded in `packaging/catalog/replacement-targets.json`.

`css_replacement_tests --shipped-catalog <table>` runs the real scanner and
outfit synthesis against a fixture containing Merrick's actual package ID.
It fails with the earlier table ("Merrick replacement missing from Use Non-CSS
Mod") and passes with the update. All 30 native host suites pass. The builder
now requires an explicit inventory and game build on its command line.

The production scanner also recognizes the untouched Phoenix archive as one
Proxima choice and GenessaDefault as four Genessa looks. GenessaHipSealFit edits
character data, and CSSX CINE supplies effect materials; neither represents a
separate wearable mesh. Both correctly stay out of the appearance picker.

## Checked coverage

| Input | Result |
| --- | --- |
| 74 CSS NPC/enemy/boss roster variants and 10 official shell meshes | 84 decoded meshes, 1,548 dependency packages, zero unresolved imports |
| Installed non-CSS containers: GenessaDefault, GenessaHipSealFit and CSSX CINE | 22 root packages, 16,648 packages including transitive imports, zero unresolved imports |
| Roster plus Phoenix dependency shader check | 1,552 packages, 290 materials, 161 shared shader hashes, zero missing shaders |
| Baseline-only dependencies omitted by Phoenix | 20 packages, two materials, two shared shader hashes, zero missing shaders |
| Phoenix original archive | Recognition and shaders pass; 13 missing material packages prevent a clean dependency result |

Dependency resolution alone missed two native meshes with explicitly empty
material slots. The additional active-section check fails on William's body and
Ball Boy's second look before the catalog correction and passes all 74 roster
variants afterward. Official shell meshes have no missing active materials.

William's memory blueprints inherit `BP_NPC_Ghost` / `BPC_GhostFX`; the latter
assigns `MI_Ghost_Material_V5_Character`. CSS now assigns that primary ghost
material to his three slots. Its `MatGlobalMult` default is 1.2, not the zero
value in the older `MI_Ghost_Character_` material. Ball Boy's memory blueprint
assigns `MI_Ghost_CharacterHostile`; the second-look mesh has one empty ghost
slot, which now uses that native hostile material. Both materials and their
shared shader resources pass readback. This reproduces the native material
selection, not the actors' full fade, particle and secondary-layer effects.
Their final in-game appearance still needs visual acceptance.

`check_native_material_slots.py` checks actual cooked section indices and catalog
overrides against successful shader readback. Its before/after results are
`native-material-slots-before.json` and `native-material-slots-after.json`.
The roster builder retains the overrides so regeneration cannot silently erase
them. Blueprint evidence is in `native-empty-material-owners.json`,
`william-owners.json`, `ghost-material-owners.json` and `ghost-defaults.json`.

The 26 previously discovered native shell choices share ten mesh assets. This
check covers those meshes; it does not repeat all 16 shade unlock/material
selections in-game. Four boss choices use CSS-adapted meshes under `/Game/CSS/`.
Their assets and dependencies pass, but that does not make arbitrary third-party
replacements of the original boss meshes compatible with the adapted skeletons.
Uninstalled mods and every creature elsewhere in the game are outside this
inventory. Parsing cannot establish gameplay, visual or physics acceptance.

The mounted inventory also has one equal-priority package overlap between the
two Seductress versions. That package is outside the native appearance dependency
closure; its existence is recorded in the audit outputs, not presented as a
reproduced rendering defect.

## Phoenix findings

Archive supplied by the user:
`/home/eins0fx/Downloads/Phoenix Proxima 2 P 531 1 2026-09-25T11-55Z 3CRRmKTmx.rar`.
SHA-256: `563006c12306a1121b146be20ea8a97c05ae3bcb6f3e88dd19cb8f976acbac4e`.
The original PAK/UTOC/UCAS remain unchanged and uninstalled.

The lowercase `_p` suffix is recognized. Its foreign project root
`mortal_proximanude/Content` is not itself an incompatibility: IoStore package
IDs resolve the Proxima replacement. It has 77,159 vertices, 142,534 triangles,
18 material slots and one render LOD. Its human skeleton reference resolves.

Slots 0 through 12 reference 13 packages missing from the supplied archive and
both game inventories. All 13 have enabled render sections, totalling 450
triangles. The remaining five material references resolve. This is not a shader
map failure, and the evidence does not establish that the October update caused
it. The material IDs are preserved in `phoenix-detail-ids.json`. Choosing arbitrary
replacement materials would change the author's appearance without a reference.
An intact archive or the missing source materials are needed for a faithful fix.

The animation blueprint under `Humans/_Shared` is new content, not an override of
the current game's blueprint. The initial suspicion of a global animation
conflict was rejected after comparing package IDs against the base inventory.
The production scanner reports no shared conflict for Phoenix.

## Audit implementation and evidence

`tools/AppearanceCompatibilityAudit` uses the retained patched CUE4Parse checkout
and .NET 10. It resolves top-level assets by package ID, walks public imports,
checks requested exports, and decodes mesh skeletons, material slots and render
sections. It verifies export hashes against the effective imported package,
without treating CUE4Parse's fallback to an older package as a successful import.

The retained parser fills its package-ID index in concurrent mount order.
The audit rebuilds that index in patch-priority order before loading any package.
Earlier `appearance-*.json` files without `ordered` or `final` in their names
selected some base assets instead of replacements and are not acceptance evidence.
The parser checkout itself was not changed.

Accepted evidence files:

- `appearance-baseline-final.json`: 84 baseline meshes and dependency closure.
- `installed-non-css.json`: all installed non-CSS package roots and their closure.
- `appearance-phoenix-final.json`, `phoenix-detail-ids.json`: Phoenix failures,
  effective owners, material sections and missing package IDs.
- `appearance-shaders-final.json`, `baseline-only-shaders.json`: shader results.
- `native-recognition.json`: actual scanner and synthesized choices.
- `recognition-before.log`, `recognition-after.log`, `host-tests.log`: regression
  and host-test results.
- `native-ghost-overrides-shaders.json`, `native-material-slots-after.json`:
  catalog ghost materials and complete active-section coverage.

Run the tool with a flat container mount, the current mapping, a text file of
unique `/Game/Package` or `/Game/Package.Export` references, and a new output path:

```sh
dotnet run --project tools/AppearanceCompatibilityAudit -c Release -- \
  CONTAINERS MAPPINGS REFERENCES NEW_OUTPUT_JSON
python3 tools/build_replacement_targets.py \
  --files work/non-css-26073152/base-files.txt --game-build 26073152
ctest --test-dir build/uuepbs-host --output-on-failure
```

CW frame-rate comparison, body-motion feel and additional Genessa skirt morph
deformation remain separate open tasks. This audit makes no claim to fix them.
