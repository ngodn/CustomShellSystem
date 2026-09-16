### Added

- The COLOR tab is rebuilt. A palette band of its own, then one section per group: parts that belong to the dress under OUTFIT and parts that belong to you under BODY. Each section opens with a Tint row that shifts everything beneath it by hue, saturation and brightness at once. Metal, gems, skin and the body's own pigments take the brightness and saturation but keep their own hue, so an outfit hue shift recolours fabric and leaves gold as gold. Tinting needs a palette: Original applies no dye, so the Tint rows are hidden there.
- Picking a part opens a strip of swatches: the author's own colour, then that part in each palette, then hues and shades of it. Left and Right walk the strip straight from the list. Exact color switches to Red, Green and Blue, and Back to swatches returns.
- A written convention for what a CSS outfit declares about its colours: a shared vocabulary of roles, which group each belongs to, and which keep their hue. `group`, `role` and `hue_locked` are optional on a colour control, because CSS infers all three from the control id for packages that predate this. `tools/css_colors.py` gains `lint_convention()` for package builders to fail their own builds on. See `docs/color-convention.md`.
- ANIMATION tab in the CSS page (between COLOR and TEMPLATES) with one option, Normal or Feminine: Walk animation. CSS drives it natively from the game's own `BS_CultistSpearLady`, so no extra pak is needed, and it plays at the pace that cycle was authored for so the feet stay planted. Standing borrows her idle too. Jogging, sprinting, combat, dodges and traversal are untouched, and templates save the setting.
- While Feminine is on, CSS re-asserts the animation whenever something else takes it, so a third-party walk mod cannot pull it back. On Normal, CSS does not touch locomotion at all and leaves such a mod free to drive it. The ANIMATION tab names any installed GenessaWalk or ProximaWalk by argisht and says which one is in charge.
- Outfit packages can correct where the game stows items such as seals (`attachments` on a variant). A stowed item is welded to its socket and never tested against the body, so on a wider shape it starts partly buried and the stride then swings the body through it. CSS now measures the item against the body's own physics asset every frame and holds it the declared clearance off, following the live pose; a fixed socket offset is the fallback for a mesh with no collision to measure. Nothing is touched while an animation borrows the item, such as a parry reaching for the seal. Seductress v2.0.2 and v1.0.3 ship the measurements.
- The tab strip now clips like the game's inventory tabs and slides to the selected tab.

### Changed

- Choosing a palette takes over the parts that palette sets and the tint of their groups, and leaves everything else alone, so a custom skin now survives changing the dress. Original still clears every override and tint, because it means no dye at all.

### Fixed

- Switching to a shell could revert its outfit to the first one installed, and did so again after restarting. Applying an appearance loads a mesh, which takes long enough for a shell switch to finish underneath it; the result was then saved against the wrong shell.
- Random dark, glossy body: a dye render target was bound to the skin before its layers were drawn, so a failed import or draw left a black texture on the body. The composite is now drawn first, checked, and only then bound; a black result keeps the authored texture.
- Dragging a Tint slider with the mouse failed, because the drag handler read the colour sliders' channel off an action that carries a field instead.
- Numbered Blueprint fields (for example SpringBone_1) resolve correctly in CSSX property requests.

### Removed

- The Jog animation and Sprint animation options from the 0.3.3 preview. Neither borrowed run matched the ground speed well enough to ship, so both gaits stay on the game's own animation. A state or template written by the preview still loads, and both settings come back as Normal.

### Upgrading

Close the game and extract `MSII-CSS-v0.4.0.zip` into `MortalShell2/Binaries/Win64/ue4ss/Mods/`, replacing the CSS runtime files. Keep `CustomShellSystem/state/`, CSSX, extensions and your installed outfit packages. Existing templates are migrated (they gain the Normal walk setting) and saved colours are preserved.

**Only CSS needs updating. Existing CSS outfit and port ZIPs keep working; socket corrections and the colour convention fields are both optional. CSSX, UI Kit and Cheat Menu remain at 0.3.0.**
