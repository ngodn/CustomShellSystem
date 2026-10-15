# Retained boss authoring recipes

These are unchanged copies of the October 15 recipes executed under
`work/npc-appearances/bosses`. `recipe-sources.json` records their original
locations and hashes. They retain the local absolute workspace paths and expect
the extracted native inputs, selected plan and authoring project described in
[the boss report](../../../docs/research/2026-10-15-boss-appearances.md).
They are source preservation, not a one-command installer.

The accepted path is:

1. `configure-isolated.py` configures separate Skeletons while retaining native
   bind transforms and evaluates the retained player animation.
2. `clear-preview.py` calls `CSSNpcLibrary.ClearBossPreview` in the UE 5.6.1
   authoring commandlet before the retained Windows cook.
3. `package.py` relocates native mesh references and assembles the isolated
   trial with the cooked Skeletons. It refuses to overwrite its output folder.
4. `verify.py` compares the decoded native mesh fields, binds and translation
   modes. It writes the trial verification receipt.
5. [add-boss-appearances.py](../../shared-assets/add-boss-appearances.py) selects
   the four accepted bosses and integrates them into the combined shared pack.

The trial contains five rigs. Orrem was rejected in the game because his body
requires additional native parts. He is excluded by the final shared-pack
specification and deferred to v1.0.1. Do not install the trial as the release.

The separate `prepare_npc_mesh.py` and `configure_npc_mesh.py` tools retain the
earlier Merrick/Zhirelle authoring experiments. The current Merrick appearance
uses his original native mesh, and Zhirelle remains deferred. Those experiments
are not additional release assets.

The tracked `CSSNpcLibrary` source matches the source in the actual editor
project used by the retained cook command. The existing cook, package and live
receipts remain authoritative; tracking these files does not claim a new cook
or another gameplay check.
