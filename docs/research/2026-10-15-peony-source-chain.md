# Peony V2 retained source and installed package chain

The current Eve package's 294 Peony V2 payloads match the retained character
and texture-option cooks. The twelve shared doubles material exports also
match their retained cook and the installed shared pack's preserved payloads.
The combined audit verifies 610 files. No game files or settings changed.

Evidence: `build/eve-surface-package1/peony-and-ghost-source-chain.json`.
Recipe: `tools/diagnostics/verify_peony_source_chain.py`, Python 3.14.7.

## Sources and cooks

The character assembly retains its nine input files, including the bound
garment Blender file, body interchange, facial shapes and stocking interchange.
The assembled mesh matches its recorded hash. Frozen assembly, authoring,
binding, cook and readback recipes match their receipts. The stocking source
and its material recipes are retained too.

All 121 authored character/dependency packages and seven texture-option
packages match their recorded editor inputs to the successful Windows cooks.
The complete cook inputs remain unchanged. The character's normal/facial
binding and deformation evidence remains linked by receipt hashes. This audit
does not rerun those geometry comparisons or claim a new cook.

All 294 cooked Peony V2 files match the current combined Eve plan and retained
payloads. The installed Eve `.pak`, `.utoc` and `.ucas` match that candidate's
verified identities. Earlier Eve surface repairs therefore preserve the
accepted Peony assets.

## Shared doubles materials

The ghost authoring workspace retains 17 authored packages and 147 cook inputs,
with a successful fresh editor reload and the accepted second Windows cook.
Its six parent materials and six opposite-culling instances preserve all twelve
compiled export payloads through the original Peony package and current shared
boss integration. The current installed shared trio matches the latter's proof.

The original Peony shared `.utoc` and `.ucas` are byte-identical to the baseline
used by the boss integration. That existing integration proof preserves all 283
baseline packages. IoStore reconstructs cooked headers, so the audit does not
misrepresent raw cooked headers as byte-identical to reconstructed headers.
No ghost graph or shader was rebuilt.

## Historical dependencies and diagnostic corrections

The first audit draft included the stocking check's links to base-game
containers. One no longer matched its October 12 hash. These 26 links follow
the game installation and are not retained editable outfit sources. The final
report records their historical hashes and current targets separately, without
claiming that the old game's dependency check was repeated. Current installed
Eve/shared identities are independently checked as described above.

A second draft looked for the stocking material recipes in `recipes`; their
frozen location is `recipes-material1`. The corrected audit verifies that
directory. Neither failed draft wrote a success report or changed an asset.
The earlier `peony-source-chain.json` covers Eve alone and is superseded by the
combined report, whose recorded verifier hash matches the final source.

## Recheck

Use a new output filename:

```sh
python3 tools/diagnostics/verify_peony_source_chain.py \
  --candidate build/eve-surface-package1 \
  --installed /mnt/eins0fxE/SteamLibrary/steamapps/common/Sparta/MortalShell2/Content/Paks/~mods/CSS_EveStellarBlade_eins0fx_P \
  --shared-candidate build/shared-boss-appearances1 \
  --output build/eve-surface-package1/peony-source-recheck.json
```

This closes the retained Peony assembly/editor/cook/package linkage. It does
not replay the original downloaded archives, establish new gameplay acceptance,
or complete the coordinated stable-release and runtime gates.
