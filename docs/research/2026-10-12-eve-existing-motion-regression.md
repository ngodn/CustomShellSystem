# Existing Eve outfit preset regression

Evidence: `work/three-outfit-rig-physics/eve-six-body-inputs1`.

The accepted tighter body rig passes the saved secondary-graph input check on
all six older Eve outfits, normal and facial meshes, at 30, 60 and 120 fps.
There are 36 cases and 20,196 evaluated frames. The editor exits 0 and all
protected source hashes remain unchanged. No game files or settings changed.

| Outfit | Saved secondary graph | Cases |
| --- | --- | ---: |
| Black Pearl | ABP_Secondary | 6 |
| Prototype Planet Diving Suit | ABP_Secondary | 6 |
| Skin Suit | ABP_Secondary | 6 |
| Vacation Bikini | ABP_BikiniFeet2 | 6 |
| Casual Knitwear | ABP_KnitFeet1 | 6 |
| Midsummer Alice | ABP_KnitFeet1 | 6 |

## Method and inputs

The existing UE 5.6.1 `CSSBodyInputs` native fixture is reused without rebuilding.
Its binary hash is retained. The matrix comes from the retained Eve/shared
manifests and decoded mesh bindings. Each source mesh's actual saved
post-process class must agree with that matrix before evaluation.

Authoring content comes from the retained `eve-rig-editor2` migration source.
Only `/Game/CSS/eins0fx/EveSB/CR_Body` is overlaid with the accepted
`peony-v2-body-limits1` source. Its file hash must match `authored.json`, and the
previous fresh reload must have passed. All source assets are symlinked into a
private project and mounted read-only, with only the new output directory
writable. This avoids copying whole authoring projects.

The fixture writes Firm, Natural, Off and Firm once at two-second boundaries.
It checks the expected values in both public AnimBP inputs and the active body
rig every frame, while moving the component. Across all cases:

- Exactly one body update is consumed per animation frame.
- Resets occur only at the preset boundaries after initialization.
- The body solver takes 4, 2 or 1 substeps at 30, 60 or 120 fps respectively,
  except on initialization/preset-reset frames.
- The expected body, hair and hand rig nodes are present.

Seven damaged-response checks are rejected: missing mesh, wrong graph, duplicate
case, duplicated update, extra reset, stopped solver and native failure. The
existing six-case Peony response still passes the generalized verifier.

The commandlet log duplicates some log lines. Its warnings include attempted
editor-preference writes rejected by the read-only mount; they are not source
asset changes. Exact invocations, raw results, matrix, source hashes and checker
hash are retained with `verified.json` and `negative-controls.json`.

## Reproduction and limits

Host Python is 3.14.7; the native helper remains the existing UE 5.6.1 C++20
fixture. The commandlet uses the engine's embedded Python.

```sh
python3 tools/diagnostics/run_eve_graph_inputs.py work/three-outfit-rig-physics/eve-six-body-inputs-new
python3 tools/diagnostics/verify_body_graph_inputs.py work/three-outfit-rig-physics/eve-six-body-inputs-new
```

The launcher requires a new output directory and records the full command.
Its retained commandlet approach follows
[Epic's Python scripting documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python).

This completes the older saved meshes' preset-input/update regression with the
accepted tighter rig. It does not execute CSS reflected writes, gameplay main
AnimBPs, playback adapters, clip-driven motion or cloth. The native fixture
explicitly disables cloth. It does not establish visual fit, collisions,
performance or full gameplay lifecycle acceptance. Those goal checks remain
open. No release artifacts were rebuilt or promoted.
