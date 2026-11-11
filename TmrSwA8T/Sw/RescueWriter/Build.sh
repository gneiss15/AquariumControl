#!/usr/bin/env bash
# Build.sh, Version: 1.01
set -euo pipefail
SourceDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
ProjectDir="$(cd -- "$SourceDir/../.." && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
source "$ProjectDir/EspBoardConfig.sh"
BuildDir="$ProjectDir/generated/build/rescue-writer"
PrivateDir="${PRJS_PRIVATE_DIR:-$PrjsDir/../../PrjsPrivate}"
Artifacts="$PrjsDir/tools/DeviceTypeArtifacts.py"
mkdir -p "$BuildDir"
sed "s|^CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=.*|CONFIG_PARTITION_TABLE_CUSTOM_FILENAME=\"$PartitionDefinition\"|" \
  "$ProjectDir/Sw/Rescue/sdkconfig.defaults" >"$BuildDir/sdkconfig.defaults"
python3 "$Artifacts" prepare "$ProjectDir/DeviceType.txt" "$BuildDir/DeviceType.inc.h" "$BuildDir/DeviceType.txt"
python3 "$Artifacts" public-key-header "$PrivateDir/public.key" "$BuildDir/EspUpdatePublicKey.h"
python3 "$PrjsDir/Common/EspRescue/EspRescueConfig.py" "$ProjectDir/Sw/TmrSwConfig.inc.h" \
  "${PRJS_SECRETS_FILE:-$PrivateDir/Secrets.local.json}" NousRescueWriter 1.01 "$BuildDir/EspRescueProject.h"
chmod 600 "$BuildDir/EspRescueProject.h"
source "${ESP_IDF_ENABLE_SCRIPT:-$PrjsDir/../../Portables/EnableEspIdf.sh}"
export PATH="${ESP_IDF_COMPILER_BIN:-$IDF_TOOLS_PATH/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin}:$PATH"
idf.py -C "$SourceDir" -B "$BuildDir" -D IDF_TARGET=esp32 -D PROJECT_VER=1.01 \
  -D "SDKCONFIG=$BuildDir/sdkconfig" -D "SDKCONFIG_DEFAULTS=$BuildDir/sdkconfig.defaults" \
  -D "WRITER_GENERATED=$BuildDir" build
Firmware="$BuildDir/NousRescueWriter.bin"
(( $(stat -c %s "$Firmware") <= 0x200000 ))
python3 "$Artifacts" wrap "$ProjectDir/DeviceType.txt" firmware "$Firmware" "$BuildDir/NousRescueWriter.unsigned"
python3 "$Artifacts" sign "$BuildDir/NousRescueWriter.unsigned" "$PrivateDir/private.key" "$BuildDir/NousRescueWriter.bin_signed"
python3 "$Artifacts" verify "$BuildDir/NousRescueWriter.bin_signed" "$PrivateDir/public.key"
printf 'Signed Nous Rescue Writer: %s\n' "$BuildDir/NousRescueWriter.bin_signed"
