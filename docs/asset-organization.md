# Authored asset organization

New authored content uses `/Game/CSS/<author>/<project>/...`. Author and project
directory names use ASCII letters, digits and underscores. Each author owns a
separate namespace; the folder convention does not require their display name
or catalog ID to match the directory name.

For example, another author can use `/Game/CSS/another_author/MyOutfit/`
for their outfit and `/Game/CSS/another_author/Shared/Locomotion/` for their
shared library. Use a stable author directory across releases. This convention
does not make another author's skeleton compatible automatically; compatibility
still needs asset evidence and explicit bindings.

For the current eins0fx migration:

| Content | Destination root | Container owner |
| --- | --- | --- |
| Eve outfits and facial meshes | `/Game/CSS/eins0fx/EveSB/` | Eve mod |
| Unholy Genessa and facial meshes | `/Game/CSS/eins0fx/UnholyGenessa/` | Unholy Genessa mod |
| Commander White and facial meshes | `/Game/CSS/eins0fx/CommanderWhite/` | Commander White mod |
| Reusable skeletons | `/Game/CSS/eins0fx/Shared/Skeletons/` | `CSS_SharedAssets_P` |
| Shared animation library | `/Game/CSS/eins0fx/Shared/Locomotion/` | `CSS_SharedAssets_P` |
| Astral resources | `/Game/CSS/eins0fx/Shared/Astral/` | `CSS_SharedAssets_P` |

Animation-library catalogs accept assets under
`/Game/CSS/<author>/Shared/Locomotion/`, with performance adapter classes in
its `Adapters/` directory for reusable adapters. Outfit-specific playback
adapters belong under `/Game/CSS/<author>/<project>/Animations/Adapters/`.
Character facial replacement meshes belong under
`/Game/CSS/<author>/<project>/Faces/`. Exact skeleton alternatives and
source-container revision checks still apply. A path passing validation is
not proof that its asset loads or is compatible.

The runtime now recognizes the verified old and new eins0fx base skeleton
paths as the same animation layout. It retains the existing directed base-to-
Genessa-wing mapping. This covers legacy outfits consuming the migrated
library and migrated base meshes consuming the legacy library. Other authors'
similarly named skeletons are not added to that allowlist by folder naming.
Genessa's new wing-skeleton path remains pending its own migration checks.

Legacy `/Game/CSS/Locomotion/` catalog paths remain accepted, including their
existing `Adapters/` and `Faces/` directories. This preserves current packs
while new packages are authored. Namespace validation does not rename assets,
create aliases or infer compatibility between old and new skeleton paths.

Migration must rebuild references, preserve saved catalog IDs, and verify each
old-to-new mapping against cooked readback. Keep a tested route for old outfit
packages. Moving a facial mesh between containers also changes its resource
dependencies; update those declarations and validate isolated installations.

The current legacy `CSS_SharedAssets_P` already contains assets, including
nineteen movement clips with stale virtual-track mappings on the audited
installation. Replace it through the planned migration, not a filename-only
rename or assumed mount precedence. See the
[active goal](development/three-outfit-rig-physics-goal.md) and
[status](development/three-outfit-rig-physics-status.md).

## Initial migration map

The audited installation has 820 export records representing 790 unique package
paths. [The generated map](../work/three-outfit-rig-physics/namespace-4/asset-and-catalog-map.json)
maps every source to an author-owned destination. After verifying 61 historical
animation copies, these sources require 729 distinct target packages:

| Destination container | Unique packages |
| --- | ---: |
| `CSS_EveStellarBlade_eins0fx_P` | 308 |
| `CSS_UnholyGenessa_eins0fx_P` | 101 |
| `CSS_CommanderWhite_eins0fx_P` | 171 |
| `CSS_SharedAssets_P` | 149 |

These counts describe the migration plan, not the final release. Dependency
checks may identify unused assets to remove. Seven facial replacement meshes
and Commander White's four optical materials move to their character owners.
The five playback adapters also move to character owners: their graphs contain
the outfit-specific physics and rig nodes, not only the reusable playback input.
The common skeleton goes under `Shared/Skeletons`. Genessa's private wing
extension stays under `UnholyGenessa/Skeletons`, as required by the goal's
ownership rules. Commander White's padded
legacy paths lose their padding but retain material slot or texture hash identity;
equal basenames alone are not a reason to merge them.

The map pins the source provider for each package. It selects the corrected Eve
copies for the nineteen conflicting movement animations and refuses an unknown
provider conflict. The catalog reference list identifies 161 paths to rebuild
and eleven existing legacy aliases to retain. A private catalog candidate adds
the 22 missing Commander White and Genessa aliases, retaining their exact old
package and export names. All 63 public animation entry IDs stay
unchanged. New source-container hashes and facial dependencies must be generated
after cooking, not copied from the installed manifest.

Regenerate the plan with Python 3.14 using a new output filename:

```bash
python3 tools/diagnostics/plan_asset_namespace.py \
  --baseline work/three-outfit-rig-physics/baseline-1/baseline.json \
  --conflicts work/three-outfit-rig-physics/baseline-1/legacy-track-conflicts.json \
  --catalog work/three-outfit-rig-physics/baseline-1/installed-locomotion-manifest.json \
  --verified-aliases work/three-outfit-rig-physics/animation-alias-audit1/verified.json \
  --output work/three-outfit-rig-physics/namespace-4/asset-and-catalog-map.json
```

[Original map validation](../work/three-outfit-rig-physics/namespace-2/validation.json)
checked destination collisions, corrected provider selection and legacy aliases,
plus rejection of unassigned facial owners, unknown paths and unverified provider
conflicts. This plan does not rename editor assets, patch cooked packages or
establish runtime compatibility. Those checks remain part of the active goal.

The [alias audit](../work/three-outfit-rig-physics/animation-alias-audit1/verified.json)
compares decoded fields and compressed bone/curve hashes, not just filenames.
Its 54 animation sequences and seven blend spaces match their documented Eve
originals after package-reference normalization. Source rows remain in the map
with `deduplicated_to` and the preserved export object name. The planner refuses
changed evidence or dependency hashes. Existing installed aliases remain intact
until the complete migration passes compatibility checks.
