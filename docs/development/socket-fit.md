# Socket fit: stowed gear follows the worn look

Reported on Nexus (bairdboy123, 2026-09-30): Sariel wearing Genessa's look carries
his gun off her back and his axe high above her shoulder. The game "compensates
for his size", and that compensation stays when the body changes.

## What the game does

All 82 weapon sockets live on the shared `SKEL_Human_Skeleton`. None of the shell
meshes (`SK_ThornBoi`, `SK_Sester_Genessa_V6`, `SK_Harros`, `SK_Shell_Gragu`)
carries a socket of its own, so socket positions are not where shells differ.

Each shell's character data (`CD_<Shell>`, class `SpartaCharacterData`) holds a
`SocketAdjustmentData` in its `TypeSpecificData`: a `TMap<FName, FTransform>`
from socket name to an adjustment. `WP_WeaponBase.GetCharacterSocketAdjustment`
reads it, and a stowed weapon's root component ends up with exactly that
adjustment as its relative transform at the socket. Live probe on the Genessa
shell: the Tarnished Seal sat at relative `(5,-5,-3)` plus CSS's own lift, pitch
-7 (quaternion Y 0.0610), and the lute at roll -4 (quaternion X 0.0349), both
matching `CD_Genessa` exactly.

The tables are body-specific (full dump in
`work/research/socket-adjustments.json`, exported by the GameDump tool):

| Socket | Genessa | Sariel | Gragu | Lazlo |
| --- | --- | --- | --- | --- |
| `Socket_CrossBow_Stowed` | Y +5 | none | Y -5 | Y -18 |
| `Socket_NailShotgun_Stowed` | Y +4 | none | Y -5 | Y -18 |
| `Socket_ParasiteGun_Stowed` | Y +5 | none | Y -5 | Y -18 |
| `Socket_Ballistazooka_Stowed` | Z +3 | none | Z -5 | Z -18 |

Positive Y pulls a back weapon in. Genessa pulls her guns 4 to 5 cm toward her
slim back; Sariel leaves them where the socket puts them. So Genessa's body on
Sariel shows the gap the player saw, and on Gragu or Lazlo the gap is larger.

## What CSS does now (beta.7)

`Variant::fit_shell` names the shell a look is sized like
(`fit_shell_for` in `data.hpp`: variant field, then outfit field, then the first
listed shell; Use Original Shell fills it from the shell's own character data;
replacement mods inherit it; NPC and enemy looks have none). Discovery also
records each official shell's `CharacterId -> character data` path.

`Appearance::refit_attachments` (attachment_follower.inl) reads both tables and,
for every named stowed socket where they differ, gives the attachment entry a `SocketRebase
{from, to}`. `AttachmentOffsets::apply` moves the game's stowed transform with
`base * inverse(from) * to` (`socket_fit.hpp`, tested in
`tests/socket_fit_tests.cpp` against the game's own quaternion readings) before
CSS's fixed offset and one-sided collision push. The push therefore still keeps
gear out of a larger body, and the rebase now brings it in to a smaller one,
which the push alone never could. The refit reruns at the 4 Hz attachment pass
when the shell list arrives or the worn character data changes. MISC "Gear
position: Default (game)" turns all of it off.

## Not covered: in-hand poses (cause still open)

This fix only rebases socket names containing `_Stowed`. The extracted tables
also include other sockets: Gragu gives `Socket_Prop_L_02` a Z +5 adjustment,
for example. Those entries are deliberately excluded from automatic fitting.
What makes a held
scythe or axe ride high on another shell is not settled. It is not the
`CharacterAnimSet`: `AMS_Gragu` and `AMS_Genessa` hold the same four shared
subsets (`AS_Player_Interactions`, `_Traversal`, `_Locomotion`, `_Abilities`).
Each shell class does carry its own anim blueprint (`ABP_Shell_Gragu`,
`ABP_Shell_Genessa`, `ABP_Shell_Thorn`) while the live pawn runs `ABP_Player`,
and neither shell blueprint is linked as a layer by class. Next step: compare
the same held weapon on the real Genessa shell against Genessa's look on Gragu.
If they match, the pose is the weapon's own carry pose.

## Validation status

Host tests pass (`css_socket_fit`, `css_data`). The Windows core builds clean.
Live check still needed on Sariel (or Gragu) wearing Genessa via Use Original
Shell, with a back gun and a seal: the gear should sit against the body, and
switching Gear position to Default should put it back where the game hangs it.
Probe scripts: `work/socket-fit/probe_attachments.py`,
`probe_weapon_detail.py` (read-only).

October 2 review corrections: fit settings are installed after a successful mesh
apply, because restoring the previous pawn clears its attachment state. Transform
ownership checks both location and rotation. A game re-stow drops the previous
collision push, and Default restores only transforms CSS still owns. Adjustment
tables are consulted before loading an asset; a cache miss retains the current
character data and checks that the pawn still matches after loading.

The host suite covers repeated stows, rotation-only updates, wrapped rotations,
borrowed items, and held-socket exclusion. These checks do not replace the live
switching, respawn, combat and frame-time comparison required before release.
