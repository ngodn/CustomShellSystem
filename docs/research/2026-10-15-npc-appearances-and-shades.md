# NPC appearances and native shell shades, October 15

These are required goal items. The user's original low priority described their order, not permission to leave them unfinished.

## Installed changes

Development core: `css_core-development-9ab14d4c095efe9f-shades.dll`.
The existing guarded hot-reload procedure passed and restored the selected Peony look, idle, visibility flags, camera, position and resources. No cooked assets were replaced in the running game.

### Merrick

- Catalog choice: `css.npc.people/sk_shopkeeper_mh`, displayed as Merrick.
- Mesh: `/Game/Sparta/Characters/NPCs/Shopkeeper/Art/Mesh/MH/SK_Shopkeeper_MH.SK_Shopkeeper_MH`.
- Exact native skeleton admitted for appearance replacement only. CSS-authored animation compatibility was not broadened.
- Live readback confirmed `ABP_Player` and the original `ABP_Shopkeeper_post`. His native mesh preserves its LODs, original materials, morph/DNA data and clothing assets.
- Screenshot: `work/npc-appearances/native-merrick/live/merrick.jpg`. This establishes standing appearance and active graphs, not full movement/combat or doubles acceptance.
- Peony and position restored. CSS added one default remembered-customization entry for Merrick during the temporary selection; existing choices and values were preserved.
- `css_skeleton_tests` passed, including rejection of arbitrary and lookalike skeleton paths.

The existing asset index has no other meshes referencing either Merrick's or Zhirelle's skeleton. The user subsequently authorized straightforward boss adaptations with independently checked rigs; this does not make unrelated NPC skeletons automatically compatible.

### Shell shades

The two-save regression in
`work/three-outfit-rig-physics/genessa-cross-save-20261015/restoration.json`
also restores Obsidian Eredrim and its effective materials after loading a
different Genessa save, exercising both forms there, and returning. Independent
Faithful Peony and Stray Eredrim selections remain intact, with exact CSS state
restoration. This checks the selected shade, not every shade or clone material.

`SHELL > Use Original Shell` keeps one list of shells and adds a separate **Shade** group for the selected shell. The catalog discovers `SkinMaterialsMap.Default` from each shell's character data. Native null slots retain the mesh default. Labels come from the corresponding native shade item definition.

The discovered alternates are Ophidian (Tiel), Second Son (Lazlo), Porcelain Prime (Genessa), Sanguine Prophet (Smert), King In Yellow (Eredrim), Apocryphal (Sariel), Warden (Gragu), and Bonegrafted (Proxima), plus each of those shells' Obsidian shade. Harros and Solomon have no shade entries in the inspected map. Runtime discovery reported no skipped entries.

CSS reads `BPFL_Player_Equipment.IsSkinUnlocked` for the current save. It does not call `UnlockSkin` or `ChangeShellSkin`. A locked explicit request is rejected. A saved shade that is unavailable in the active save falls back to that appearance's original variant. Profile selection continues to use the existing variant ID storage. Non-CSS replacement entries exclude native shade variants, preserving their own materials and avoiding duplicate appearances.

The native alternate assets use folders such as `Alt-Skin_01`; the asset-path validator now accepts hyphens while retaining its traversal and character restrictions.

Live evidence: `work/npc-appearances/native-shades2/live/`.

- Obsidian Eredrim used distinct native Obsidian material parents in the world.
- CSS menu preview used the same effective materials.
- Returning to Original restored the original material parents. Dynamic instances are recreated during changes, so comparing their object identities was an incorrect first assertion; the parent readback corrected that comparison.
- The Shade group visibly showed Original, Obsidian Eredrim and disabled King In Yellow. Direct selection of the locked shade was rejected.
- Gameplay form remained `CharacterId.Player.Darkform.CorruptedGenessa` throughout.
- Peony, original customization and camera/position restored. One new empty remembered-customization entry for the tested shade remained; no existing settings changed.
- Host `css_data`, `css_profiles`, and `css_replacements` tests passed. The replacement test includes a native shade and verifies it does not become a duplicate replacement look.

`work/npc-appearances/shade-profile1/result.json` now verifies the native shade
after a normal game restart and through profile save/change/load/repeated load.
Switching to Original changes the effective material parents; loading the saved
profile restores the Obsidian parents, selection and customization. The preview
after profile loading matches the world, and closing the menu preserves them.
The temporary profile is deleted, with exact CSS state, appearance, position and
camera restoration. Gameplay remains Genessa in Stray throughout.

The same fresh-launch check finds the native revival blocker still active.
An ordinary Faithful request shows the Alteration explanation and does not
change form. The request is reset to Default afterward. This checks normal
restriction handling after restart, not a second-save override.

Remaining acceptance: cross-save/form transitions and native effects/doubles as
applicable. Do not claim every lifecycle or every shade has been tested from
this one live shade. The earlier material and unlock checks remain applicable.

## Zhirelle deferred to CSS v1.0.1

The user explicitly deferred Zhirelle on October 15. Authoring trials are stopped. Keep the evidence below for v1.0.1; these rejected trials are not installed and do not block v1.0.0.

Source: `/Game/Sparta/Characters/NPCs/ShellKeeper/Art/Mesh/SK_ShellKeeper.SK_ShellKeeper`.

The original mesh is cross-legged and seated. Its left leg uses `unrealHip1_L`, `unrealKnee1_L`, `unrealAnkle1_L` and `unrealToes1_L`. Their axes differ from the native player chain: the calf/foot translations point along positive X where the human left chain uses negative X. The seated pelvis also has a much lower reference height.

The isolated import preserves the source geometry, UVs and original weights and adds leg-name aliases. It is not installed or listed in the catalog. Native animation previews show that admitting its skeleton or copying reference rotations is insufficient. Correcting translation directions and reference height alone also leaves severe dress distortion. Unreal's optional mesh reference supplies the actual translation basis, so changing only the Skeleton reference does not correct that mesh bind.

Source geometry and skin data: `work/npc-appearances/zhirelle/`. The original seated preview is `zhirelle-seated-reference-renders/pose-00000.png`. Rejected previews include `zhirelle-renders`, `zhirelle-leg-axis-renders`, `zhirelle-rebound-renders`, and the two `zhirelle-reframe-*-renders` directories. Do not promote any of them.

The component/weight audit identifies skirt sections with substantial left-knee weighting across both sides of the seated garment. Reframing the left leg improves its orientation but does not repair those weights. When v1.0.1 work resumes, the next work is the standing garment and bind correction, preserving the recognizable original dress, followed by import/cook and targeted game checks. The current helper and import scripts are preparatory authoring code, not a successful Zhirelle release recipe.

All work uses the relocated `work` symlink under `/run/media/eins0fx/theark/CSS/CustomShellSystem-work`. Latest `df -h`: root 95 GB available, theark 48 GB available. No full project copies were made for these attempts.
