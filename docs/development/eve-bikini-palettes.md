# Vacation Bikini color pass

Five original coordinated looks accompany CSS's Original option: Oasis, Sunset, Deep Sea, Orchid and Pearl Shore. Controls are Top color, Shorts color, Ties and lace, Heels color, and Buckles and rings. Each has Default plus eleven swatches.

The builder uses the saved Bikini material mapping, not the Prototype or old Black Pearl slot order:

| Control | Slots | Source base-color atlas |
| --- | --- | --- |
| Top | 18 | BK_Top |
| Shorts | 20 | BK_Shorts |
| Ties/lace | 16,17,19,21 | BK_Trim |
| Heels | 23,24,25,26,27 | CS_Heels |
| Metal | 22,28 | BK_Trim |

Body slots0–15 and hair/accessory slots29–31 are excluded. Geometry, weights, morphs and the visibility-aware heel graph stay in byte-identical asset containers from `b1motion`.

The authored BK_Shorts texture has pure-black cloth with a light decorative pattern. Multiplying black by a color cannot change its color. For non-default color layers only, the builder maps grayscale detail into the 0.55–1 linear range so the fabric can take the requested shade while retaining the pattern. Original is the unmodified original texture, not this layer tinted white. Inspect the result on the model and in-game before acceptance.

Build: `python3 tools/eve-fit/prepare_prototype_palettes.py --outfit bikini`.
Render: Blender with `tools/eve-fit/render_prototype_palettes.py -- --outfit bikini`.
Outputs: `work/eve26/b1colors`.

This is garment-color work. Body/anatomical/hair color controls, game palette restoration and persistence still require separate verification. A verified fitting package or these palette definitions alone are not a completed outfit.


Package/resource verification and asset byte-identity checks pass. All twelve model previews were inspected in `b1colors/model/review.jpg`. Installed with backup `backups/packages-0015`; hashes match `b1colors/installed.json`. Game was not launched. Live material behavior remains unverified.
