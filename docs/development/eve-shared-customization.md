# Shared customization on fitted Eve outfits

September 27, 2026.

Every selected outfit needs modular clothing, appropriate body/hair/clothing motion, independent color controls, Original plus five garment palettes, and saved-choice validation. Fitting alone is not completion. Preserve the accepted body proportions and rig.

Prototype F14, Skin F16 and Vacation Bikini F1 now have their garment palettes and the six shared body/hair color controls installed. Each restored control has Default plus eleven swatches. Prototype fitting, tail and body motion were accepted by the user. Skin and Bikini still need game acceptance. New color bindings on all three need live visual and persistence checks.

## Material-order regression prevention

The older Black Pearl recipe targets a different material order. Its body is slot 2; these fitted meshes use slot 0. Never copy numeric color bindings between outfits without resolving the target material identities.

`tools/eve-fit/restore_body_colors.py` maps the accepted Black Pearl masks by material name, including covered-body sections and foot/toenail lining aliases. It preserves resource hashes, garment palettes, existing controls and the cooked asset containers. The script refuses existing output directories.

Evidence under `work/eve26/`:

- `p14custom/verification.json` and `installed.json`
- `s16custom/verification.json` and `installed.json`
- `b1custom/verification.json` and `installed.json`

All three package verifiers passed. Installation completed with backup `backups/packages-0016`; every installed trio hash matches its candidate. No game launch was requested or performed. The `.ucas` and `.utoc` files are unchanged from the preceding clothing-palette packages.

These are isolated fitting packages requiring the installed Eve assets, not the final combined release. Verified mapping and resource integrity do not establish live UV alignment, color output or profile persistence. Check those on the next convenient game review. Continue unfinished outfit work without waiting for the user to restart.
