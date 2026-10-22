# Reporter screenshots and older outfit textures

October 22, game build 26073152. The supplied screenshots show Eve Skin Suit
aiming with an incorrectly raised support arm. A separate comment from
madeofnapalm reports grey textures on Dark Elf, Goth Girlfriend and "Genessa
with a big cake". The user confirms the last name means Genessa Skin.
Neither report supplies exact installed versions or a complete reproduction.

Neither symptom was reproduced on the tested local installation. This is a
bounded visual comparison, not a claim that the reporter's installation works.
No runtime source, installed asset, release archive or version was changed.

## Textures

Fresh native game captures cover front and back views of all nine variants:

| Package | Installed release | Variants checked |
| --- | --- | --- |
| BeauteKnightPROXIMA | 1.0.1 | Dark Elf Ranger, Dark Elf Ranger Light |
| BigTittyGothGirlfriend | 1.0.1 | Casual, Obsidian |
| Genessa Skin | 1.0.16 | Original, Pale, Human, Human warmer |
| HIT2 DE Scyther | 1.1.1 | Default, additional Dark Elf candidate |

The inspected front captures show distinct skin/clothing/armor materials and
surface detail, rather than a uniform grey fallback. The four package trios
match the prepared release manifest hashes. CSS alone does not replace those
outfit containers; a player retaining older outfit files still needs the outfit
update. This does not establish that the reporter has retained old files.

## Sidearm pose

The loaded core is CSS 1.0.2, verified from the process mapping and DLL hash
`d465c84db60f88f9f1d6b305b302ebc48eb187b9691960f832f25478b41e3ad9`.
Eve's installed container trio matches the prepared 1.5.1 release.

Crossbow, Parasite Gun, Lute and Ballistazooka were checked in four conditions:
original Genessa with CSS loaded, Eve Skin Suit with default idle, Eve Skin Suit
with Eve idle, and Eve idle with stowed weapons hidden. All 16 aim captures
record accepted LeftControl input and hand/head socket positions. Hands remain
below the head in these samples; inspected Eve screenshots show the support
hand by the weapon rather than the raised-arm pose in the report. Hiding the
stowed primary weapon does not reproduce it. No firing/damage claim follows
from this aim-only check. Other shell selections, camera pitches, outfits,
transitions and conflicting installations are not exhausted.

## Installation difference preserved

Local Shared Assets is no longer an exact copy of the prepared 1.0.2 ZIP.
Its installed `.pak` hash is
`f107d993b65fbf29cf5224ca95abbfd032cff20fdf274b3ccbd39cfb2885dbb9`.
Unpacking both versions finds four identical entry names; only
`CSS/Packages/css.locomotion/manifest.json` differs. Parsed differences are
the two version fields, now 1.0.4, and two appended tHorny Proxima adapters.
The original 23 adapters and all other manifest values are unchanged.
The `.ucas` and `.utoc` match the 1.0.2 ZIP exactly. This pre-existing local
change was preserved, not silently replaced for this audit. Do not describe
this installation as the complete untouched 1.0.2 release cohort.

## Evidence and restoration

Evidence directory: `work/reporter-visual-26073152/`.

- `check.json`: nine rear texture views and four initial Eve aim captures.
  Its subsequent native-control loop stopped on an incorrect diagnostic catalog
  ID, then restored state. The corrected complete matrix is `pose-repeat.json`.
- `front.json`: nine inspected front texture captures and camera restoration.
- `pose-repeat.json`: twelve native/Eve/default-idle/Eve-idle combinations.
- `pose-hidden.json`: four further Eve-idle combinations with stowed weapons hidden.
- `mounted-package-hashes.json` and `shared-pak/comparison.json`: package evidence.
  The earlier broad hash receipt also includes inactive backups outside Paks;
  those mismatches are not mounted-package failures.
- `final.json`: original tHorny Proxima appearance/customization and idle
  selection restored; settings exactly match the prelaunch backup; all sampled
  aim/fire keys released. Per-loop receipts also verify the original Parasite
  Gun definitions in controller/save object and unchanged equipment unlocks.

The game was closed at entry and opened through Steam in a separate workspace.
Input explicitly targeted its validated X11 window; screenshots came from the
game, with no desktop focus activation. Normal autosaves were not rolled back.
After restoring the user's state, the game quit normally and the temporary
window rule was disabled. No whole-save or exhaustive gameplay claim is made.

For further diagnosis, obtain CSS, Shared Assets and outfit versions, the actual
shell and idle settings, and a short clip showing how the raised arm starts.
For grey textures, compare installed outfit filenames/versions and check for
duplicate older containers before proposing another release.
