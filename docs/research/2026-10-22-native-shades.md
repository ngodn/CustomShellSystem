# Native shades after build 26073152

The October update adds eight Regal shades. CSS's old Alternative/Obsidian
allowlist rejected their gameplay tag. CSS 1.0.2 now accepts safe leaf names
under the native shared-shade namespace, retaining the existing Alternative
ID (`altskin`), localized item names and native `IsSkinUnlocked` check.

| Shell | New shade |
| --- | --- |
| Eredrim | Anointed Butcher |
| Genessa | Sapphire Herald |
| Gragu | Regal Druzhina |
| Proxima | Queen of Hearts |
| Lazlo | White Knight |
| Smert | Lunar Emissary |
| Sariel | Gilded Scarab |
| Tiel | Red Kingsnake |

The current character data supplies 24 shades across eight shells. Together
with the ten default shells, that gives 34 appearance definitions before
native unlock filtering. Harros and Solomon have no populated shade map;
an unused Harros Obsidian item definition alone does not enable a shade.

## Genessa companion

`GenessaHipSealFit_P`, included in Genessa Skin, overrides four character-data
packages. Its older `CD_Genessa` removed the new Regal material map. Import
validation alone could not detect this semantic regression.

The companion is rebuilt from the four updated game packages. Only the
`Socket_Prop_Stowed_InfiniteSeal_Right` transform is copied from the existing
mod. AssetReadback's Debug-only socket reader records parser-derived spans;
`tools/rebase_seal_socket.py` checks package hashes and socket layouts before
copying each complete 82-byte serialized transform. This preserves its exact
numeric values, including precision lost by a JSON round trip. All other
current-game bytes remain unchanged in each converted raw package.

Independent packed readback confirms all four packages match the current
game's decoded properties except that one preserved socket transform. The
updated Genessa shade map has all three entries. The normal Genessa Skin
1.0.16 ZIP includes the repaired companion; no extra patch download is needed.

## Verification

Evidence is retained under `work/shades-26073152/`:

- `items.json`, `shell-blueprints.json`, and `character-data-base.json` establish
  the names, meshes and native material maps directly from the updated game.
- `shade-slots.json`: all 24 shades cover active mesh sections, retaining native
  mesh materials wherever the shade map supplies a null slot.
- `shade-shaders.json` and `final-shaders.json`: all 112 unique shade materials
  have their required shaders in both the base game and full repaired mount.
- `final-dependencies.json`: 116 roots and 17,036 dependency packages pass
  effective-container ownership, requested-export and public-import checks.
- `rebase-spans.json`, `genessa-readback-check.json` and `rebase-verify.log`
  verify the companion repair and packed container integrity.
- `shades-before.log` reproduces rejection of Regal by the old helper;
  `shades-after.log` passes the production helper regression. All 31 host
  suites pass in `host-tests.log`.

Final in-game rendering and locked/unlocked behavior still need acceptance.
The game has been kept closed to preserve desktop focus. These checks do not
resolve the separate Commander White FPS or body-motion tuning reports.
