# Six original Eve outfits: retained imports match the installed geometry

All six normal meshes pass the existing import-to-cooked comparison against the
October 14 installed Eve package. Planet Diving, Bikini, Knitwear and Midsummer
Alice gain this coverage. Black Pearl and Skin Suit were checked again because
their earlier reports referenced an older package.

| Appearance | Retained import, relative to CSS/work | Source points | Cooked vertices | Triangles |
| --- | --- | ---: | ---: | ---: |
| Black Pearl | `grip-grounding-v1/heel-support-export-v2/candidate.mesh.json` | 133,234 | 558,243 | 193,589 |
| Planet Diving | `eve26/planet-f14-import/planet.mesh.json` | 123,959 | 510,980 | 175,142 |
| Skin Suit | `eve26/skin-f16-import/skin.mesh.json` | 114,373 | 444,427 | 153,507 |
| Bikini | `eve26/bikini-import1/bikini.mesh.json` | 137,402 | 569,795 | 200,619 |
| Knitwear | `eve26/knit-w2-import/knit.mesh.json` | 146,464 | 616,658 | 216,613 |
| Midsummer Alice | `eve26/alice-import4/alice.mesh.json` | 147,947 | 631,411 | 220,611 |

Each mesh retains 379 bind bones and 22 positional morphs. Position, UV0 and
material-slot correspondence is exact after float32 conversion. Triangle
multiplicity and winding match. Bind transforms meet the checker's float
conversion bounds. A single source point must explain both the cooked weights
and every positional morph at each matching render vertex.

Weights are consistently normalized to 255 in the decoded byte representation.
The largest influence difference is 0.0195621, within the existing bound based
on the source influence count and eight-bit conversion. The largest stored
morph-component difference is 0.00200665 cm; the largest omitted source
component is 0.01499206 cm. These are bounded cooking differences, not byte
equality. The algorithm and local UE 5.6.1 implementation evidence are documented
in [Skin Suit's source record](../../../CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/authoring/skin-suit-source-provenance.md#import-payload-to-current-cooked-mesh).
The checker is unchanged since its winding, weight and morph mutation tests;
all three retained negative cases rejected their altered input.

## Evidence and reproduction

The [six-outfit receipt](../../work/three-outfit-rig-physics/eve-six-current-cooked-provenance.json)
pins each comparison report. Individual folders retain separate skin and morph
readbacks, commands, logs, exit statuses, input hashes and unchanged-input
checks. They use the existing
[installed inventory](../development/three-outfit-installed-inventory.md) mount.
The six checks retain approximately 398 MiB of readback evidence, without copies
of whole containers or authoring projects.

Installed Eve identities:

- PAK: `d0ff238fd26f692f9f4f7eadb7f8b96240cd05b26023b589c773d4b6212a79d4`
- UTOC: `17307bca225d0b3e1a7678ba27c32c771c975dced11d7710e50a94e1a6e20068`
- UCAS: `31eae2443d26d1b8f224db8850fe9970815e3da14186d7021d91cdb25e5b9053`

[`run_import_cooked_check.py`](../../tools/diagnostics/run_import_cooked_check.py)
runs the existing MeshExport skin/morph modes and
[`check_import_cooked_mesh.py`](../../tools/diagnostics/check_import_cooked_mesh.py).
Supply the inventory JSON, import file from the table, normal mesh package path
from the inventory, a new output directory under CSS/work, the existing .NET
10.0.401 binary and the game mappings. It uses Python 3.14.7 with NumPy. It
refuses an existing output directory and checks pinned files before and after
comparison. Planet Diving used the same commands before this runner was added;
its exact commands and protected-input check are retained separately.

## Remaining scope

This closes the listed LOD0 import-to-cooked fields for the six original normal
meshes at the pinned installed revision. It does not complete the earlier
Blender/export chain, facial replacements, normals/tangents, other UV channels,
colors, material graphs, cloth simulation, posed fit or gameplay regression.
Peony and the other two outfit mods retain their separate evidence and open
requirements. Nothing was cooked or installed, and the CW walking trial remains
untouched and awaiting visual acceptance.
