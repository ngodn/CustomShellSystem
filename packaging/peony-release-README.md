# Eve v1.5.0: Peony

Install all three downloads for this release: Eve v1.5.0, CSS v1.0.0-beta.12,
and CSS Shared Assets v1.0.0-beta.12. UE4SS stays a separate requirement;
use the build specified in the CSS runtime README.

1. Close Mortal Shell II.
2. Extract the runtime's `CustomShellSystem` folder into
   `MortalShell2/Binaries/Win64/ue4ss/Mods/`, replacing matching files.
3. Replace `CSS_SharedAssets_P` under `MortalShell2/Content/Paks/~mods/` with
   the folder from the shared ZIP. Remove the superseded
   `CSS_AstralSharedAssets_P` and `CSS_LocomotionSharedAssets_P` folders if present.
4. Replace `CSS_EveStellarBlade_eins0fx_P` in the same `~mods` directory with
   the Eve ZIP's folder. Keep every package's `.pak`, `.utoc` and `.ucas` together.
5. Keep your settings and profiles. Launch the game and select Eve > Peony in CSS.

Peony includes four palette templates, removable pieces, body-shape controls
and optional glowing, flowing trim. Black and white floral is the default.
The existing six Eve outfits remain included. Slipstream is deferred.

The author accepted Peony's fit in CSS and gameplay, palette/profile restoration,
trim flow, facial playback, performance interruption, customized doubles in the
tested Genessa form, and Firm/Natural motion while walking and playing Iris Out.
Minor clipping can remain in some poses or at extreme body settings. The full
per-outfit, both-form, transition and performance test matrix is not complete.
The separate reported sunlight-noise issue has not been reproduced locally;
this release does not claim to fix it.

Peony/Hydrangea source: [V2 CNS and recolors](https://www.nexusmods.com/stellarblade/mods/1724).
CSS adaptation by eins0fx. Original Stellar Blade assets by SHIFT UP.
Additional animation credits are provided in `ANIMATION-CREDITS.txt`.
