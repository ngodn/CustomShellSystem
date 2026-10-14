# Temporary Genessa CINE preset removal

Remove only the following diagnostic-created file after restoring the original
CINE preset and verifying its hash still matches the diagnostic's installed copy:

- `MortalShell2/Binaries/Win64/ue4ss/Mods/CSSX/extensions/eins0fx.cine/presets/css-diagnostic-genessa-face-20261014.cine.json`: stationary portrait fixture for the two-variant Iris Out lifecycle check. Keep its exact recipe and preset JSON with the test evidence; it is not a shipped preset.

Reload the preset list after removal. Keep the screenshots, logs, state snapshots,
current packages, source assets, other sessions' presets and rollback files.
No large intermediate outputs are planned. Record the actual outcome in the test
result and research note. Available space before testing: root 92G, theark 67G.

Outcome: the temporary installed preset was removed after each attempt's
verified recovery, including the final passing two-variant run. Recipes and
evidence remain in the three `genessa-cine-takes-20261014*` folders. No heavy
intermediates were generated or removed. Afterward `df -h` reports root 91G and
theark 66G available; rounded shared-drive totals do not measure this test's
individual disk use.

The later `genessa-cine-interrupt-20261014*` control uses the same temporary
preset and hash-checked removal. Both attempts restored saved states and removed
the installed fixture. Their small evidence folders remain.
