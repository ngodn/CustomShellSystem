#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 4 ]]; then
    echo 'Usage: run-pose-check.sh ENGINE_DIRECTORY STAGE_DIRECTORY pose|physics LOG_NAME' >&2
    exit 2
fi
engine=$(realpath "$1")
stage=$(realpath "$2")
mode=$3
name=$4
scripts=$(cd "$(dirname "$0")" && pwd)
[[ -f "$engine/Build/Build.version" && -f "$stage/CSSShared.uproject" ]]
[[ "$name" =~ ^[a-zA-Z0-9_-]+$ && ! -e "$stage/$name.log" ]]
case "$mode" in
    pose) script=check-copy-pose.py ;;
    physics) script=check-copy-pose-physics.py ;;
    *) echo 'Expected pose or physics' >&2; exit 2 ;;
esac
exec 9>"$stage/pose-test.lock"
flock -n 9 || { echo 'A pose check is already running.' >&2; exit 2; }
libraries="$engine/Binaries/Linux"
for plugin in Animation/IKRig Animation/ControlRig Runtime/RigVM ChaosClothAsset ChaosCloth; do
    libraries+=":$engine/Plugins/$plugin/Binaries/Linux"
done
bwrap --unshare-net --ro-bind / / --dev-bind /dev /dev --proc /proc \
    --bind "$stage" "$stage" --bind "$stage/scratch" /tmp \
    --setenv TMPDIR "$stage/scratch" --setenv XDG_CACHE_HOME "$stage/cache" \
    --setenv UE_LocalDataCachePath "$stage/ddc" --setenv LD_LIBRARY_PATH "$libraries" \
    --setenv CSS_POSE_CHECK_NAME "$name" \
    "$engine/Binaries/Linux/UnrealEditor-Cmd" "$stage/CSSShared.uproject" \
    -unattended -nosplash -NoSound -NullRHI -NoShaderCompile \
    -ddc=InstalledNoZenLocalFallback "-UserDir=$stage/user/" "-abslog=$stage/$name.log" \
    -run=pythonscript "-script=$scripts/$script" 9>&- >"$stage/$name-console.log" 2>&1
