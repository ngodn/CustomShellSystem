# Genessa facial input reproducibility

Both accepted W3 facial packets reproduce byte-for-byte with the retained recipe
and inputs. A new cooked comparison also passes for both installed facial meshes.
No game settings, core or assets were changed.

Evidence: `work/three-outfit-rig-physics/genessa-face-source-replay-20261015/`.
Recipe: `tools/diagnostics/verify_genessa_face_inputs.py`, Python 3.14.7.

The audit checks 39 input hashes, including the two original editable W3 meshes,
face mapping, head patch, integration packets, editor module sources and binary,
editor captures and the current installed Genessa trio. The integration record
names those exact source/patch pairs. The retained packet recipe is also identical
to the current authoring tool. Both generated outputs match their historical
hashes and full report, including 115 expression controls and seven lip-seam
duplicates per variant.

The existing independent cooked verifier is rerun against the installed-package
readbacks. It validates the archived binary buffer hashes, then compares each
facial mesh's positions, UV0, section order, indices and all 137 morphs with the
editor captures. Both pass; the installed package trio still matches the hashes
from extraction. This uses retained readbacks of unchanged installed files, not
a new extraction or cook.

The replay's two duplicate packets are removed under the
[recorded cleanup list](../development/cleanup-2026-10-15-genessa-face-replay.md).
Original inputs, exact recipes, output hashes, logs and verification remain.

This closes the exact facial-packet replay and retained authoring-module link.
Full outfit Blender-source reproduction, intermediate namespace/cloth assembly,
remaining rig/material provenance and runtime/performance coverage remain
separate requirements. The earlier facial-preservation record already documents
the permitted oral topology additions; this replay does not change their design.
The later [package-lineage audit](2026-10-15-genessa-package-lineage.md) verifies
the retained namespace/cloth/final-hair stages against the installed trio without
a new cook or package rebuild.

## Blender source through final packets

Two further runs extend the proof back to `authoring/GownEye1.blend` and the
retained Eve facial donor. The local executable reports Blender 5.2.2 LTS, build
`d13f752e3b9c`; its full version and hash are retained. Commands use background,
factory-startup, disabled auto-execution and Python exception exit codes, with
the source filesystem mounted read-only and only the replay directory writable.
Those CLI options follow the [Blender command-line documentation](https://docs.blender.org/manual/en/5.4/advanced/command_line/arguments.html)
and were exercised by the locally verified 5.2.2 executable.

- `genessa-blender-patch-replay-20261015/`: the existing patch exporter reads the
  original blend and retained expression/backing data. It reproduces the complete
  accepted head-patch bytes and report, preserving nine pinned inputs.
- `genessa-face-chain-replay-20261015/`: five stages regenerate the neutral
  surface, initial transfer, calibrated articulation, underlying head backing
  and final export. Surface, transfer and articulation payloads are byte-exact.
  Backing and final export preserve all geometry, topology, fit keys, source
  vertex mappings and morph data. Their only difference is the nested provenance
  entry naming and hashing the freshly generated `articulation/preview.blend`.

The first chain attempt stops on that provenance difference. Its failed receipt
and executed recipe remain. The resumed comparison permits only that exact
replacement after verifying both original and new preview hashes; no geometry
tolerance was added. Completed processes are reused after checking their commands,
recipe hashes and output hashes. Only the remaining export is executed.

Twelve corrupted records are rejected for the intended reason: position,
triangle, morph delta, source vertex mapping, preview hash and extra input,
each tested against both backing and export. `comparison-controls.json` retains
the results and verifier hash.

The regenerated final patch then feeds the existing W3 packet builder. Its
private mapping copy changes only the input path/hash for this validated patch;
all correspondence data and original sources remain unchanged. Both resulting
packets match the accepted hashes exactly:

| Packet | SHA-256 |
| --- | --- |
| `SK_EveW3.json` | `eaa559919394381f08aed6684e0144efa8cf4a3bbeba08e471c1451afb6b1a4f` |
| `SK_GenessaW3.json` | `de9284ad090a0c71fd2dc153738899fcaa5997fd1bf79c515d4129a5451765b2` |

The chain preserves sixteen pinned inputs. Its accepted packets connect to the
editor-module and installed geometry/morph verification above. This closes the
facial data-generation chain from the editable Genessa source and retained donor;
it does not claim a fresh UE cook or reproduction of the entire outfit or donor.
Recipes are `verify_genessa_blender_patch.py` and
`replay_genessa_face_authoring.py` under `tools/diagnostics`.

Regenerable duplicates and unused scratch renders are removed under the
[Blender replay cleanup list](../development/cleanup-2026-10-15-genessa-blender-replay.md).
The generated articulation preview and all small provenance/failure records remain.
