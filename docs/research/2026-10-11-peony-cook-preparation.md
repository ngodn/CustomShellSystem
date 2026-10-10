# Peony cook preparation

The private `peony-cookprep1` editor project has complete package dependencies
and canonical mesh paths. It is not cooked, installed or accepted for gameplay.
Walking arm contact and the running GPU cloth check remain open.

## Mesh paths and preservation

`tools/authoring-probes/rig-audit/prepare_peony_cook_meshes.py` copies the two
checked cloth-basis meshes with Unreal's asset tools:

| Source in `EveSB/Peony/Meshes` | Target under `/Game/CSS/eins0fx/EveSB/` |
| --- | --- |
| `SK_Peony_BasisStudy3` | `Peony/Meshes/SK_Peony` |
| `SK_Peony_Face_BasisStudy3` | `Faces/Peony/SK_Facial1` |

The facial target follows CSS's existing `facial_adapter_asset` ownership rule.
The study's `Peony/Meshes/SK_Peony_Face` path would not satisfy that rule. No
runtime relaxation or adapter with fabricated package hashes is added.

Authoring and fresh-process reload both exit zero. They compare source/render
geometry, weights, all 22/137 morphs, rig references, cloth data, the fifteen
Float buffers and shared deformer bindings. Cloth objects are independent
copies with equal data. Both complete mesh reports are byte-identical between
authoring and reload. Protected inputs remain unchanged.

## Dependency closure and ownership

The initial registry-only check in `peony-dependencies1` failed on three
missing editor ponytail textures, `Tail/T_ShellKeeper_Hair_01_{BC,BRM,N}`.
The replacement private project includes those existing editor sources from
`CustomShellSystem/work/three-outfit-rig-physics/eve-rig-editor2/Content`.
This does not replace the installed ponytail or authorize recooking its materials.

The fresh registry check exits zero: 36 roots, 109 packages and no missing
packages. Its roots include both canonical meshes and all palette materials.
`cook-plan.json` accounts for the complete closure:

| Treatment | Packages |
| --- | ---: |
| New Peony outfit content, including meshes and both retained cloth Physics Assets | 55 |
| Shared trim-flow function | 1 |
| Reuse accepted cooked Eve dependencies | 43 |
| Reuse accepted cooked shared dependencies | 3 |
| Editor-only references/stand-ins | 7 |

No rejected Physics Asset or `BasisStudy` mesh remains in this closure. The
seven editor-only packages must not ship. Cooked imports still need independent
verification; editor registry closure does not prove cook stripping or runtime
ownership. The existing shared deformer and skeleton retain their shared owner.

`validation-receipt.json` pins 137 retained files and the authoring script.
`preservation.json`, author/reload reports, process exits and `dependencies.json`
contain the checks above. Windows cooking/readback, facial adapter identities,
Peony ghost-material companions, fit and gameplay remain unfinished.
