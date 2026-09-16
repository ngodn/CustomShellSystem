# Support fixes, 16 September 2026

Work order agreed with _eins0fx: resolve cross-shell accessory attachments first,
then investigate appearance recovery and shell-switch safety. The non-CSS
package browser remains stopped and stashed. No release bump is requested yet.

| Report | Action and evidence needed |
| --- | --- |
| JayLuk3: Long Hair Proxima leaves shell accessories at the feet | CSS-wide fix is live locally. Actual Tiel/Eredrim attachments, draw/stow, six swaps and core-reload cleanup pass. Published packages are unchanged. See [recording review](jayluk3-longhair-attachments.md). |
| Irmassidarkstar: portal leaves a permanent material-override warning | Reproduce nonempty game-created MIDs blocking recovery. Preserve real gameplay effects; do not clear every MID or force expiry on a timer. |
| SamFisher91: crash after changing shell and walking/sprinting | Investigate transition object lifetime and animation state. Without a matching crash trace or reproduction, do not label an unrelated safety change as this crash's fix. |
| questman: retain selected appearance in Harbinger state | Inspect darkform identity and current selection policy. Preserve transformation gameplay and native effects. |
| Milbox: ordinary Kratos replacement compatibility | Ordinary replacements do not become CSS packages automatically. Keep the non-CSS browser deferred; document verified coexistence limits only. |
| joeyssteven1130: “mipmap requires a verified game build” | Confirmed CSS texture-dye adapter guard (`update_dye_mips`), not the minimap. Existing adapters cover two verified executables. Preserve those checks; the report alone cannot identify which executable or CSS version was installed. |
| QingYaMiaoMiao: latest CSS now works | Positive user confirmation, no new bug. |

The older portal/launch-pad comments remain covered by the transition recovery
investigation. Comment dates and unspecified “latest” versions are not enough
to establish the installed build.

## Current work

- Long Hair's cooked mesh confirms both missing dedicated bones. Hidden runtime
  fixtures reproduce the origin fallback and pass with a stock pose follower.
- The integrated C++ fix passes actual accessory, draw/stow, six shell swaps,
  Original restoration and core-reload cleanup checks. Three material-effect
  recovery cycles and native Inventory cleanup also pass. Original gameplay
  state and CSS preferences were restored. See the attachment report for limits.
- Developer and distribution cores build, with all nine portable test groups
  passing. No release ZIP or version was changed.
- Harbinger/darkform tags already pass CSS's same-skeleton compatibility filter.
  Selections are saved per exact character tag, so a darkform can have its own
  selection. Automatically inheriting the previous shell's selection would be a
  separate behavior change, not a gameplay transformation override.
- Do not claim the shell-switch crash fixed without a reproduction or matching
  crash trace. Keep active game-material effects protected during the pending
  portal investigation.
