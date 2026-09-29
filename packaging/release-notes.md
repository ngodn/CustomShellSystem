### CSS 1.0.0-beta.6

### Added

- **Use Non-CSS Mod**, a fifth Appearance row on the SHELL tab. It lists every replacement mod installed under `Content/Paks` that changes a shell, enemy or NPC (a container without CSS metadata that ships game assets at their original paths) and wears that character's look on your current shell, the way Use NPC / Enemy does. Left / Right walk the looks, the details window groups them per mod, and Search mods opens the picker. Weapon, effect and other mods are left out. A mod over the shell you are wearing is noted as already shown by the game; two mods over one character are noted as well.
- `catalog/replacement-targets.json`, the table CSS reads a mod's package ids against: every game package under the 51 character folders it can wear from, 8,876 packages, built from the game data by `tools/build_replacement_targets.py`. Recognizing a mod reads a few KB of its `.utoc` and nothing else.
- CUSTOMIZE and LOCOMOTION say what a replacement look supports: Ground height, and Default or the CSS feminine idle and walk. MISC and profiles work unchanged.
- A conflict warning, never a block: a patch container that overrides a shared asset CSS depends on (the human skeleton, the player's animation graphs, the shell base classes) is named in `CSS.log`, in `runtime/status.json` and at the top of the Use Non-CSS Mod details, since it can make every CSS look fail to apply or animate wrongly.
- `runtime/status.json` carries `catalog.replacements`, `replacement_conflicts`, `replacement_scan`, `replacement_errors` and `replacement_targets`; `CSS.log` names each recognized mod and its folders. See [Use Non-CSS Mod](../docs/replacement-mods.md).

### CSS 1.0.0-beta.5-hotfix.2

### Fixed

- Outfit packages with long dye mask names load on a default Steam install (`C:\Program Files (x86)\Steam\...`). The package cache moved from `cache/packages/<id>/<64-hex manifest hash>/` to `cache/packages/<16-hex manifest hash>/`. The old layout pushed the mask temp files of Curvy and Cute (264 characters), BTGG (265) and Beaute Knight Proxima (263) past Windows' 259-character limit, and the failed write rejected the whole package. The longest possible cached path is now 242 characters.
- Old-layout cache folders are removed on the next scan.
