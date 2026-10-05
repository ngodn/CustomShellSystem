#!/usr/bin/env bash
# Author CINE's materials in a fresh stage directory with the Linux UE 5.6.1 editor.
# Usage: author.sh NEW_STAGE_DIRECTORY
set -euo pipefail
[[ $# == 1 ]] || { echo 'Usage: author.sh NEW_STAGE_DIRECTORY' >&2; exit 2; }
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
root=$(cd -- "$here/../../../.." && pwd)
engine="$root/CSS-eins0fx-collections/reference-tools/UnrealEngine-5.6.1-installed/Engine"
[[ -x "$engine/Binaries/Linux/UnrealEditor-Cmd" ]] || { echo 'Linux UE 5.6.1 editor not found.' >&2; exit 1; }
stage=$(realpath -m "$1")
[[ ! -e "$stage" ]] || { echo 'Stage exists; use a new directory.' >&2; exit 1; }
# Other pipelines on this machine share the editor; never run two at once.
for comm in /proc/[0-9]*/comm; do
    if [[ "$(cat "$comm" 2>/dev/null)" == UnrealEditor-Cm* ]]; then echo 'Another UnrealEditor-Cmd is running.' >&2; exit 1; fi
done
mkdir -p "$stage"
cp -r "$here/project/." "$stage/"
mkdir -p "$stage/scratch" "$stage/ddc" "$stage/user" "$stage/Content"
set +e
bwrap --ro-bind / / --bind "$root" "$root" --bind "$stage" "$stage" --bind "$stage/scratch" /tmp \
    --dev-bind /dev /dev --proc /proc --unshare-net --die-with-parent -- \
    "$engine/Binaries/Linux/UnrealEditor-Cmd" "$stage/CineAssets.uproject" \
    -unattended -nosplash -NullRHI -NoSound -NoShaderCompile -ddc=NoZenLocalFallback \
    "-LocalDataCachePath=$stage/ddc" "-UserDir=$stage/user/" "-abslog=$stage/author.log" \
    -run=pythonscript "-script=$here/author_materials.py" > "$stage/author-console.log" 2>&1
result=$?
set -e
echo "editor exit $result"
[[ -f "$stage/cine-materials.json" ]] || { echo 'Authoring did not finish; see author.log.' >&2; exit 1; }
cat "$stage/cine-materials.json"
