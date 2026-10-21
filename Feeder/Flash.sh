#!/usr/bin/env bash
# Flash.sh, Version: 1.01

set -euo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"

ProjectName="Feeder"
BinSubDir="wifi_kit_8_Arduino2"
Chip="esp8266"
FlashOffset="0x000000"
LittleFsOffset="0x200000"

# shellcheck source=../tools/EspFlashCommon.sh
source "$PrjsDir/tools/EspFlashCommon.sh"
