# Genessa doubles appearance goal

Requested October 4, 2026, after acceptance and packaging of CSS beta.10.

Make Genessa's summoned doubles inherit the player's current CSS appearance,
starting with Unholy Genessa, while preserving the game's Faithful and Stray
ghost presentation and ability behavior.

Support all appearance sources selected through CSS: CSS outfits, non-CSS
replacements, NPC/enemy appearances, and other original shells. The actual
gameplay shell must be Genessa; changing another shell's appearance does not
grant Genessa's abilities. Include unsaved customization. Use an original-double
fallback for unsupported assets, but do not use that fallback to exclude the
supported cases from the completion requirements.

Research entry point: [index](genessa-doubles-index.md). Current evidence and
next steps: [status](genessa-doubles-status.md).

## Initial evidence

The game's local CXXHeaderDump declares:

- `BPC_AstralAISpawner`: owner, spawned-character list, spawn-completion event,
  activation, cache and uncache paths. `NewSpawn` distinguishes fresh actors
  from reused ones.
- `BPC_AstralAI`: character mesh, `MID_Astral`, material, spawn/despawn systems,
  fade lifecycle, weapon and sidearm animation layers, and a
  mesh-customization-completion callback.
- `GA_AstralClones_Action`: primary/secondary clone references, spawn/removal
  functions, melee/ranged activation and a spawn callback.

These declarations establish useful investigation points. They do not yet prove
which callbacks run in each Genessa form, their ordering, or how each ghost
material is assigned. The blue/red distinction is the author's observed target;
trace its actual material/cue setup in cooked assets before implementation.

## Work

1. Trace both Faithful and Stray abilities through their cooked spawn,
   initialization, attack, fade and reuse paths. Identify the player's own
   doubles precisely, without changing unrelated ghosts, NPCs or summons.
2. Copy the currently applied appearance and customization: body/skin variant,
   morphs, visible modular parts, palette/material settings, sheer-fabric
   visibility/opacity, wings, glow and supported visual physics. Use current
   effective appearance, including unsaved edits, rather than only a saved
   profile. Refresh reused doubles so old outfits or settings do not persist.
3. Preserve the game's Faithful blue and Stray red ghost treatment, including
   animated effects, spawn/despawn and fades. Compose this with the custom
   materials without losing wing/fabric cutouts, revealing hidden parts or
   turning sheer sections into opaque sheets. Native ghost tint must remain
   recognizable. Document any material limitation instead of silently dropping
   customization.
4. Keep each double's own attack and movement animation, weapon/sidearm layers,
   targeting, damage, ability costs, timing, collision and lifetime. Adapt visual
   rig/cloth/wing behavior to its own pose. Do not copy the player's live pose or
   force a player idle over the double's combat animation.
5. Implement reusable appearance support in CSS's C++23/UE 5.6.1 runtime where
   appropriate. Change individual outfit packages only if evidence shows an
   asset change is needed. Preserve the accepted beta.10 release and commit new
   work by context for a later release.
6. Keep ownership and cleanup scoped to each double. Handle multiple doubles,
   cached reuse, customization changes, shell/form switches, CSS disable,
   Original appearance, death/revive, travel, missing assets and actor removal.
   Avoid per-frame world scans, repeated asset loading or material creation.
7. Test both forms, fresh and reused doubles, both supported body skins, edited
   shapes, palettes, hidden/transparent fabric, wings and glow. Check melee and
   ranged behavior where supported, effect cleanup and repeated summons.
   Regress player appearance, aiming, locomotion, profiles and gate recovery.
   Measure frame cost and resource cleanup rather than promising zero impact.
   Start with Unholy Genessa, then test Eve, Commander White and representative
   original-shell, NPC/enemy and non-CSS appearances. Include Eve and Commander
   White material support, including their hair cutouts, current colors and
   modular garment surfaces. Share companions between compatible material
   families where possible; do not assume one companion supports unrelated
   graphs. Keep each release ZIP and its BBCode
   changelog together in the agreed version directory.

## Existing pending work

Unholy Genessa's metallic/glowing garment parts fit in Inventory/CSS but clip in
gameplay. Keep this investigation active in the work queue. Compare identical
customization values, pose, skin weights, physics and update order before
changing the outfit. This clone request does not establish its cause or fix it.

Done means both kinds of doubles visibly match the current customized outfit
under their native ghost effects, retain normal ability behavior, and pass the
above regression checks and the author's in-game review. Prepare ZIP/BBCode
deliverables only for the subsequently agreed release versions.
