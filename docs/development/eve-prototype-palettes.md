# Prototype Planet Diving Suit palettes

The user accepted Prototype F14 fitting, tail and body motion on September 27. Preserve those assets. This pass adds garment colors to that accepted candidate.

Original remains CSS's built-in restoration option. Five authored looks accompany it:

| Palette | Suit panels | Technical accents | Trim and markings |
| --- | --- | --- | --- |
| Xion Ember | Burgundy `#542D3A` | Patinated teal `#55B5AA` | Copper `#C58E64` |
| Wasteland Recon | Field green `#3E5147` | Amber `#D7AF66` | Weathered silver `#88918B` |
| Great Desert | Sand `#B5A18A` | Oasis blue `#458EA2` | Bronze `#82583B` |
| Stargazer | Midnight indigo `#303A65` | Lavender `#AC91DE` | Pale silver `#CCD6E7` |
| Angel's Descent | Pearl white `#DEE5E5` | Ice blue `#74C4DC` | Champagne `#C6AF7B` |

These are original interpretations, not claims of official alternate colorways. The official [launch article](https://blog.playstation.com/?p=388665) describes Xion, the Wasteland, Great Desert and Stargazer outfit. The [developer interview](https://blog.playstation.com/2024/04/08/stellar-blade-interview-creating-stylish-sci-fi-action-in-a-post-apocalyptic-world/) explains that residents call Eve Angel.

## Implementation and verification

`tools/eve-fit/prepare_prototype_palettes.py` builds `work/eve26/p14colors`. It starts from the installed motion-repaired F14 manifest and copies its cooked asset containers byte-for-byte. It adds three garment color controls and five palette definitions. Each color control has Default plus eleven swatches.

Masks are specific to the inspected PD_Suit and PD_Acc base-color atlases from Gemini's staged textures. They distinguish dark panels, cyan/teal technical accents and saturated gold trim. Warm mesh panels and neutral hardware retain their original colors. This is not a general-purpose outfit classifier.

Surface bindings are suit slot16 and accessory slots17/26/27/28. They exclude body, hair and blade slots. Body morphs, motion controls and separate garment visibility remain intact. Do not reuse older Planet manifest slots: their hair bindings do not match the current fitted mesh.

Generated atlas previews are offline color-composite approximations. They do not demonstrate the live Unreal material result, lighting, Original restoration or profile persistence. Before acceptance, inspect all five palettes on the character, restore Original, and confirm saved selections survive menu re-entry. No final release archive is replaced by this script.


## Model review

`render_prototype_palettes.py` completed in Blender 5.2.2 with exit0. It reconstructs the F14 import mesh and its first UV channel, hides the same covered body sections, and renders Original plus five palettes from front and back. All twelve images were reviewed in `work/eve26/p14colors/model/review.jpg`. Panel, trim, collar, tail and ribbon color placements are consistent across the views. This uses Workbench texture display with a neutral body and no hair; it checks UV placement, not the Unreal shader or runtime restoration. No blend or accepted mesh asset is modified.
