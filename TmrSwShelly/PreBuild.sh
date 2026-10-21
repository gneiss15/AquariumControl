#!/usr/bin/env bash
# PreBuild.sh, Version: 1.09
set -Eeuo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
# shellcheck source=../tools/_TrashPaths.sh
source "$PrjsDir/tools/_TrashPaths.sh"
DeviceName="${PRJS_DEVICE_NAME:?PRJS_DEVICE_NAME not set}"
DeviceStateDir="${PRJS_DEVICE_STATE_DIR:-$ProjectDir/DevState/$DeviceName}"
DeviceConfigFile="${PRJS_DEVICE_CONFIG_FILE:-$DeviceStateDir/Config.json}"
DataDir="${PRJS_DATA_DIR:-$ProjectDir/generated/Instances/$DeviceName/LittleFS}"
FinalDataDir="$DataDir"
mkdir -p -- "$(dirname -- "$FinalDataDir")"
DataDir="$(mktemp -d "${FinalDataDir}.stage.XXXXXX")"
StagingDataDir="$DataDir"
CleanupPreBuild()
{
  if declare -F RestoreDir >/dev/null; then RestoreDir; fi
  [ -z "$StagingDataDir" ] || rm -rf -- "$StagingDataDir"
}
trap CleanupPreBuild EXIT
BuildWebSrcDir="${PRJS_BUILD_ROOT:-$ProjectDir/generated/build/main}/WebSrc"
WebSrcDir="$PrjsDir/Libs/Common/TmrSw/WebSrc"
PrepareWebSrc="$PrjsDir/Libs/Common/EspUpdate/PrepareWebSrc.py"
ValidateJsonFiles="$PrjsDir/tools/ValidateJsonFiles.py"

[ -d "$WebSrcDir" ] || { echo "Web source directory not found: $WebSrcDir" >&2; exit 1; }
[ -d "$DeviceStateDir" ] || { echo "Device state directory not found: $DeviceStateDir" >&2; exit 1; }
[ -f "$DeviceConfigFile" ] || { echo "Device Config.json not found: $DeviceConfigFile" >&2; exit 1; }
python3 "$ValidateJsonFiles" "$WebSrcDir" "$DeviceStateDir"
python3 "$PrepareWebSrc" "$WebSrcDir" "$BuildWebSrcDir" --platform esp32-rescue --shelly-calibration-warning


mkdir -p -- "$DataDir"

pushd "$ProjectDir" >/dev/null
PRJS_WEB_SRC_DIR="$BuildWebSrcDir" PRJS_CONFIG_DIR="" PRJS_DATA_DIR="$DataDir" bash "$PrjsDir/tools/UpdateWebFiles.sh"
for SourceFile in "$DeviceStateDir"/*.json; do
  [ -f "$SourceFile" ] || continue
  [ "$(basename -- "$SourceFile")" = "Config.json" ] && continue
  install -m 0644 "$SourceFile" "$DataDir/$(basename -- "$SourceFile")"
done
PRJS_DATA_DIR="$DataDir" PRJS_DEVICE_CONFIG_FILE="$DeviceConfigFile" \
  bash "$PrjsDir/tools/UpdateJsonFiles.sh" "$ProjectDir/Sw/TmrSwConfig.inc.h" Config.json ConfigEntries.json "$DataDir" aligned
PRJS_DATA_DIR="$DataDir" python3 "$PrjsDir/tools/UpdateVersionFiles.py" "$ProjectDir"
popd >/dev/null

# Publish only final differences; intermediate formatting must not touch live outputs.
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" tree "$DataDir" "$FinalDataDir"
