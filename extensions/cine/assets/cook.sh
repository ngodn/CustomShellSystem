#!/usr/bin/env bash
# Cook an authored stage for Windows (compiles the D3D SM5/SM6 shaders) in the workspace's Wine
# container. Uses the Windows UE 5.6.1 editor and container image kept by the Unholy Genessa
# package's tooling; neither is modified.
# The cook directory must be under /home: the workspace daemon does not see other drives.
# Usage: cook.sh AUTHORED_STAGE_DIRECTORY NEW_COOK_DIRECTORY
set -euo pipefail
[[ $# == 2 ]] || { echo 'Usage: cook.sh AUTHORED_STAGE_DIRECTORY NEW_COOK_DIRECTORY' >&2; exit 2; }
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(cd -- "$here/../../../.." && pwd)
tools="$root/CSS-Mod-Authoring/eins0fx-collections/CSS_UnholyGenessa_eins0fx_P"
stage=$(realpath "$1")
[[ -f "$stage/cine-materials.json" ]] || { echo 'Stage has no authored materials.' >&2; exit 1; }
cook=$(realpath -m "$2")
[[ ! -e "$cook" ]] || { echo 'Cook directory exists; use a new one.' >&2; exit 1; }
[[ "$cook" == /home/* ]] || { echo 'Cook directory must be under /home.' >&2; exit 1; }
[[ -f "$tools/work/ue56/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ]] || { echo 'Windows UE 5.6.1 editor not found.' >&2; exit 1; }
image_id="$(bash "$tools/tools/docker-local.sh" image inspect unholy-wine:11.7 --format '{{.Id}}')"
# A clean cook project: content and config only, no editor leftovers and no Python plugin.
mkdir -p "$cook/project" "$cook/out"
cp -r "$here/project/Config" "$cook/project/"
cp -r "$stage/Content" "$cook/project/"
printf '%s\n' '{"FileVersion": 3, "EngineAssociation": "5.6", "Description": "CINE materials: Windows cook only"}' > "$cook/project/CineAssets.uproject"
printf '%s\n' "$image_id" > "$cook/image"
set +e
bash "$tools/tools/docker-local.sh" run --rm --init --user 0:0 \
    --name cine-material-cook --network none --shm-size 1g \
    --mount "type=bind,src=$tools/work/ue56,dst=/ue,readonly" \
    --mount "type=bind,src=$cook/project,dst=/project" \
    --mount "type=bind,src=$cook/out,dst=/out" \
    --mount "type=bind,src=$here/cook-inner.sh,dst=/cook.sh,readonly" \
    "$image_id" bash /cook.sh > "$cook/cook.log" 2>&1
result=$?
set -e
printf '%s\n' "$result" > "$cook/exit"
echo "cook exit $result"
exit "$result"
