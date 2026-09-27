# CCS UI reference

The user's latest reference is [screenshot-2026-09-27_08-27-59.png](/home/eins0fx/Pictures/screenshot-2026-09-27_08-27-59.png), reviewed during the takeover on 27 September 2026. This replaces the older 07:47 sketch as the layout reference.

The sketch specifies:

- A logo area above the left column, followed by Customize, Preset and Settings tabs.
- A left browser grouped into Player's Weapon, Tarstones, and Enemy's Weapon. Populate actual selectable records from the catalog. The labels and sample rows in the sketch express grouping, not a fixed list of equipment.
- Five light slots (L1, L2, L3, LF, LC) above five heavy slots (H1, H2, H3, HF, HC), near the bottom center. Highlight the active slot and display the referenced weapon or Tarstone icon.
- The game's inspection card on the right, with actual item name, level, icon, compatible weapons, description, upgrade effects and input prompts.
- Top and bottom bars spanning the available page area. Their contents are not specified by the sketch.

Use the game's existing UMG widgets, item display data and input glyphs. Preserve the spacing relationships within the Player Menu's painted local geometry. Do not assume a 1920x1080 canvas or create a new set of widgets on each navigation step. Pool widgets and update changed properties; bound creation work per tick as CSSX does.

An empty slot must show an empty inspection state. Do not leave Stillblade sample text, invented combat values or a fake 3D preview visible. Reopening the menu must reapply the active model after the game's Construct event. Controller and keyboard actions must come from the actual input mappings.

The shipped default build keeps the current prototype inactive. Its builders now use retained pools and bounded record windows. The transitional layout still needs final native widgets, icons, glyphs, live item inspection and rendered aspect-ratio checks. The user subsequently requested autonomous implementation while they rest. Apply the UI checks during implementation, using this screenshot and existing native widgets as the design constraints.

Player Menu tab order, as specified by the user: Inventory, Tarstones, Map, CSS (if present), CCS, CSSX (if present). CCS alone follows Map. Experimental hosting now orders paired buttons and pages and preserves the selected page. Offline permutation tests pass; activation still requires native runtime verification.
