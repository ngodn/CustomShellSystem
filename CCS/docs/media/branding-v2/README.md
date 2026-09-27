# CCS branding v2

Direction: CSSX materials and colors, CSS centered composition. The full title is Custom Combat System, credited to by _eins0fx. Version 2 supersedes the red-accented, side-by-side v1 design.

## Assets

| File | Purpose |
| --- | --- |
| ../../../assets/inventory-logo-v2.png | Transparent full-title master, crest centered above title and credit |
| ../../../assets/inventory-logo-1120x373-v2.png | Export matching the existing CSS inventory logo rectangle |
| ../../../assets/banner-v2.png | Transparent full-title banner for library cards or Nexus listing artwork |
| ../../../assets/emblem-v2.png | Supplementary standalone emblem master |
| ../../../assets/icon-256-v2.png | Small icon matching CSSX's existing 256px asset; current CCS code uses a square slot |
| ccs-nexus-header-v2.png | Full promotional artwork for the Nexus description |
| ccs-nexus-header-1300x372-v2.png | Exact 1300 x 372 Nexus Header upload canvas |

The complete name and credit belong to the primary logo and banner. The small emblem is supplementary. These files are prepared artwork, not a UI integration or a published Nexus update. The full-title logo requires a wide image slot; do not put it into the current square CCS logo slot.

## Reference audit

CSS currently imports assets/inventory-logo-v1.png in native/src/engine.hpp and places it in a 1120 x 373 rectangle in native/src/inventory_native.inl. The older wardrobe-v1.png is a portrait panel reference, not used by this native logo path.

CSSX imports extensions/core/assets/logo.png (256 x 256) for its header and assets/banner.png for its framework details. Its Nexus documents select docs/media/v0.3.0/cssx-header.png for descriptions and cssx-header-1300x372.png for the Header upload.

Reviewed those assets, CSS Nexus header v2 and its upload export, wardrobe-v1.png, and the user's screenshots. CSSX's fractured charcoal material, aged gold fittings, ivory lettering and pale blue-white soul-light provide palette continuity. CSS's centered crest/title/credit hierarchy provides the requested positioning. Crossed sword and scythe identify combat customization.

## Production and checks

Artwork generated using the built-in image generation tool. ImageMagick was used only for proportional export sizing and transparent canvas padding, following the existing repository's Nexus media workflow. No crop or stretch. Original generated files retained.

- Hard gate PASS: user-requested artwork; exact title and author credit visually checked; no invented product claims. Interactive UI checks do not apply to these image files.
- Purpose PASS: the palette matches CSSX, positioning matches CSS, and weapons distinguish CCS.
- Liveliness PASS: energy 2, rhythm 2, motion 1 (static); centered title and crest are the focal point, with pale soul-light as the accent.
- Craftsmanship PASS: complete lettering and crest remain visible; transparent PNG alpha and exact export dimensions checked. Full and upload Nexus artwork are each under 3 MB.

## Full-title logo prompt

Use case: logo-brand, reference-based redesign.
Make a finished transparent in-game title logo for "Custom Combat System" by "_eins0fx". Wide 3:1 canvas 2172x724.
REFERENCE ROLES: Image 1 CSS is the EXACT centered stacked composition and typography hierarchy to follow. Image 2 CSSX is the EXACT material and color palette reference. Image 3 is the earlier CCS combat emblem idea only, its left/right arrangement and red accents are superseded.
Composition must follow image 1: a moderately sized combat crest CENTERED ABOVE the words, small "CUSTOM" centered under crest, large elegant serif "COMBAT SYSTEM" across ONE line below it, centered smaller "by _eins0fx" beneath, with delicate horizontal ornament flanking credit. All elements symmetrical in overall balance and centered on the canvas, no left-hand emblem beside text.
Crest: crossed ancient straight sword and crescent war scythe on a fractured black iron circular halo. Black hammered relic surfaces with fine antique-gold engraved sigils and small brass bindings, delicate pale blue-white soul-light in the fractures. Compact weapons stay within the crest silhouette. Replace the old red gemstone with a small subdued ivory and pale cyan light at crossing. No red or orange. No overly bright yellow gold. Muted antique brass, weathered ivory lettering and charcoal exactly as CSSX.
Lettering: slender high contrast classical Roman serif matching CSS, aged ivory faces with restrained gold edges, not chunky bronze lettering. Exact text once each: "CUSTOM", "COMBAT SYSTEM", "by _eins0fx". Essential leading underscore and digit ZERO in author name, e i n s 0 f x.
Crest upper central half, typography lower half, ornaments spread horizontally with purposeful open breathing space like CSS. Full design contained with safe margins. No extra words, no CSSX or CCS initials, no official game title.
Real transparent alpha background including letter counters and open emblem gaps. No scenery, background rectangle, checkerboard, border, neon cloud, watermark or mockup. Sharp professional final PNG, readable at inventory-header sizes.

## Nexus header prompt

Use case: logo-brand, compositing.
Create the finished Nexus Mods promotional page header for "Custom Combat System" by "_eins0fx".
Image 1 is the APPROVED full CCS brand lockup. Preserve its exact centered stacked arrangement, lettering and combat crest: emblem ABOVE "CUSTOM", then ONE line "COMBAT SYSTEM", then "by _eins0fx". Maintain the underscore and zero in the author credit. The whole lockup is centered in the whole banner, NOT positioned to the left or right.
Image 2 CSSX Nexus header is the palette, textured environment and fine perimeter border reference ONLY. Do not copy its horizontal emblem-left/title-right layout or any of its wording.
Wide landscape banner at 2172x724 (3:1), no contact sheet or website mockup. Integrate the exact image 1 brand design as the central focal point, slightly reduce total lockup height to ensure comfortable breathing space inside a fine antique gold perimeter border. Around it, black charcoal fractured stone with thin pale blue-white soul-lit cracks near outer edges, restrained antique brass ritual line engravings and worn metal grain from image 2. Quiet dark area behind text. The typography is aged ivory with antique gold edges; no red, no orange, no saturated yellow gold. Combat emblem keeps crossed sword and scythe, circular fragmented black relic, pale soul-light at their crossing.
Keep every letter, weapon tip and all four border edges entirely visible with 4 percent safe margin. Exact text ONLY "CUSTOM", "COMBAT SYSTEM", "by _eins0fx". No initials, game name, tagline, version, characters, logos from other games, UI, screenshots, watermark or extra words. Finished opaque promotional artwork in the same asset family as CSS/CSSX, with CSS positioning and CSSX palette.

## Supplementary icon prompt

Use case: precise-object-edit.
Produce the standalone square emblem variant of this supplied CCS title logo, for the small 260x260 in-game icon slot, rendered at 1024x1024.
Keep the exact central crest design and palette: crossed straight sword and curved scythe, fractured charcoal black iron circular relic with brass bindings and delicate golden etchings, ivory weapon edges, and pale blue-white soul-light in the cracks and at the center.
Remove every letter and every horizontal title ornament below and beside the crest. Keep only the weapon-and-halo crest, complete and centered on a genuine transparent alpha background with 8 percent safe margin, all blade tips visible. Do not redesign the weapons or change the palette. No red, no orange, no black rectangle, no scenery, no checkerboard baked in, no extra words. This is the supplementary icon; the supplied reference remains the primary full-title logo.

