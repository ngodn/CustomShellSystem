# Body input handoff fixture

Private UE 5.6.1 editor module, C++20. It never accesses the game, saves assets
or changes source defaults. Copy `Source` into an isolated editor project with
a `CSSBodyInputs` editor module and the ControlRig/PythonScriptPlugin plugins.
The existing project pins C++20 and the engine's Python 3.11.

Build using `Engine/Build/BatchFiles/Linux/Build.sh CSSBodyInputsEditor Linux
Development /absolute/path/CSSBodyInputs.uproject -NoHotReload -MaxParallelActions=3`.
The completed recipe and source hashes live under
`work/three-outfit-rig-physics/peony-v2-body-inputs1`.

Mount the retained Peony character2 editor content read-only. Set
`CSS_MOTION_STAGE` to the private output directory and run
`tools/diagnostics/check_body_graph_inputs.py` through UnrealEditor-Cmd's
PythonScript commandlet with `-NullRHI`. The retained `command.json` includes the
read-only filesystem mount and private writable paths.

The native fixture writes Firm, Natural, Off and Firm only at phase boundaries.
Every frame checks both the AnimBP's public values and the live body rig's values
against the expected preset. A matching pair of overwritten values cannot pass.
The Python checker checks matrix coverage, one animation update per frame and
resets only at the preset transitions. Run it after the editor exits:

```sh
python3 tools/diagnostics/verify_body_graph_inputs.py work/three-outfit-rig-physics/peony-v2-body-inputs1
```

This exercises the saved normal/facial Peony meshes and secondary AnimBP.
It does not execute CSS core, the performance adapter, native gameplay animation
or a user's movement. A passing result does not accept the reported jiggle bug.
