# Authored swatches and Eve palette checkpoint

2026-09-21. Offline candidate prepared; live appearance and menu checks remain.

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

Next: install through a normal restart, visually inspect palettes and mask
boundaries, exercise Default/cycling and saved appearance restoration. Do not
call the candidate or v1.0.0 release ready until those checks pass. The animation
expansion and official-shell appearance selector remain queued.
