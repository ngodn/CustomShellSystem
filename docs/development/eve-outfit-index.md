# Eve outfit evidence index

Updated 2026-09-26. Start here after a context reset.

## Findings to preserve

- Five palette entries do not prove five working garment palettes. All ten Gemini additions currently redirect garment palette colors to hair.
- Preserve the accepted original body proportions, V44 hand repair, camera translation repair and corrected S1 movement while extending outfits.
- The user requires five additional clothing palettes plus Original for every outfit, not five saved looks.
- Claude Code owns concurrent CSS UI/UX and performance changes. Isolate asset work and stage explicit paths only.
- Gemini's improved custom skeleton is the baseline. Its editor reload report records 386 bones, 82 sockets and nine virtual bones; verify the cooked copy separately and never silently revert to the older 379-bone skeleton.
- Latest user priority is outfit fitting, starting with Holiday Reveler. Palette work is parked. `Variants_Fixed` has fully unweighted sleeve/leg/underwear/Christmas heel objects and mostly unweighted dress vertices; do not export it as a repaired candidate without resolving this.

## Failed approaches and traps

- Do not redirect unsupported garment colors to hair. It hides missing customization and produces the reported behavior.
- Do not assume generic object-name matching or first-material selection identifies hair versus accessories.
- Do not rerun Gemini fitting/packaging scripts blindly: some overwrite source blends or delete staging directories.
- Do not accept `saved=1` followed by process termination as proof of a complete successful cook.
- Do not hot reload a DLL for this audit; earlier animation work crashed that way.

## Evidence map

Paths in this table are relative to the workspace root unless linked.

| Artifact | Finding or purpose |
| --- | --- |
| [Current goal/status](eve-outfit-goal.md) | Scope, acceptance criteria, takeover findings and next steps |
| `CustomShellSystem/tools/eve_source_audit.py` | Distinguishes palette count from declared garment binding coverage; not a visual acceptance test |
| `CustomShellSystem/tools/authoring-probes/eve_inventory.py` | Read-only Blender object, material, weight and skeleton inventory |
| `CustomShellSystem/work/eve26/installed-manifest.json` | Metadata extracted from the installed Eve pak |
| `CustomShellSystem/work/eve26/installed-audit-v2.json` | Installed manifest semantic palette audit |
| `CustomShellSystem/work/eve26/blend-inventory.json` | Gemini Variants_Fixed blend inventory, with source digest |
| `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/generate_variants_manifest.py` | Removes garment controls and remaps suit palettes to hair |
| `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/disk_verification_report.json` | Gemini's fresh editor reload evidence for 386 bones, 82 sockets and nine virtual bones |
| `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/sockets_and_bones_payload.json` | Added bone transforms/modes and socket definitions; preserve when exporting |
| `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/fit_holiday_reveler_mastery.py` | In-place fitting and weight transfer; preserve source before adapting |
| `CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/gemini-work/renders_xmas_fitted_check` | Offline front/rear fitting evidence, not game material verification |
| [Camera repair](animation-camera-repair.md) | Prior accepted riposte/trap framing correction |
| [Modding colors](../modding/colors.md) | Dye layer format and material binding contract |

References already reviewed: local cloth-physics-guide, MustardUI README and CommanderWhite Daz/MHX addon inventory. MustardUI is an authoring interface, not proof of Unreal cloth export support. Use matching UE 5.6.1 source for Chaos implementation details.
