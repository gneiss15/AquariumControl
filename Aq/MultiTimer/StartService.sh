#!/usr/bin/env bash
# StartService.sh, Version: 1.02

set -euo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
RunMarker=/run/MultiTimer.started
StartArgument=--ReStart

if [ ! -e "$RunMarker" ]; then
  StartArgument=--log-start
  touch "$RunMarker"
fi

cd "$ProjectDir"

exec "$ProjectDir/MultiTimer" "$StartArgument"
