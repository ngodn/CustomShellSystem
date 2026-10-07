CSS Locomotion Shared Assets 0.1.0-dev
Private trial, gameplay acceptance pending.

63 animation choices for CSS and CINE: 8 idles, 31 photo poses, 7 dances,
11 other performances, 3 catwalks, and Walk, Jog and Sprint.

Use with the matching animation-library CSS/CINE trial. CSS beta.11 alone
does not provide this new library. The asset ZIP contains no runtime DLLs.
The exact tested runtime hashes are recorded in candidate.json.

INSTALL
Close the game. Copy the CSS_LocomotionSharedAssets_P folder into:
MortalShell2/Content/Paks/~mods/
Keep its .pak, .utoc and .ucas files together. Replace an earlier copy in
that same folder; do not leave another copy elsewhere in ~mods.
Keep your existing outfit packages installed. They do not need repacking.

The full library targets Eve's six outfits, both Unholy Genessa faces, and
Commander White. Other compatible default game human skeleton appearances
receive only Eve Default Idle, Eve Walk, Eve Jog and Eve Sprint.
Other custom skeletons are not covered by this trial.

Facial support is separate from body animation support. Eve has 21 expression
choices, Commander White has 9 including Neutral, and each has 6 facial
performances. Unholy Genessa's rigid native mask has no expression controls
in this pack. Outfit, animation and facial restoration still need live checks.

Existing saved animation IDs are retained. CSS prefers a compatible shared
entry and can use an audited bundled equivalent when the shared entry is
unavailable. A shared-only choice cannot play after removing the pack.

KNOWN LIMITATIONS
Some poses and performances intersect the floor. They remain included;
further floor-contact work is deferred. This trial is not a claim of complete
visual, gameplay or performance acceptance. Some extra source-bone definitions
remain unresolved; see the development acceptance record before public release.

UNINSTALL
Close the game and remove only CSS_LocomotionSharedAssets_P from ~mods.
Keep the outfit packages, profiles and other shared packs.

Source credits and links are in CREDITS.txt. No music is included.
