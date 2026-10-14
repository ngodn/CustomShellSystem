# Installed outfit inventory, October 14

Readback completed at 05:39 UTC against the installed CW walking trial and
matching shared metadata. All ten appearances have one normal mesh and one
facial adapter. The adapters match their outfit's installed TOC hash and bulk
size. All twenty meshes resolve their skeleton, physics asset and post-process
graph to exactly one provider among the four checked packages.

| Appearance | LOD0 vertices, normal / facial | Morphs, normal / facial |
| --- | --- | --- |
| Eve Black Pearl | 558,243 / 558,243 | 22 / 137 |
| Eve Prototype | 510,980 / 510,980 | 22 / 137 |
| Eve Skin Suit | 444,427 / 444,427 | 22 / 137 |
| Eve Bikini | 569,795 / 569,795 | 22 / 137 |
| Eve Casual Sweater | 616,658 / 616,658 | 22 / 137 |
| Eve Midsummer Alice | 631,411 / 631,411 | 22 / 137 |
| Eve Peony V2 | 151,979 / 151,979 | 22 / 137 |
| Unholy Genessa, Eve | 160,794 / 162,939 | 22 / 137 |
| Unholy Genessa, Genessa | 160,794 / 162,939 | 22 / 137 |
| Commander White | 2,312,092 / 2,312,092 | 16 / 206 |

The four installed containers provide 1,021 unique exported packages: Eve 435,
Unholy Genessa 107, CW 196 and Shared 283. There are no duplicate providers
within that set. The retired separate Astral and Locomotion shared TOCs are
absent from the game's Paks tree. This ownership scan is distinct from the
walking assembly's 924-package dependency traversal.

The installed manifests still carry development versions: Eve `1.4.0`,
Unholy Genessa `0.2.0`, CW `0.0.6-dev` and Shared `0.1.0-dev`. These are
inventory observations, not the planned public release numbers. The selected
core is `css_core-cw-walk-acab6363906f0e88.dll`.

Evidence is in
[`current-inventory-20261014/inventory.json`](../../work/three-outfit-rig-physics/current-inventory-20261014/inventory.json).
It pins all twelve container files, the core, asset ownership, mesh references,
decoded surface/morph digests, reader identity and raw readback identity.
The reader exited successfully. Protected installed files and the core selector
were unchanged at the end; no game process was running at that check.

Recipe: [`current_outfit_inventory.py`](../../tools/diagnostics/current_outfit_inventory.py),
run with Python 3.14 and `--output` pointing to a new directory under `work`.
The output uses links to installed containers, not full package copies.

This verifies installed identity and ownership. It does not establish the
Blender-to-cook source chain, weight normalization, morph extremes, visible fit,
walking acceptance, lifecycle behavior or runtime cost. Earlier normalized
weight checks covered eighteen meshes at their recorded revisions; do not
extend that result to this twenty-mesh snapshot without comparing evidence.
The remaining release gates stay open in
[`three-outfit-release-gates.md`](three-outfit-release-gates.md).
