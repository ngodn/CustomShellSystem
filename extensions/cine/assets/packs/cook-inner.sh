#!/usr/bin/env bash
set -euo pipefail
seed_prefix="$WINEPREFIX"
export WINEPREFIX=/out/prefix WINEDEBUG=-all TMPDIR=/out/tmp
mkdir -p "$TMPDIR" /out/ddc /out/user
[[ ! -e /out/cook-ue.log && ! -e "$WINEPREFIX" ]] || exit 1
cp -a --no-preserve=ownership "$seed_prefix" "$WINEPREFIX"
cd /project
set +e
env 'UE-LocalDataCachePath=Z:\out\ddc' wine64 \
    /ue/Engine/Binaries/Win64/UnrealEditor-Cmd.exe \
    'Z:\project\CinePack.uproject' -run=cook -targetplatform=Windows -cookall \
    -unattended -nop4 -NoSound -stdout -FullStdOutLogOutput -NullRHI \
    -ddc=InstalledNoZenLocalFallback -numcores=6 \
    '-UserDir=Z:\out\user' '-abslog=Z:\out\cook-ue.log'
result=$?
set -e
wineserver --wait
exit "$result"
