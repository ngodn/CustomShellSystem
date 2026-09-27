# Knitwear review candidate, September 27

The user accepts the current offline fit with minor clipping. Stop refining small hem intersections unless gameplay reveals an obvious problem. This is not gameplay acceptance.

## Saved candidate

`/Game/CSS/EveTest/SK_KCloth4` persists the accepted W2 geometry and trimmed `PA_KCloth4` collider, with 15 cm nonlegacy backstop radii for movable cloth points. Body proportions, shared skeleton, `PA_Body` and `ABP_KnitFeet1` remain intact. Source files are preserved.

The 65-frame native sprint recording reproduces the accepted temporary backstop trial exactly, including particles and poses, with no diagnostic overrides. Maximum displacement is 2.202 cm, maximum excess over the authored distance cap is 0.542 cm, and maximum edge ratio is 2.840. These are measured diagnostics, not a guarantee of no clipping. Evidence: `work/eve26/knit-cloth4/`, `knit-ready-{copy,bind,inspect,motion,cook}.log`.

## Customization and package

`prepare_knit_trial.py` creates `work/eve26/k4trial/CSS_EveKnitFit4_P`, a private fitting package using installed Eve dependencies. Original plus Xion Evening, Oasis Sage, Desert Rose, Orbital Ivory and Lily Lavender recolor clothing only. Sweater, shoes, buckle and glasses frame have separate color controls. Glass lenses remain unchanged. Each color control has 12 swatches including Default.

Sweater (including neck ribbons), shoes, glasses and hair have visibility controls. Six body/hair color controls retain the accepted resources with slots remapped by material identity. Five motion controls and six shape controls are included. Existing idle ID `eve` is retained, with display name Eve Default Idle.

Package/resource verification and cooking finished successfully. The atlas sheet and Xion Evening front/Lily Lavender rear model renders were inspected. The model renders verify texture placement only: neutral body, hair omitted, no game shader or cloth simulation, and footwear shown without its runtime corrective graph.

The complete trio was installed as a new directory while the game was running. It replaces no mounted package and requires the user's next normal launch to activate. Installed bytes match the source hashes in `k4trial/install.json`. No restart or game input was sent. Release archives are unchanged.

## Remaining review

After the next user-controlled restart, check Knitwear in the world through turns, sprint and attacks; check motion controls, garment visibility, all palettes, Default restoration and saved selections. Do not equate package verification with these live checks. Final delivery still merges accepted outfits into one `CSS_EveStellarBlade_eins0fx_P` trio. Proceed to Midsummer Alice fitting while waiting for game review, then War Aegis. Fourteen additional idles remain queued separately.
