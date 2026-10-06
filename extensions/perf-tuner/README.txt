CSSX Perf Tuner
===============

One goal: more frames per second with the picture you already have. The tuner
never touches resolution, the upscaler, quality levels, the frame cap or
v-sync; those are yours, in the game's Video settings. What it does:

- Applies a short list of engine tweaks that cost nothing visible (a bigger
  texture pool so textures stop being re-streamed as you turn, no forced
  purge when a dungeon streams out, and a few things worth measuring on your
  card). Every tweak is set through the game's console, read first, and put
  back when the tuner is off or the game closes.
- Measures. Pick a tweak (or all that are on) and press Measure it: the tuner
  switches it off, on, off, on in the scene you are in, fifteen seconds each,
  and tells you the gain in plain numbers. A gain only counts when both on
  windows beat both off windows. Trust the numbers, not the list.
- Finds the bottleneck. "Where does the time go?" renders a few seconds at
  native resolution and tells you whether the GPU or the CPU is the limit, so
  you know which tweaks can help at all.
- Checks your own settings. Things in your config that cost frames for nothing
  on this machine (frame generation turned on for a card that cannot do it,
  GPU crash debugging left on, a frame cap from a launch option) are listed
  with what they mean. Nothing there is changed by the tuner.
- Reads your Engine.ini. If you installed a tweak pack from the web, every line
  is judged: lines that do nothing in this engine, lines that change the look
  (not free frame rate), lines the game itself wrote, and lines the tuner takes
  over while it is on. The short list is in the menu; the full one is written
  to Mods/CSSX/output/eins0fx.perf-tuner/engine-ini-report.txt. The file itself is left alone.

Requires CSSX. Install CSSX first, then drop this folder into
Sparta/MortalShell2/Binaries/Win64/ue4ss/Mods/CSSX/extensions/ alongside the
other extensions. Windows and Proton (Linux) both work; tweaks that do not fit
your setup (another card maker, too little video memory, Steam Deck) are shown
greyed out with the reason.

Using it (CSSX tab > Perf Tuner)
--------------------------------
- Tune: the one button. Press it in the world and leave it for about eight
  minutes (keep playing or stand still). It turns the tuner on, audits the
  area, finds where the frame time goes, measures every switch that fits your
  machine off and on twice each, keeps the ones that win, leaves the rest off,
  and measures the kept set as a whole. The row then shows what it kept.
  Run it again in another area or after a driver change; results differ by
  scene.
- Perf Tuner: the master switch. Off puts everything back.
- Frame rate: the last five seconds, from the mod loader's own frame clock.
- Where does the time go?: the bottleneck probe, sixteen seconds. Stand still.
- Measure: choose what to measure, then Measure it. About a minute; keep
  playing or stand still, either is fine. The result stays next to the tweak.
- Smoothness / Worth measuring: the tweaks, each a switch with what it does
  and the console variables it sets. "Worth measuring" ones start off: measure
  first, keep what wins on your card.
- Your Engine.ini and Check: read-only.

tweaks.json
-----------
The tweak list is a plain file you can edit with the game closed. The tuner
refuses any entry that would lower the picture (resolution, quality groups,
mip or LOD bias, the upscaler, frame caps, Nanite detail) and says why in the
menu. Values can scale with your card's memory ("from_vram").

What it will not do
-------------------
Promise frames it has not measured. Most "free fps" lines passed around the
web are either already the engine's default in 5.6, do not exist in this
engine any more, or quietly lower the picture. The tuner's job is to tell
those apart on your machine.
