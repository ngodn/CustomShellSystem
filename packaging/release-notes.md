### CSS 1.0.2

#### Added

- Adjust body-motion Travel on supported outfits.
- Support 24 body-shape controls in the updated Eve, Unholy Genessa and Commander White packages, with an optional UUEPBS profile.

#### Fixed

- Apply the Travel value shown by each motion preset, including older saved looks.
- Reduce redundant facial-animation updates.
- Recognize Merrick replacement mods and refresh non-CSS character detection for the October 20 game update.
- Assign native ghost materials to William and Ball Boy's second look in Use NPC / Enemy.

#### Upgrading

Install CSS Shared Assets 1.0.2 and the updated outfit packages. Texture repairs
are included in each outfit. Remove superseded standalone shader overlays and
keep your settings and profiles. Motion-feel and CW frame-rate follow-up remain
open.

### CSS 1.0.0

#### Added

- Select facial expressions in Customize and save them in profiles.
- Play synchronized facial animations on supported Eve, Unholy Genessa and Commander White outfits.
- Choose from 52 additional idle animations, with native-human versions for compatible original shells.
- Wear Merrick, Tarsus, Droeg, the Nameless Captive and Sir Isaac through Use NPC / Enemy.
- Choose sixteen native shades across eight original shells, respecting the game's unlocks.
- Confirm a one-time Faithful override when Alteration blocks revival. Upgrades remain intact; form and health save normally.
- Choose Default (game) or Reduced noise under MISC > Sunlight shadows. Reduced noise makes sunlight shadows sharper.

#### Fixed

- Hide attached Tarstone effects with their sidearm while keeping infusions active and fired projectiles visible.
- Keep the Trebuchaxe's unused preview axe hidden while aiming.
- Restore Default visibility and mirrored outfit choices when loading profiles.
- Check missing outfit dependencies before applying profiles.
- Restore customization after Genessa form changes.
- Remove the 128-attachment snapshot limit.

#### Upgrading

Close the game before updating. Keep CustomShellSystem/state and profiles.
Update CSS, the combined CSS Shared Assets package and the matching outfit
packages together. Remove superseded CSS_AstralSharedAssets_P and
CSS_LocomotionSharedAssets_P folders. Shared Assets is a separate download
in the CSS release post.

### CSS 1.0.0-beta.12 (withdrawn rehearsal)

> This local rehearsal was never a public release. It is superseded by the
> [coordinated 1.0.0 candidate](../docs/releases/stable-changelog-drafts.md).
> The previous public release remains beta.11.

#### Added

- Support Peony's expanded customization, texture palettes and flowing trim.
- Support shared animations and facial playback for compatible outfits.

#### Fixed

- Preserve current body-motion settings when a performance restores normal animation.
- Support Peony's outfit-local face mesh and customized double materials.

#### Upgrading

Install the matching unified CSS Shared Assets package. Replace the older
CSS_SharedAssets_P folder and remove superseded CSS_AstralSharedAssets_P and
CSS_LocomotionSharedAssets_P folders if present. Keep settings and profiles.
Install Eve v1.5.0 separately for Peony.

### CSS 1.0.0-beta.11

### Added

- Let Genessa's Faithful and Stray doubles copy supported current appearances and customization, keeping their ghost effects.
- Choose Default or Use CSS independently for both doubles in MISC, with profile persistence.
- Switch Genessa Form once between Faithful and Stray; later transitions follow the game.

### Changed

- Reuse cloth visuals on cached Faithful doubles. Fresh cloth-heavy doubles can still briefly hitch; further optimization is planned for beta.12.

### Upgrading

Install the separate Astral Shared Assets download for customized doubles.
Keep the older CSS_SharedAssets_P Eve package and your CSS state folder.
Unholy Genessa, Eve and Commander White are tested. Other appearances have
partial support; unsupported looks keep the original double.

### CSS 1.0.0-beta.10

### Added

- Hide the Heart of Vatra belt accessory through MISC.

### Fixed

- Restore saved outfits after delayed mesh resets from gates and launchpads.
- Restore sidearm aiming after traversal and other game-driven mesh changes.
- Keep selected idle, walk, jog and sprint animations when carrying an outfit into Harbinger form.
- Allow appearance changes while persistent shell traversal listeners are active.
- Save a new profile with Enter while typing its name. Explain invalid names and full profile storage.
- Clear stale status labels when reopening CSS so they no longer overlap SHELL rows.

### CSS 1.0.0-beta.9

### Added

- Other mods can read and change the current look through CSS's `css.customize` service: palettes, toggles, colours and sliders, either saved or just for the moment. CINE uses it to animate your outfit during a take and put your own look back afterwards. CSS works the same with or without CSSX.

### CSS 1.0.0-beta.8

### Added

- Material visibility toggles, allowing supported outfits to hide sheer fabric independently of solid details.
- Support for up to 64 customization controls per variant, increased from 32.

Existing outfits retain their section visibility controls. Fabric visibility and
opacity require support from the outfit's materials and customization recipe.

### CSS 1.0.0-beta.7

### Added

- Separate controls for per-slot fabric overlays and their base materials.

### Fixed

- Restore customized fabric overlays after temporary native game effects.
- Light translucent materials in the CSS preview.
- Fit stowed gear using the selected appearance's socket adjustments.
- Handle Gragu's helmet when switching appearances.

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
