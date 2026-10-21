#!/usr/bin/env bash
# Flash.sh, Version: 1.01

set -euo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"

# shellcheck source=EspBoardConfig.sh
source "$ProjectDir/EspBoardConfig.sh"

ProjectName="FeederSimple"
BinSubDir="${BoardId}_Arduino2"
Chip="esp8266"
FlashOffset="0x000000"
LittleFsOffset="${LittleFsOffset:?LittleFsOffset is not configured in $ProjectDir/EspBoardConfig.sh}"

# shellcheck source=../tools/EspFlashCommon.sh
source "$PrjsDir/tools/EspFlashCommon.sh"
