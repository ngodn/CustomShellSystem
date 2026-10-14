# Commander White permanent paths in the running game

The final walking BlendSpace and all eight directional animations resolve in
the current game session. This closes the pending startup path-resolution check
after the accepted walking correction was moved out of its study directory.

Evidence: `work/three-outfit-rig-physics/cw-release-live-20261014/result.json`,
`requests.jsonl`, `owner.json`, both saved-state snapshots and `recipe.py`.
Recipe: `tools/diagnostics/capture_cw_release_bindings.py`.

The diagnostic checks the installed CW trio against the accepted final-path
installation receipt and pins those hashes, the accepted core and the source
mapping/blend receipts. The inputs still match after capture. PID 1944353 stays
unchanged throughout. The separately captured process mapping already identifies
the accepted CW core in this session; this diagnostic checks its selector/file.

## Observed bindings

- `/Game/CSS/eins0fx/CommanderWhite/Animations/Locomotion/Walk/BS_CW_Walk`
  was already loaded when inspected. The diagnostic did not need its explicit
  load fallback.
- All 36 reflected samples use the expected permanent animation paths. Their
  sample coordinates and rate scales exactly match the accepted source data.
  The eight distinct clips are `AN_CW_Walk_B`, `BL`, `BR`, `F`, `FL`, `FR`, `L`
  and `R`, under the same permanent Walk directory.
- The active mesh is `Faces/SK_Facial20`; its active post-process is
  `Animations/Adapters/ABP_FacialPerformanceBelt20`. The owner checker matches
  the installed shared metadata and reports a stable capture.
- Exactly one `Rig/CR_Belt20` instance is present. Enabled, Initialized and
  SupportedScale are true, TotalSteps is 101 and ResetCount is 3 at capture.
  All six post-process shape values equal the belt's six bridge values, at
  the remembered zero/default shape.
- CINE's performance input remains enabled with weight 1. The animation status
  reports no error. Base locomotion is not engaged in this stationary capture.

The diagnostic temporarily selected CW through the established CSS service,
then restored Peony, its customization and idle override. The entire saved
CSS state compares equal before and after. No camera, movement, performance,
idle, motion-preset or shape commands were sent. No package/core replacement
or reload occurred.

## Limits and next checks

This is path resolution and active rig/adapter evidence. It does not repeat or
extend the user's accepted walking judgment, show the final BlendSpace actively
driving a walking frame, or measure continuous facial playback. Native grounding,
terrain/lifecycle, extreme-shape appearance and matched performance checks remain
separate. Preserve the accepted belt, skirt and gait corrections.

Repeat with one running game, a settled non-CW appearance and a new directory:

```sh
python3 tools/diagnostics/capture_cw_release_bindings.py \
  --output work/three-outfit-rig-physics/cw-release-live-recheck
```

The diagnostic refuses to overwrite a concurrent appearance choice. It retains
request/reply records and restoration results if a read or assertion fails.
