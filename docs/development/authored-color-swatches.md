# Authored swatches and Eve palette checkpoint

2026-09-21. Candidate installed after a normal restart. Jade material binding and
the palette list pass an initial live check; full swatch and appearance checks remain.

## User-reported rendering defects

The first live swatch review rejected skin and nipple/areola rendering. The
selected Ivory swatch looked gray, areola boundaries were hard and dark, and a
pale groin patch remained against tinted skin. Hair was reported working. The
user reset the appearance to Original afterward; the failing selection was not
retained, so exact anatomical swatch values are unknown.

Package, schema and Jade binding checks below still pass, but they do not prove
correct color rendering. Release acceptance is withheld while runtime pixel
readback and mask-boundary checks reproduce and isolate these failures. The
central material-5 island includes surrounding skin; excluding the entire
island from the skin control is an authoring defect. Preserve the accepted rig,
proportions, hair settings and ground offset while repairing color behavior.

## Ownership

Mod authors define their model's dye masks, material slots, palette values and
swatches. CSS reads those declarations, validates supported data, applies color
to the declared layers, and owns the menu and saved-state behavior. It cannot
recognize anatomy in an arbitrary texture or repair a misplaced mask. This is
also the division to teach in the planned `CSS-Modding` kit.

Eve's masks are asset authoring work. The native swatch list is a reusable CSS
feature. See [the schema](../control-convention.md#authored-color-swatches).

## Runtime change

Color controls may carry an ordered list of 2 to 24 named swatches. The first
entry resets the override rather than applying its preview RGB. Existing
packages retain their generated strip. Both mouse selection and left/right
cycling include Default. Cycling uses the stored RGB before group tint, keeping
the selected index stable under tinting. Exact color editing remains available.

Default restores the part's palette value if that palette owns it, otherwise
the original texture. This does not change the existing global Original/Reset
all behavior, which clears all overrides and tints.

## Eve candidate

Prepared with `tools/authoring-probes/colors/extract_eve_regions.py` and
`prepare_eve_colors.py`, from the accepted heel-support Blender file and
`CSS_SeduXtress_eins0fx/work/ground1`. Candidate:
`CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work/colors3/trio`.
These are Eve-specific source rules, not an automatic anatomical classifier.

- Five palettes besides Original: Moonstone, Garnet, Abyss, Jade and Amethyst.
  They set only suit, stockings, shoes and hair accessory colors. Existing
  `ivory` and `crimson` IDs remain valid as Moonstone and Garnet.
- Ten color controls, each with exactly 12 choices including Default.
- Separate nipple and areola masks use the source's named depth-shape support,
  actual body UVs, and one texel of raster-boundary padding. Body skin excludes
  their union. Their alpha covers 451 and 2,154 texels respectively.
- Material 5's central external UV island and two lateral lining islands form
  separate Labia and Genital lining masks. Coverage is 12,535 and 8,663 texels.
  These labels and visible boundaries still need in-game inspection.
- Partitions are disjoint, retain grayscale detail exactly, and reconstruct
  the original alpha byte-for-byte. Raw RGB previews can show the whole atlas
  despite alpha zero; inspect alpha coverage as well.
- The package retains all 15 non-color controls, hair 200/24/0 and the accepted
  -3 cm grounding. Both cooked containers are copied byte-for-byte. Mesh,
  rig, physics, morphs, animation and gameplay assets are not recooked.

The source extractor records the Blender hash and checks it is unchanged after
reading. The builder verifies the candidate package, every resource checksum,
and exact reversal of the declared metadata edits. Each variant's controls are
updated directly; an outfit-level recipe does not override variant metadata.

## Evidence and remaining checks

Local evidence is under `CustomShellSystem/work/colors1`. Native package loading,
thumbnail/cache repair and corrupt-package rejection pass against `colors3`.
Host control tests and all 17 Python authoring tests pass. Development
(`build/windows`) and shipping (`build/inventory-shipping`) builds both finish
with exit zero. Exit receipts and logs are saved alongside the package evidence.
Reusable swatch implementation commit: `eefd6bd`.

Rejected drafts remain under authoring `work/colors1` and `work/colors2`:
the first retained the old variant recipe and failed resource-set validation;
the second verified its package but hit a mistaken assertion that the original
also had outfit-level controls. Neither was installed. `colors3` edits only
the actual variant and passes both verification paths.

## Installed color checkpoint

`deploy_eve_colors.py` installed `css_core-colors1.dll` and the `colors3` trio
after backing up the grounding core, outfit and actual saved state. The normal
exit and Steam launch completed; Linux game PID was 3025501 at verification.
Deployment preserved the complete saved-state directory. CSSX remains disabled.

Core SHA-256:
`5e37a895af587df8039a5f265388c24d4f7edd0379bc5450b8119c5501561aae`.
Pak SHA-256:
`9c5c3ad5a00bc2c4927e3db7514b48428b8c9db0f82f1821447ee404c65c02ca`.
UCAS/UTOC hashes remain the accepted grounding package's hashes.

Evidence in `work/colors1/live`:

- `deployment.json`, `launch-exit.json`: installed hashes and preserved state.
- `open.jsonl`: world uses `SK_BlackPearl2` at relative Z -99 cm.
- `review.jpg` with equal `review-before.json` and `review-after.json`: Jade
  selected; the menu lists all five palettes plus Original. Dark lace and green
  accents are visible. This is not acceptance of all five rendered looks.
- `materials.json` and its request log: with Jade selected, both inspected
  protected material slots (2 and 5) retain their exact original Texture2D
  bindings. Outfit slots 16, 17, 18, 19, 20 and 29 use color render targets.
  Player identity and saved state remain unchanged throughout the read.
- `menu-review.mp4`: 15 seconds of the actual game window, no injected input,
  no camera orbit. Reviewed frames at 1 and 13 seconds show the Jade palette
  page with ordinary idle motion. No swatch selection occurs in this clip.

The palette and preview changed during menu preparation. The user confirms they
are testing the colors, so continue passive observation without changing their
controls. Controlled input checks must be coordinated afterward. The saved selection at the first read is
Jade with no explicit overrides, whereas the pre-restart selection was Original
with body-shape and outfit overrides. Do not overwrite the user's later choices
with the pre-restart backup. Freshly snapshot their current selection before a
future trial and restore only that trial's changes.

Next: inspect the remaining palettes and mask boundaries, exercise the 12-chip
pages, Default/cycling, and saved appearance restoration. Do not call the
candidate or v1.0.0 release ready until those checks pass. The animation expansion
and official-shell appearance selector remain queued.

## Color repair, 2026-09-21

The actual renderer was measured before changing assets. With Ivory
`221,192,162` and body-mask gray `171`, the live render target returned
`148,128,107`, exactly matching linear-light multiplication followed by sRGB
encoding. Three body texels and one face texel matched within one byte. Steam
screenshots reproduced the visibly darker skin. The complete saved state was
identical before and after the temporary Skin override. These findings rule
out a tint conversion error at the sampled full-resolution texture pixels;
they do not establish every mip level or every material's final appearance.
Evidence: `work/colors1/failure/pixels.json`, `default.jpg`, `ivory.jpg`.

`check_eve_color_masks.py` failed on colors3 with three concrete signals:
body-mask median 168 (the chip was multiplied by an already-dark texture), zero
soft areola-edge texels, and no skin layer on material 5. This is the regression
check for the rejected authoring, separate from the successful renderer check.

`repair_eve_colors.py` prepares colors4. It normalizes the skin detail layers
in linear light using a shared body reference of 180, preserving the scale
across connected skin atlases. Values above the reference clamp at white;
Default still restores the untouched original material texture. Pigmented
regions use their own measured reference so the selected shade is not darkened
again by the original pigment. The UV masks are explicitly authored against
Eve's 1024px source textures: soft chest boundaries and a local central region
within material 5, with surrounding skin assigned back to Skin. They are not
a generic anatomical classifier. Soft boundaries intentionally blend; protected
cores stay excluded from Skin.

The regression passes with body median 238, 2,080 soft-edge texels and the
required material-5 skin coverage. Alpha partitions cover the previous masks
within one byte of rounding. All controls and palette values remain identical;
only the extra material-5 skin layer changes the recipe. Hair textures and both
cooked containers remain byte-for-byte identical. Python package verification
and the native package/cache/corruption checks pass.

The colors4 trio is installed after a normal restart, with the same
`css_core-colors1.dll`. Package hash and saved-state preservation are recorded
in `work/colors1/repair-live/deployment.json`. The restarted process maps the
expected core. Final live appearance and independent-region acceptance remain
pending. Do not label this repair visually accepted from the offline checks.

### Sparse UV readback failure

The first colors4 live check exposed a second failure in CSS itself. Body and
face texels correctly brightened (for example, `210,183,154` at body `512,400`),
but material 5 remained bound to its original texture. The live regression
`check_eve_live_color.py` exits 1 for that missing render target and restores
the complete saved selection. Evidence: `work/colors1/readback-red/pixels.json`.

The former nine-point nonblack guard samples only UV coordinates .25, .5 and
.75. Material 5's central island is below .83 in image Y, while its side islands
are near the horizontal edges. All nine samples miss its visible geometry.
CSS silently rejected a valid composite, so independent controls on this
material could appear ineffective. Nonblack RGB also cannot validate a legal
all-black appearance.

The replacement checks that the canvas exists, draws before binding as before,
updates mipmaps, and requires a successful one-pixel GPU readback. The pinned
UE 5.6.1 `ReadRenderTargetRawPixelArea` returns an empty array on failure, unlike
`ReadRenderTargetPixel`, whose error sentinel is indistinguishable from valid
red content. The new check validates array layout and requires exactly one
sample, regardless of its color. A live null-target probe confirms the empty
array failure path. Both development and shipping builds pass. `css_core-colors2.dll` is installed
and mapped by the restarted game process; the colors4 package and saved state
are preserved. Live green regression and final appearance review are still
required.

### Live regression result

With `css_core-colors2.dll` and colors4 loaded, the same live regression passes.
It reads six texels across face, torso and the previously rejected material 5.
Ivory matches the expected linear-light composite within one RGB level; the
surrounding groin skin returns `212,184,155` where the body returns
`212,184,155`. A second run with `--rgb 000000` keeps the render targets bound
and returns black at all six points. Both trials restore the complete saved
state byte-for-byte as parsed JSON. Evidence: `work/colors1/ivory-check` and
`work/colors1/black-check`. Steam screenshots were inspected. This verifies
the sparse-atlas regression and the selected shade's texture output; complete
visual acceptance of all anatomical swatches remains with the user review.

Reproduce against the running CSS preview (coordinates are Eve-specific):

```sh
python3 tools/authoring-probes/colors/check_eve_live_color.py \
  --metadata ../CSS-Mod-Authoring/eins0fx-collections/CSS_SeduXtress_eins0fx/work/colors4/metadata \
  --output work/colors1/next-check
```

Use a fresh output folder. The check temporarily changes only Skin, captures
Steam screenshots, reads actual material textures and restores Skin afterward.
Use it while controls are idle. It aborts without starting the trial if CSS is
closed. Add `--rgb 000000` for the valid-black case. Do not run another runtime
request producer or screen recorder concurrently.
