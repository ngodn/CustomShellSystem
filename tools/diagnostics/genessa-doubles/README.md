# Genessa doubles capture

Requires the already installed, running MS2AttackProbe with its on-demand
`repair_` request handler. Python 3.14 on Linux and UE4SS Lua 5.4. This is a
development diagnostic, not a CSS runtime dependency.

```sh
python3 tools/diagnostics/genessa-doubles/capture.py \
  /path/to/ue4ss/Mods/MS2AttackProbe \
  work/genessa-doubles/session.log --seconds 180
```

The user summons normally. The capture reads only their owned spawner and
its actor list, bounded to 32 actors and 128 material slots per actor. It sends
no gameplay input, changes no materials, and loads no game assets.

Use `--mode materials` for current player base/overlay parent chains and scalar,
vector and texture overrides. This includes transient customization MIDs.
It is not a complete material graph dump or proof of visual equivalence.

The wrapper temporarily replaces `Scripts/RepairPrologue.lua` with the
read-only callback and restores the previous bytes on completion, error,
Ctrl+C or SIGTERM. It refuses to overwrite a concurrent callback edit.
Do not run another probe client concurrently. The lock coordinates only other
instances of this wrapper. SIGKILL or host failure cannot run cleanup.

Output files must be new. Each sample ends with ASTRAL_END; errors stop capture
instead of treating a partial sample as evidence. Request logs remain in the
probe directory; the combined file copies their contents for analysis.

Research and limitations: [index](../../../docs/development/genessa-doubles-index.md).
