#!/usr/bin/env bash
# Build.sh, Version: 1.00
set -euo pipefail
SourceDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(cd -- "$SourceDir/../../.." && pwd -P)"
case "${1:-}" in
  TmrSwA8T|TmrSwShelly) Project="$1" ;;
  *) echo "Usage: bash Build.sh TmrSwA8T|TmrSwShelly" >&2; exit 2 ;;
esac
[ "$#" -eq 1 ] || exit 2
ProjectDir="$PrjsDir/$Project"
source "$ProjectDir/EspBoardConfig.sh"
BuildDir="$ProjectDir/generated/build/mini-main"
PrivateDir="${PRJS_PRIVATE_DIR:-$PrjsDir/../../PrjsPrivate}"
mkdir -p "$BuildDir"
sed "s|^CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=.*|CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"$PartitionDefinition\"|" \
  "$ProjectDir/Sw/Rescue/sdkconfig.defaults" >"$BuildDir/sdkconfig.defaults"
source "${ESP_IDF_ENABLE_SCRIPT:-$PrjsDir/../../Portables/EnableEspIdf.sh}"
export PATH="$IDF_TOOLS_PATH/tools/riscv32-esp-elf/esp-14.2.0_20241119/riscv32-esp-elf/bin:$IDF_TOOLS_PATH/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin:$PATH"
idf.py -C "$SourceDir" -B "$BuildDir" -D "IDF_TARGET=$BoardId" \
  -D "SDKCONFIG=$BuildDir/sdkconfig" -D "SDKCONFIG_DEFAULTS=$BuildDir/sdkconfig.defaults" \
  -D "PROJECT_VER=MiniMain-1.00" -D "MINI_PROJECT=$Project" \
  -D "MINI_HARDWARE_DIR=$ProjectDir/Sw" build
Firmware="$BuildDir/$Project.bin"
(( $(stat -c %s "$Firmware") <= MainPartitionSize ))
python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" wrap "$ProjectDir/DeviceType.txt" firmware "$Firmware" "$BuildDir/MiniMain.unsigned"
python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" sign "$BuildDir/MiniMain.unsigned" "$PrivateDir/private.key" "$BuildDir/$Project.MiniMain.bin_signed"
python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" verify "$BuildDir/$Project.MiniMain.bin_signed" "$PrivateDir/public.key"
printf 'Signed LED MiniMain: %s\n' "$BuildDir/$Project.MiniMain.bin_signed"
