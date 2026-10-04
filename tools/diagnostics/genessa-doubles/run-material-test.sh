#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 4 ]]; then
    echo 'Usage: run-material-test.sh ENGINE_DIRECTORY STAGE_DIRECTORY create|render|hair|native-create|native-compile|native-render|coverage-create|coverage-inspect|ghost-hair|surface-create|surface-prepare|surface-render|eye-create|eye-compile|eye-render|body-create|body-render|refraction-create|refraction-render LOG_NAME' >&2
    exit 2
fi
engine=$(realpath "$1")
stage=$(realpath "$2")
mode=$3
name=$4
tools=$(cd "$(dirname "$0")" && pwd)
[[ -f "$engine/Build/Build.version" && -f "$stage/source-manifest.json" ]]
[[ "$mode" == create || "$mode" == render || "$mode" == hair || "$mode" == native-create || "$mode" == native-compile || "$mode" == native-render || "$mode" == coverage-create || "$mode" == coverage-inspect || "$mode" == ghost-hair || "$mode" == surface-create || "$mode" == surface-prepare || "$mode" == surface-render || "$mode" == eye-create || "$mode" == eye-compile || "$mode" == eye-render || "$mode" == body-create || "$mode" == body-render || "$mode" == refraction-create || "$mode" == refraction-render ]]
[[ "$name" =~ ^[a-zA-Z0-9_-]+$ && ! -e "$stage/$name.log" ]]
mkdir -p "$stage"/{scratch,cache,ddc,user}
exec 9>"$stage/material-test.lock"
flock -n 9 || { echo 'Material test already running in this stage.' >&2; exit 2; }
libraries="$engine/Binaries/Linux"
for plugin in Animation/IKRig Animation/ControlRig Runtime/RigVM ChaosClothAsset ChaosCloth; do
    libraries+=":$engine/Plugins/$plugin/Binaries/Linux"
done
command=("$engine/Binaries/Linux/UnrealEditor-Cmd" "$stage/AstralMaterials.uproject"
    -unattended -nosplash -NoSound -ddc=InstalledNoZenLocalFallback
    "-LocalDataCachePath=$stage/ddc" "-UserDir=$stage/user/" "-abslog=$stage/$name.log")
if [[ "$mode" == create || "$mode" == native-create || "$mode" == coverage-create || "$mode" == coverage-inspect || "$mode" == surface-create || "$mode" == surface-prepare || "$mode" == eye-create || "$mode" == body-create || "$mode" == refraction-create ]]; then
    script=create-fade-materials.py
    [[ "$mode" != native-create ]] || script=create-native-ghost-material.py
    [[ "$mode" != coverage-create ]] || script=create-ghost-coverage.py
    [[ "$mode" != coverage-inspect ]] || script=inspect-ghost-coverage.py
    [[ "$mode" != surface-create ]] || script=create-ghost-surfaces.py
    [[ "$mode" != surface-prepare ]] || script=prepare-ghost-surfaces.py
    [[ "$mode" != eye-create ]] || script=create-native-eye-materials.py
    [[ "$mode" != body-create ]] || script=create-native-body-materials.py
    [[ "$mode" != refraction-create ]] || script=create-native-refraction-material.py
    command+=(-NullRHI -NoShaderCompile -run=pythonscript "-script=$tools/$script")
else
    [[ -f "$stage/render-sources.json" ]]
    script=render-fade-materials.py
    [[ "$mode" != hair ]] || script=render-hair-fades.py
    [[ "$mode" != native-compile ]] || script=compile-native-ghost-material.py
    [[ "$mode" != native-render ]] || script=render-native-ghost-material.py
    [[ "$mode" != ghost-hair ]] || script=render-ghost-hair.py
    [[ "$mode" != surface-render ]] || script=render-ghost-surfaces.py
    [[ "$mode" != eye-compile ]] || script=compile-native-eye-materials.py
    [[ "$mode" != eye-render ]] || script=render-native-eye-materials.py
    [[ "$mode" != body-render ]] || script=render-native-body-materials.py
    [[ "$mode" != refraction-render ]] || script=render-native-refraction-material.py
    command+=(-RenderOffscreen -vulkan -AllowCommandletRendering -corelimit=8
        -run=pythonscript "-script=$tools/$script")
    if [[ ${CSS_ASTRAL_SERIAL_RHI:-0} == 1 ]]; then
        command+=(-norhithread)
    elif [[ ${CSS_ASTRAL_SERIAL_RHI:-0} != 0 ]]; then
        echo 'CSS_ASTRAL_SERIAL_RHI must be 0 or 1.' >&2
        exit 2
    fi
fi
# Source symlinks remain read-only even though the experiment is writable.
bwrap --unshare-net --ro-bind / / --dev-bind /dev /dev --proc /proc \
    --bind "$stage" "$stage" --bind "$stage/scratch" /tmp \
    --setenv TMPDIR "$stage/scratch" --setenv XDG_CACHE_HOME "$stage/cache" \
    --setenv UE_LocalDataCachePath "$stage/ddc" \
    --setenv LD_LIBRARY_PATH "$libraries" \
    "${command[@]}" 9>&- >"$stage/$name-console.log" 2>&1
