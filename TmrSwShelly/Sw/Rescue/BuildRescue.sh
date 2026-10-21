#!/usr/bin/env bash
# BuildRescue.sh, Version: 1.12
set -Eeuo pipefail
export GENERATED_TOOLS="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../../tools" && pwd -P)"
[[ $# == 0 ]] || exit 2
RescueDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(cd -- "$RescueDir/../../.." && pwd -P)"
Generated="$PrjsDir/TmrSwShelly/generated/build/rescue"
RescueVersion="$(sed -n 's/^Rescue=//p' "$PrjsDir/TmrSwShelly/Versions.txt" | head -n1)"
[[ -n "$RescueVersion" ]] || { echo 'Rescue version is missing from TmrSwShelly/Versions.txt.' >&2; exit 1; }
PrivateDir="${PRJS_PRIVATE_DIR:-$PrjsDir/../../PrjsPrivate}"
BuildDir="$Generated/esp32/idf"
mkdir -p "$Generated" "$BuildDir"
python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" prepare "$RescueDir/DeviceType.txt" "$Generated/DeviceType.inc.h" "$Generated/DeviceType.txt"
python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" public-key-header "$PrivateDir/public.key" "$Generated/EspUpdatePublicKey.h"
python3 "$PrjsDir/Libs/Common/EspRescue/EspRescueConfig.py" "$RescueDir/Config.inc.h" "${PRJS_SECRETS_FILE:-$PrivateDir/Secrets.local.json}" TmrSwShelly "$RescueVersion" "$Generated/EspRescueProject.h"
chmod 600 "$Generated/EspRescueProject.h"
python3 "$RescueDir/PrepareSources.py" "$Generated"
source "${ESP_IDF_ENABLE_SCRIPT:-$PrjsDir/../../Portables/EnableEspIdf.sh}"
python3 - "$PrjsDir/tools" "$IDF_PATH/components/partition_table/gen_esp32part.py" "$PrjsDir/TmrSwShelly/partitions.csv" "$Generated/NewTable.bin" <<'PYTABLE'
from pathlib import Path
import sys
sys.path.insert(0,sys.argv[1])
from SyncGeneratedFiles import generate_output_if_changed
generate_output_if_changed([sys.executable,sys.argv[2],'--offset','0x10000',sys.argv[3],sys.argv[4]],padding=4096)
PYTABLE
python3 "$RescueDir/VerifyActivation.py"
cd "$RescueDir"
export PATH="${ESP_IDF_COMPILER_BIN:-$IDF_TOOLS_PATH/tools/riscv32-esp-elf/esp-14.2.0_20241119/riscv32-esp-elf/bin}:$PATH"
idf.py -B "$BuildDir" -D "SDKCONFIG=$BuildDir/sdkconfig" -D IDF_TARGET=esp32c3 build
install -m 0644 "$BuildDir/ShellyRescue.bin" "$Generated/Rescue.bin"
python3 "$RescueDir/VerifyBuild.py"

# Produce the ordinary signed Main-to-Rescue update artifact as well as the payload.
python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" wrap "$RescueDir/DeviceType.txt" rescue "$Generated/Rescue.bin" "$Generated/Rescue.unsigned"
python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" sign "$Generated/Rescue.unsigned" "$PrivateDir/private.key" "$Generated/TmrSwShelly.rescue_signed"
python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" verify "$Generated/TmrSwShelly.rescue_signed" "$PrivateDir/public.key"
source "$PrjsDir/tools/_TrashPaths.sh"
rm -rf -- "$Generated/Rescue.unsigned"
BinDir="$PrjsDir/TmrSwShelly/generated/bin/esp32c3_Arduino2"
mkdir -p "$BinDir"
install -m 0644 "$Generated/TmrSwShelly.rescue_signed" "$BinDir/TmrSwShelly.Rescue.rescue_signed"
rm -rf -- "$BinDir/TmrSwShelly.Rescue.ino.bin_signed"
