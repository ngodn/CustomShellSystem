#!/usr/bin/env bash
# Runs inside the Wine container: cook /project for Windows with the UE 5.6.1 Windows editor.
set -euo pipefail
seed_prefix="$WINEPREFIX"
export WINEPREFIX=/out/prefix
export WINEDEBUG=-all
export TMPDIR=/out/tmp
mkdir -p "$TMPDIR" /out/ddc /out/user
[[ ! -e /out/cook-ue.log && ! -e "$WINEPREFIX" ]] || { echo 'Output directory was used before; start a new one.' >&2; exit 1; }
# Container root maps to the host user in the workspace's rootless daemon.
cp -a --no-preserve=ownership "$seed_prefix" "$WINEPREFIX"
cd /project
set +e
env 'UE-LocalDataCachePath=Z:\out\ddc' wine64 \
    /ue/Engine/Binaries/Win64/UnrealEditor-Cmd.exe \
    'Z:\project\CineAssets.uproject' -run=cook -targetplatform=Windows -cookall \
    -unattended -nop4 -NoSound -stdout -FullStdOutLogOutput -NullRHI \
    -ddc=InstalledNoZenLocalFallback -numcores=6 \
    '-UserDir=Z:\out\user' '-abslog=Z:\out\cook-ue.log'
result=$?
set -e
wineserver --wait
exit "$result"
