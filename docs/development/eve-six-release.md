# Six-outfit release assembly

Package filename: `CSS_EveStellarBlade_eins0fx_P`.

Scope: Black Pearl, Prototype Planet Diving Suit, Skin Suit, Vacation Bikini, Casual Knitwear and Midsummer Alice. Holiday Reveler and War Aegis are deferred by explicit user instruction. Fourteen additional idles remain unfinished and must not be advertised as included.

## Current evidence

`work/eve26/six-audit.json` records freshly verified metadata and container hashes for the installed base and five private candidates. All source trios match their installed copies. Black Pearl has six palette definitions; each new candidate has five, alongside Original restoration. This is package evidence, not new gameplay acceptance.

`tools/eve-fit/stage_six_metadata.py` completed successfully into `work/eve26/six-meta1`. It selects only the six retained variants, preserves package identity, checks source hashes, prefixes every used dye resource by source outfit, and verifies all remapped control recipes. The combined draft manifest is 111468 bytes, below the 256 KiB runtime limit. It is not a release package: container references still describe the base.

The base has Black Pearl-specific outfit templates. Review their variant applicability before shipping; do not expose its clothing toggle IDs against unrelated variants.

## Thumbnail

The user selected `~/Pictures/screenshot-2026-09-27_18-30-49.png`; visually confirmed against their chat image. Exact source copied to `work/eve26/thumb/source.png`. It is 356 x 332, while CSS requires square PNGs. Local padding versus cropping is pending user choice. Preserve the source image.

## Names

[Published outfit list](https://www.pushsquare.com/guides/stellar-blade-all-outfits-and-how-to-get-them) confirms Black Pearl, Prototype Planet Diving Suit and Skin Suit. [Midsummer Alice reference](https://stellarblade.fandom.com/wiki/Midsummer_Alice) confirms that name and the matching bow-front swimsuit construction.

Vacation Bikini and Casual Knitwear are retained working names, not confirmed official game names. Inspected current original-color renders. Do not rename them Blue Monsoon or Daily Knitted Dress merely because both are swimwear/knitwear: the published designs differ. Check original author provenance before choosing a different public label.

## Remaining assembly

Combine cooked assets while preserving accepted base dependencies and rejecting conflicting duplicate asset paths. Round-trip the combined container and compare decoded payloads. Replace thumbnail, update hashes and verify the embedded manifest/resources. Review outfit switching, colors, modular controls, physics and persistence in game. Only then replace release archives. Do not modify Claude-owned runtime changes.

## Combined asset container verified

`build_six_assets.py` completed into `work/eve26/six-assets1`. All 323 unique asset paths are under `/Game/CSS/`. The sole candidate overlap, ABP_KnitFeet1 in Alice and Knitwear, has identical decoded uasset/uexp hashes. Combined retoc verification passed, then extraction using game dependencies plus only the combined mod matched every decoded header and every uexp/ubulk/uptnl payload. No installed package or release ZIP changed.

The container preserves the entire accepted base dependency set, including unused legacy outfit assets. Only six variants are exposed by the staged manifest. Removing unused cooked assets is deferred until dependency reachability can be proved.

Runtime source confirms outfit-level templates have no variant filtering. Removed Black Pearl-only clothing combinations from the combined metadata to avoid no-op presets on other outfits. Individual toggles remain intact. Retained shared physics presets, checked their control IDs exist in all six variants, and renamed their Normal display label to Default. This does not modify CSS runtime code.
