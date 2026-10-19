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

The Detailed Panel > Morphs tab lists every morph target the worn mesh carries. Eve, Unholy Genessa and Commander White are gaining a 24-morph body set (the six originals plus a chest set: cleavage, diameter, perk side, upward slope, natural, implants, nipple size and depth, areola size, chest width, underbust depth; and glute width, glutes up/down, hip bone size, thigh size, calf size, shoulder size, waist depth). To get them as Simplified Panel sliders, one per morph, use the ready profile `docs/research/uuepbs/MortalShell2.profile.json` from the CSS repo as your `GameProfiles/MortalShell2.json`; it carries the Breasts bone group above and a `MorphGroups` block like:

```json
"MorphGroups": {
  "Breast size": { "Morphs": ["PBMBreastsSize"],     "Section": "CSS chest" },
  "Cleavage":    { "Morphs": ["PBMBreastsCleavage"], "Section": "CSS chest" },
  "Glute size":  { "Morphs": ["PBMGlutesSize"],      "Section": "CSS body" },
  "Body tone":   { "Morphs": ["FBMBodyTone"],        "Section": "CSS body" }
}
```

A wildcard group such as `["PBMBreasts*"]` drives every matching morph at once, which is not what you want on the chest set; name one morph per slider. A slider whose morph the worn outfit lacks is not shown (UUEPBS drops groups with no match on the current mesh), so the one profile serves every outfit. Stock game shells carry no morph targets, so the Morphs tab is empty on them; only bone sliders apply there.

## Rules of the road

- Press **F7** (Refresh) after changing outfit or variant in CSS. CSS swaps the mesh on the same component; UUEPBS needs a rescan to list the new mesh's morphs.
- A morph you move in UUEPBS replaces CSS's value for that morph, even at 0, until you Reset it in UUEPBS. CSS's own CUSTOMIZE shape sliders write the same morphs, so use one tool per morph at a time.
- Scaling the chest or glute helpers also scales their motion radius. CSS motion limits were measured on the unscaled body, so very large scales can let the helper pass through the chest; cosmetic, reduce the scale or the motion amount.
- UUEPBS presets are per game; a Mortal Shell II preset works on every CSS outfit on `SKEL_Base`, not on stock shells (different bone names).

## Pose hook pin

After the live test the recommended `"Hook"` line for `MortalShell2.json` goes here, so the search is skipped on start.
