# Using UUEPBS with CSS outfits

[UUEPBS](https://github.com/XTGMods/UUEPBS) (Universal Unreal Engine Player Body Sliders, by XTGMods) adds bone and morph sliders to the player character. It works beside CSS: it edits the final pose after the animation and CSS physics have run, so CSS springs, body motion controls and customization keep working, and its edits sit on top of them.

Status: this page is a draft from reading the UUEPBS source (v2.6.1). The live test on Eve, Unholy Genessa, Commander White and tHorny Proxima is pending; the hook pin below is filled in after it.

## What binds on a CSS body

CSS outfits use the shared skeleton `SKEL_Base`. UUEPBS finds its Simplified Panel sliders by bone name:

| UUEPBS slider | SKEL_Base bones | Binds by default |
| --- | --- | --- |
| Butt | `butt001`, `butt002` | yes |
| Belly | `belly` | yes |
| Thighs | `thigh_l`, `thigh_r` plus twist helpers | yes |
| Calves | `calf_l`, `calf_r` | yes |
| Rib cage | `spine_02` | yes |
| Waist | `spine_01` | yes |
| Hips | `pelvis` | yes |
| Shoulders | `clavicle_l`, `clavicle_r` | yes |
| Breasts | `brust001`, `brust002` | no, add the profile line below |

The chest helpers are named `brust` (German), which UUEPBS's dictionary does not know yet. Until that lands upstream, add this to `Binaries/Win64/ue4ss/Mods/UUEPBS/Scripts/GameProfiles/MortalShell2.json` (the file is created on the first run):

```json
"BodyGroups": { "Breasts": ["brust001", "brust002"] }
```

## Morph sliders

The Detailed Panel > Morphs tab lists every morph target the worn mesh carries. Current eins0fx bodies carry six (`PBMBreastsSize`, `PBMGlutesSize`, `PBMHipSize`, `PBMWaistWidth`, `PBMThighsTone`, `FBMBodyTone`); the expanded chest and body sets are in progress per package. To get them as Simplified Panel sliders, add to the same profile:

```json
"MorphGroups": {
  "Breasts (CSS)": ["PBMBreasts*"],
  "Glutes (CSS)":  ["PBMGlutes*"],
  "Hips (CSS)":    ["PBMHip*"],
  "Waist (CSS)":   ["PBMWaist*"],
  "Thighs (CSS)":  ["PBMThighs*"],
  "Body tone":     ["FBMBodyTone"]
}
```

Stock game shells carry no morph targets, so the Morphs tab is empty on them; only bone sliders apply there.

## Rules of the road

- Press **F7** (Refresh) after changing outfit or variant in CSS. CSS swaps the mesh on the same component; UUEPBS needs a rescan to list the new mesh's morphs.
- A morph you move in UUEPBS replaces CSS's value for that morph, even at 0, until you Reset it in UUEPBS. CSS's own CUSTOMIZE shape sliders write the same morphs, so use one tool per morph at a time.
- Scaling the chest or glute helpers also scales their motion radius. CSS motion limits were measured on the unscaled body, so very large scales can let the helper pass through the chest; cosmetic, reduce the scale or the motion amount.
- UUEPBS presets are per game; a Mortal Shell II preset works on every CSS outfit on `SKEL_Base`, not on stock shells (different bone names).

## Pose hook pin

After the live test the recommended `"Hook"` line for `MortalShell2.json` goes here, so the search is skipped on start.
