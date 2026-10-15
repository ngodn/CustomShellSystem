# Genessa W3 editable source continuity

Both retained W3 editor meshes exactly match their original creation receipts,
the namespace migration inputs and the facial integration inputs. The original
wing comparison also reruns successfully against its full saved data, producing
the same report bytes. This connects the accepted editable outfit baseline to
the already verified current package lineage without rebuilding its design.

Evidence: `work/three-outfit-rig-physics/genessa-w3-sources-20261015/verified.json`.
Recipe: `tools/diagnostics/verify_genessa_w3_sources.py`, Python 3.14.7.
The completed run exits zero and verifies 2,220 distinct retained files,
including the downstream material/rig and installed-package audit inputs.

| Editable mesh | SHA-256 |
| --- | --- |
| SK_EveW3.uasset | `748f436491caac2def9843f4a2c1759be36eb2017e76fe9b8b71c31ee3fd7e0b` |
| SK_GenessaW3.uasset | `7c3874dab8b0d8067c5c51f329b792c703cf565bc17b8714d8323cfdf351d2f9` |

The source files remain under the Genessa author's
`work/ue/Content/CSS/UnholyGenessa/`. The original W3 creation receipt preserves
the F4 meshes, W2 meshes, wing skeleton, graph and reference animations. All 15
protected inputs match. The seven F4/foundation inputs also match the original
hinge comparison, and the 14 pose-check inputs remain intact.

The unmodified `check_wing_hinges.py` compares both saved W2 readbacks with F4.
It permits the four intended wing weight names and skeleton references, then
requires equality of the remaining geometry, morph, cloth and collision data.
The replay mounts the filesystem read-only, with only a new report file writable
at the checker's original output path. The original report is unchanged.

The W3 creation recipe duplicates W2 and assigns its compiled wing graph.
Its two recorded output hashes match the actual editable assets and both later
consumers. Retained Python and C++ wing recipes are inventoried; their presence
is not presented as a new editor build. The downstream audit receipts are
hash-pinned and all their protected inputs rechecked, including installed files.

This supplies the retained W3 baseline link for the
[material/rig source chain](2026-10-15-genessa-material-rig-sources.md),
[facial authoring chain](2026-10-15-genessa-facial-source-replay.md) and
[current installed assembly](2026-10-15-genessa-package-lineage.md).
Those records distinguish original sources from later cloth and graph overlays.

## Release scope

The goal requires reproducible assets and complete source-to-package provenance.
It does not require recreating every accepted historical outfit from its first
Blender scene. Editable W3 assets are retained source inputs, with the subsequent
recipes, cooks and package overlays accounted for. A fresh whole-outfit Blender
export or repeated UE cook is not an additional acceptance gate unless missing
inputs, a failed reproduction or a source change makes one necessary.

This check makes no new appearance, performance, game-state or final release
claim. Genessa's remaining gameplay/lifecycle coverage and coordinated stable
packaging remain open. No game files, settings or accepted source files changed.
Only a small comparison report, recipe, command, log and receipt were generated;
there are no heavy duplicate cooks or assets to remove.
