# Planet outfit identification and tail attachment

Checked 2026-09-27 against online gameplay imagery and the installed Stellar Blade assets. Holiday remains parked; fit the existing Planet model next, then Skin Suit.

## Identification

The existing `SK_Eve_PlanetDiving.mesh.json` contains objects named `Eve Prototype Planet Diving Suit`. Its shape and materials match **Prototype Planet Diving Suit**, rather than the separate Planet Diving Suit (6th). Do not substitute a different outfit or use 6th-suit images as this model's fitting reference.

- [Gameplay reference, including rear view](https://esports.gg/news/stellar-blade/how-to-unlock-the-prototype-planet-diving-suit-in-stellar-blade/)
- [Stellar Blade modding asset IDs](https://github.com/Stellar-Blade-Modding-Team/Stellar-Blade-Modding-Guide/wiki/ID%27s-Library): Prototype is `CH_P_EVE_14_1`; 6th is `CH_P_EVE_08`.

## Tail evidence

The gameplay image shows the long strip attached to the central lower-back fitting. Its lower ornament hangs free. This is a garment appendage, not the ponytail.

Read-only decoding of the installed game confirms:

- `/Game/Art/Character/PC/CH_P_EVE_14/CH_P_EVE_14_1` references `CH_P_EVE_01_Skeleton`, `CH_P_EVE_14_Physics`, and post-process `CH_P_EVE_14_1_AnimBP_C`.
- The post-process defaults contain a KawaiiPhysics node rooted at `Ab_Tail00`.
- The shared source skeleton parents `Ab_Tail00` to `Bip001-Pelvis`, followed by `Ab_Tail01` through `Ab_Tail12`. Shared skeleton membership alone does not prove every branch is weighted by this outfit.
- Our exported tail has 840 points and no dedicated tail-chain influences. Aggregate weights are pelvis 785.726059, spine_01 18.091314, butt002 18.091314 and butt001 18.091314. Normalized weights alone therefore do not establish correct tail deformation.

Keep the upper attachment fixed to the lower-back fitting and restore appropriate movement below it. Check the existing CSS accessory chains before adding bones. Preserve the production skeleton's sockets, virtual bones and accepted gameplay fixes. Do not copy KawaiiPhysics numeric settings directly into a different solver.

## Local evidence and remaining work

Under `work/eve26/planet-ref/`: `prototype.jpg`, decoded mesh in `game/`, post-process defaults in `anim/`, shared skeleton in `skeleton/`, `physics-summary.json`, and `tail-chain.json`. Decodes completed successfully without modifying the installed game. `tail-chain.json` also contains unrelated head-tail entries; filter garment names explicitly.

No attachment or weight repair has been implemented yet. Measure the exported attachment against the fitting, correct garment clearance while preserving intentional openings, then validate the attachment and tail during motion. Continue the main suit fitting rather than expanding this into another collider investigation.
