#!/usr/bin/env bash
# CopyToRasPi.sh, Version: 1.00
set -euo pipefail
ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
RemoteHost="${RASPI_HOST:-pi@192.168.99.51}"
RemoteRoot="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
RemoteDir="$RemoteRoot/RoomTemperature"
printf -v RemoteCommand 'mkdir -p -- %q' "$RemoteDir/data"
ssh "$RemoteHost" "$RemoteCommand"
cd "$ProjectDir"
rsync -av --protect-args RoomTemperature.py Versions.txt RoomTemperature.service Install.sh README.md "$RemoteHost:$RemoteDir/"
# Configuration is initialized once, never overwritten by source updates.
rsync -av --protect-args --ignore-existing data/Config.json "$RemoteHost:$RemoteDir/data/"
echo 'Sources copied; existing remote configuration preserved. No install/start performed.'
