# Heart of Vatra visibility

October 4, 2026. Follow-up to the beta.8 report from StickyFingazu.

## Evidence

The reporter describes a persistent belt object after collecting Heart of Vatra
in NG+ with Gragu's quest already complete. Their `status.json` shows five MISC
rules active and a successfully applied Eve Black Pearl. The logs do not identify
the belt actor directly. Removing CSS did not remove the object, according to
the reporter.

Cooked game assets establish the relevant attachment:

- `ID_Attachable_Heart` is named **Heart of Vatra** and is a key item.
- Its `IF_AttachableItem` spawns `BP_Attachable_Item_Heart_C`.
- That actor has a `DefaultSceneRoot` with a nested `SM_Heart` mesh.
- Its socket is `Socket_Prop_Stowed_InfiniteSeal_Right`.
- Its mesh is `SM_Gragu_AlienHeart`. This does not make it Gragu's consumable
  `WP_AlienHeart_C` (Revered Heart).
- The parent actor's `HandleAttachment` uses `K2_AttachToComponent`.

Local extraction evidence is under `work/vatra-heart/`: `assets.json`,
`base.json`, package lists, extraction logs and the reporter's screenshot.
Reporter logs remain in the supplied beta.8 bug-report directory.

## Cause and change

MISC admitted a bare scene root only for Gragu's helmet. The quest heart's root
was skipped before its nested mesh could be hidden. Simply admitting it would
also misclassify it as a seal because of its socket name.

The exact heart actor is now an accessory with its own **Heart of Vatra** row.
Its category is **Accessories & Shell Tools**, regardless of the seal socket.
The individual rule uses `item:bp_attachable_item_heart`, independent of
`item:wp_alienheart`. Existing individual overrides, category fallback and
restoration remain in use.

No inventory, quest, save-game or attachment-transform mutation is introduced.
CSS uses its existing actor/mesh visibility path. Pawn-owned components are
still excluded. Other scene roots, audio and VFX remain excluded. There are no
new hooks, timers, actor scans or descendant walks.

## Verification

The host regression first failed with:
`Heart of Vatra was skipped before MISC could hide its child mesh`.
After the fix, `css_misc_attachment` and `css_data` pass. It covers the real cooked root and
socket, distinct heart types, the helmet, unknown scene roots, effects, the seal,
stowed guns/melee and excluded body sockets.

The C++23 production `css_core` build passes with inventory development actions
and transition tests disabled. These checks establish classification and build
correctness, not live NG+ acceptance.

Still to verify in-game with the quest heart attached: individual hide/show,
category hide with individual show override, menu/world consistency, saved-rule
restoration after restart, and continued normal quest/item behavior. The public
beta.9 ZIP has not been replaced. Candidate installation details are recorded
in `work/vatra-heart/install.json`. The combined candidate is installed as
`css_core-beta9-vatra-aad13a87abd209f5.dll`, verified against the build by SHA-256.
It also includes the pending traversal-aim and mirrored-animation repairs.
