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

## Final package assembler prepared

`tools/eve-fit/package_six.py --thumbnail PATH` checks the six-variant scope, five or more palettes per variant, all `/Game/` manifest references against the combined container inventory, source container hashes, thumbnail dimensions, and packed metadata/resource equality. It creates `work/eve26/six-candidate1/CSS_EveStellarBlade_eins0fx_P`; it does not install or create a public ZIP.

The independent reference check completed: all ten mesh/animation references occur in the verified combined container (`six-reference-check.json`). The assembler has not yet been run because the requested portrait is non-square and the user's local-padding/cropping choice remains pending. No replacement portrait was generated. The game was confirmed running through `/proc` executable entries during this check, so no restart was assumed.

## Combined package loader check and replacement backups

`package_six.py --output six-check1` produced a verification-only trio with the existing thumbnail. Python package verification passes. Current native C++23 package loader tests were built separately in `six-host` and pass against that trio: one catalog, thumbnail loading, resource caching, cache repair and corrupt-index rejection. The retained test log is `six-loader-test.log`; the logged run uses workspace TMPDIR. This is not live game rendering or persistence evidence.

User explicitly authorized backing up and removing the installed split trials and old Eve package before installing the new combined trio. All six current installations were copied to `backups/eve-six1` and checked against the fresh audit hashes. `receipt.json` records source paths and hashes. Installed copies have not been removed yet. Recheck them against this receipt before replacement so another agent's changed files are not discarded.

The verification copy uses the old thumbnail and must not become the public download. Final candidate still requires the selected portrait and live review.

## Replacement transaction prepared

`install_six.py` uses only the six explicitly recorded packages. It requires the game stopped, checks installed files and workspace backups against their hashes, stages the final candidate outside Paks, moves old directories out of mount discovery, and publishes the verified trio. On an installation failure it restores all retired directories. It does not launch the game or alter CSS runtime files.

Workspace-only fixture checks cover success and an injected verification failure after publication (`swap-check1/results.json`). Success leaves only the new trio; failure restores all six old packages byte-for-byte; both preserve backups. These exercise filesystem transaction behavior, not real game installation.

A live `inventory_inspect` request timed out waiting for a developer acknowledgement. Main runtime status instead acknowledged that request as `Unknown CSS command`. Do not repeat this unsupported helper or install a different CSS core over Claude's work. Use available screenshot/recording controls and user review for the eventual combined build.

## Author naming evidence

Read the supplied author PDF into `work/eve26/author-page.txt`. Its feature list distinguishes game outfits from additional outfits but does not give official game names for the bikini or sweater. Source object names retained in the original-scene inspection and Gemini's export selection are `Eve Bikini - Top`, `Eve Bikini - Shorts` and `Eve Extras - Sweater` (with Extras heels/glasses). This does not establish a match to a named official Nano Suit. Keep Vacation Bikini and Casual Knitwear as descriptive mod labels and do not advertise them as verified official names. No source blend or outfit geometry was changed during this check.

## Final thumbnail and candidate assembled

User clarified maximum 1:1 crop anchored at top-right, then resize to 512 x 512. Source `Untitled_9bsq091.png` is 2160 x 3840; the crop is (0, 0, 2160, 2160). ImageMagick produced `work/eve26/thumb/thumbnail.png`; visually inspected. `thumb/receipt.json` records source/output hashes and exact rectangle. This supersedes the earlier screenshot and padding question.

`six-candidate1/CSS_EveStellarBlade_eins0fx_P` is now assembled with the selected thumbnail, all six variants and updated container hashes. Package verification passes; current native loader tests pass against this exact candidate (`six-final-loader.log`). Installation and live review remain pending; no public ZIP has been replaced.
