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
Full Blender-source reproduction, intermediate namespace/cloth assembly,
remaining rig/material provenance and runtime/performance coverage remain
separate requirements. The earlier facial-preservation record already documents
the permitted oral topology additions; this replay does not change their design.
