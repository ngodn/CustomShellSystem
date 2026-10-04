# New-game Harbinger appearance block

October 4, 2026. The author reported a stock Harbinger body and inability to
change anything on SHELL after installing the combined beta.9 candidate.

## Cause

Commit `337c192` broadened the traversal guard from `GA_Traversal_` actions to
`GA_ShellTraversal*` abilities. The latter are persistent event listeners.
Being able to switch the character mesh does not mean their entire active
lifetime is a mesh transition.

The live read-only capture shows `CharacterId.Player.Darkform.StrongOne`,
`GA_ShellTraversal_StrongOne_C` with `activeCount=1`, and no change across 32
samples. CSS logs `Recovery blocked by: traversal ability active`. The cooked
parent graph registers `WaitGameplayEvent` tasks for detach/recall. Together
these explain why the new guard blocks normal gameplay and manual SHELL choices.

The user also disabled Harbinger mirroring and restored the original appearance
while investigating. Preserve those choices. The fix must allow another manual
selection, not forcibly re-enable mirroring or invent a saved StrongOne outfit.

## Repair and verification

Removed the persistent listener family from the blocking predicate. The original
`GA_Traversal_` action guard, quest/teleport, controller input, transition widget
and montage checks remain. Equipped-sidearm recovery and the shared mirrored
animation selection resolver remain in place.

The regression first failed with `Persistent shell listener blocked appearance
changes`. The corrected predicate passes listener exclusion and real jump-action
checks. Five relevant host suites pass: recovery, animation data, animation
runtime, profiles and MISC attachments. The original test incorrectly asserted
that listeners must block; it tested the assumption instead of a gameplay case.

Evidence is under `work/harbinger-start-regression/`: original status/settings,
read-only capture, build logs and candidate installation record. The diagnostic
capture finished normally and did not issue gameplay input or alter abilities.

The corrected candidate `css_core-beta9-harbinger-e10ebb46170741fc.dll` was
installed after the game closed and its SHA-256 verified. The author then
confirmed "ok it works now" after being asked to select and switch outfits in
Harbinger form. This accepts the reported SHELL-switching regression repair.
Default restoration, saved recovery across another restart and the other pending
fixes are not independently confirmed by that reply. The public ZIP remains
unchanged.
