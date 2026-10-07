# Authored asset organization

New authored content uses `/Game/CSS/<author>/<project>/...`. Author and project
directory names use ASCII letters, digits and underscores. Each author owns a
separate namespace; the folder convention does not require their display name
or catalog ID to match the directory name.

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
its `Adapters/` directory. Character facial replacement meshes belong under
`/Game/CSS/<author>/<project>/Faces/`. Exact skeleton alternatives and
source-container revision checks still apply. A path passing validation is
not proof that its asset loads or is compatible.

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
