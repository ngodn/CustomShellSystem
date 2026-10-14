# Repaired Eve variants in the running game

After the user opened the game and reported ready, Black Pearl and Planet
Diving were temporarily selected through the existing CSS customization service.
Both resolve their repaired facial mesh and the shared `ABP_Eve` adapter. Each
capture found exactly one body rig, matching public regional settings and the
accepted tighter angular and travel limits. The animation owner remained stable.

| Variant | Facial mesh suffix | CINE performance input | Saved state restored |
| --- | --- | --- | --- |
| Black Pearl | `SK_Face_Jali3V1_BlackPearl2` | Enabled, weight 1 | Exact JSON equality |
| Planet Diving (`prototype`) | `SK_Face_Jali3V1_PFit14` | Enabled, weight 1 | Exact JSON equality |

Evidence directory:
`work/three-outfit-rig-physics/eve-surface-live-20261014`.
The successful clothed captures are `black-pearl-clothed1` and `planet1`.
Each contains the recipe, its recorded hash, owner and rig reads, screenshot,
before/after saved state and checksums. PID 1944353 remained unchanged.

Black Pearl's remembered Bodysuit and Stockings controls were both off in the
first `black-pearl1` capture. This explains the missing clothing in that image.
The second capture temporarily enabled those controls, restored Black Pearl's
original customization first, and then restored Peony. Planet's remembered
clothing was already visible and needed no override. One preliminary Planet
attempt (`planet-clothed1`) stopped because its visibility controls use the
`piece` role, rather than `garment`. Its restoration and whole-state equality
checks passed before retrying without the unnecessary override.

Peony was restored with the user's `black_white_floral` palette, customization
and idle override unchanged. The entire saved CSS state compares equal before
and after each run. No movement, camera, performance or idle commands were sent.
CINE remained active in the captured scene. No assets were installed or rebuilt.

## What these captures establish

Both outfits and their patterned clothing render in the existing shaded,
overhead CINE view. There is no obvious whole-mesh explosion in these stills.
The reads establish the active animation adapter and body inputs, not correct
facial motion over time or uninterrupted CINE playback. The character occupies
a small part of each image, with the face and lower clothing partly obscured.

These are not matched before/after sunlight captures. They do not establish
that the tangent repair fixes Vistani's moving sunlight artifact, nor prove
collision, fine clipping, frame-time cost or the remaining lifecycle gates.
Direct sunlight and continuous movement checks remain open.

## Repeat

Use a new output directory and a settled Eve variant other than the target:

```sh
python3 tools/diagnostics/capture_eve_surface_variant.py \
  --variant black_pearl --show-clothing --output /path/to/new-capture
python3 tools/diagnostics/capture_eve_surface_variant.py \
  --variant prototype --output /path/to/another-new-capture
```

The diagnostic uses the current Python 3.14.7 project tooling. It aborts on a
concurrent appearance change rather than overwrite a newly chosen look. Saved
state inequality after restoration is an error, with both snapshots retained.
