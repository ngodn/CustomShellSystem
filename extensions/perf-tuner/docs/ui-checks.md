# Perf Tuner UI checks, 6 Oct 2026

Scope: native menu model, wording and rendered game screenshots. Navigation and activation
used dev-channel calls to CSSX's native key/action handlers. This checks the menu path;
it does not test physical keyboard events, a controller or mouse hit coordinates.

- R-02/R-16/R-36 PASS: rewritten descriptions use plain action/effect wording. Text scan
  found no em or en dashes in the menu source, manifest, README or catalog. Removed the
  false engine-default claim and unsupported bottleneck/overflow conclusions.
- R-17/R-38 PASS: frame time and machine values come from host data. The live run found
  no optional gain and rejected the combined set on p99 regression. Recorded in findings.md.
  Old resolution verdicts without verified writes are no longer displayed.
- R-24/R-26 PASS at model level: Tune, Enabled, Stop, Run test, Resolution test, Scan
  scene and tweak switches have native event handlers. The mock run verifies changes
  and originals independently. Stop stays enabled while a diagnostic is active.
  Live: Tune activated through native Accept; Stop tuning and Stop test through native
  navigation/Accept. All 26 watched cvars restored. Resolution confirmation opened and
  cancelled without running it. Manual test changed a cvar while Enabled was off, and
  Stop remained available and restored it.
- R-27 PASS at model level: unloaded world explains that a save must be loaded;
  unmeasured rows say so; running work reports progress; refused resolution changes
  publish no new verdict; refused cleanup retains originals and reports an error.
- R-37/R-31 PASS: DESIGN.md records the native-menu direction and 1/1/1 dials.
  Tune is first; Tests is separate; long technical explanations stay out of status.
- R-33 PASS: changes live in the C++ menu model and catalog, without a UI patch script.
- R-35 PASS for the offline build: 11 host suites and 121 tuner checks pass; Windows
  release and dev DLLs build. No new images, fonts, colors, theme or host layout shipped.

Rendered checks: Tune, Tests, confirmation, Frame rate, both Stop states and completed
rollback summary were legible at the game's current viewport. Selected rows remained
highlighted and native input prompts were present. The live pass found that CSSX hides
label/button values. The tuner now uses label descriptions/button hints, with regression
checks. Label text stays on one line before native wrapping to avoid exposed markup tags.

The core and loader binaries were retained. Reloads used an identical temporary copy of
the installed core; its original selector was restored afterward. Scan/idle costs are in
findings.md. Tuner is off after the live pass; player config files were not written.

Remaining: physical controller/mouse checks, other viewports and contrast/accessibility
certification of the shared native theme. No theme, color or font changes shipped here.
