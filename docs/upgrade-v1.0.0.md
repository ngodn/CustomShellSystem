# Installing the coordinated CSS v1.0.0 update

**Draft for the coordinated release.** The current packages are development
trials, not the final downloads. Final version stamps, ZIPs and release checks
are still pending. Use the release's matching files when they are published.

## Downloads and locations

| Download | Target version | Install under `MortalShell2/` |
| --- | --- | --- |
| CSS runtime | 1.0.0 | `Binaries/Win64/ue4ss/Mods/CustomShellSystem/` |
| CSS Shared Assets | To be confirmed | `Content/Paks/~mods/CSS_SharedAssets_P/` |
| Eve Stellar Blade | 1.5.0 | `Content/Paks/~mods/CSS_EveStellarBlade_eins0fx_P/` |
| Unholy Genessa | 0.3.0 | `Content/Paks/~mods/CSS_UnholyGenessa_eins0fx_P/` |
| Commander White | 0.1.0 | `Content/Paks/~mods/CSS_CommanderWhite_eins0fx_P/` |

The runtime and shared assets are separate downloads. Install the outfits you
want, with the matching shared pack. You do not need all three outfits.
The shared pack contains the common animation and Astral resources; it does
not contain the complete outfits. Keep the supported UE4SS installation and
any separately installed CSSX/CINE extensions.

Each asset package has three files with the same base name: `.pak`, `.utoc`
and `.ucas`. Install all three together. Do not mix files from different
versions or rename them. The runtime folder belongs under UE4SS, while asset
packages belong under `Content/Paks/~mods/`.

## Updating an existing installation

1. Close the game completely. Core DLL and mounted asset updates require a
   normal restart. The current core hot-reload path is not verified safe.
2. Back up `Binaries/Win64/ue4ss/Mods/CustomShellSystem/` and the asset packages
   you are replacing. Store backups outside `Content/Paks/`, so the game cannot
   mount old and new copies together.
3. Update the CSS runtime in its existing folder. Keep `state/`, including
   your saved settings and profiles. Do not delete your game saves or reinstall
   UE4SS as part of this update.
4. Remove the old installed trios named `CSS_LocomotionSharedAssets_P` and
   `CSS_AstralSharedAssets_P`. Replace the old `CSS_SharedAssets_P` trio with
   the consolidated pack. Check for duplicate copies in other mod folders.
5. Replace each installed Eve, Unholy Genessa and Commander White trio with
   its matching version from the table. Keep unrelated mods and extensions.
6. Launch the game. Check your saved appearance and profile, customization,
   selected idle and movement animations. Movement, aiming and attacking
   should interrupt an idle normally. With gameplay shell Genessa, check
   Faithful and Stray doubles if you use their **Use CSS** options.

For a fresh installation, install the supported UE4SS build required by the
runtime release, then put the runtime, consolidated shared pack and chosen
outfits in the locations above. Do not add the older separate locomotion or
Astral packs.

## Version mismatches

Keep the upgraded outfits and consolidated shared pack together. An old
outfit may still appear in the menu with the new library, but that does not
mean its facial or animation adapters match. Update that outfit before
expecting the new facial and playback support.

If CSS asks you to **Install** or **Update CSS Shared Assets**, check that the
consolidated pack is present and all three files came from the same download.
A **Remove duplicate copies** message means more than one library is installed.
Remove the older copy with the game closed. Do not keep it in a backup folder
inside `Content/Paks/`.

Saved selection IDs are retained by this migration. Do not edit Unreal asset
paths in your profiles to match the new author folders. If an appearance or
animation is missing, check package versions first. For a report, include
`Binaries/Win64/ue4ss/UE4SS.log`, `Mods/CustomShellSystem/CSS.log` and
`Mods/CustomShellSystem/runtime/status.json` (the latter two are relative to
the `ue4ss/` folder).

To roll back, close the game and restore the complete matching runtime and
asset backup. Keep a copy of any profiles changed since that backup before
restoring its older `state/` directory.

See [asset organization](asset-organization.md) for author namespaces and
dependency contracts, and [implementation status](development/three-outfit-rig-physics-status.md)
for trial evidence and remaining release work. This installation guide does
not establish that every outfit, animation or gameplay case has passed.
