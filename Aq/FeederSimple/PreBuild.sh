#!/usr/bin/env bash
# PreBuild.sh, Version: 1.14

set -Eeuo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
# shellcheck source=../tools/_TrashPaths.sh
source "$PrjsDir/tools/_TrashPaths.sh"
DeviceName="${PRJS_DEVICE_NAME:?PRJS_DEVICE_NAME not set}"
DeviceStateDir="${PRJS_DEVICE_STATE_DIR:-$ProjectDir/DevState/$DeviceName}"
InstancesDir="${PRJS_INSTANCES_DIR:-$ProjectDir/generated/Instances}"
BuildRoot="${PRJS_BUILD_ROOT:-$ProjectDir/generated/build/main}"
DataDir="${PRJS_DATA_DIR:-$InstancesDir/$DeviceName/LittleFS}"
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
CommonWebSrcDir="$PrjsDir/Libs/Common/Feeder/WebSrc"
BuildWebSrcDir="$BuildRoot/WebSrc"
PrepareWebSrc="$PrjsDir/Libs/Common/EspUpdate/PrepareWebSrc.py"
UpdateWebFiles="$PrjsDir/tools/UpdateWebFiles.sh"
ValidateJsonFiles="$PrjsDir/tools/ValidateJsonFiles.py"

[ -d "$CommonWebSrcDir" ] || { echo "Common Feeder web source directory not found: $CommonWebSrcDir" >&2; exit 1; }
[ -d "$DeviceStateDir" ] || { echo "Device state directory not found: $DeviceStateDir" >&2; exit 1; }
[ -f "$DeviceStateDir/Config.json" ] || { echo "Device Config.json not found: $DeviceStateDir/Config.json" >&2; exit 1; }
python3 "$ValidateJsonFiles" "$CommonWebSrcDir" "$DeviceStateDir"
python3 "$PrepareWebSrc" "$CommonWebSrcDir" "$BuildWebSrcDir"

mkdir -p -- "$DataDir"

pushd "$ProjectDir" >/dev/null
PRJS_WEB_SRC_DIR="$BuildWebSrcDir" PRJS_CONFIG_DIR="" PRJS_DATA_DIR="$DataDir" bash "$UpdateWebFiles"
for SourceFile in "$DeviceStateDir"/*.json; do
  [ -f "$SourceFile" ] || continue
  [ "$(basename -- "$SourceFile")" = "Config.json" ] && continue
  install -m 0644 "$SourceFile" "$DataDir/$(basename -- "$SourceFile")"
done
TrashPaths "$DataDir/ServoSteps.json"
PRJS_DATA_DIR="$DataDir" PRJS_DEVICE_CONFIG_FILE="$DeviceStateDir/Config.json" \
  bash "$PrjsDir/tools/UpdateJsonFiles.sh" "$ProjectDir/Sw/FeederConfig.inc.h" Config.json ConfigEntries.json "$DataDir" aligned
PRJS_DATA_DIR="$DataDir" PRJS_WEB_SRC_DIR="$CommonWebSrcDir" python3 "$PrjsDir/tools/UpdateVersionFiles.py" "$ProjectDir"
popd >/dev/null

# Publish only final differences; intermediate formatting must not touch live outputs.
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" tree "$DataDir" "$FinalDataDir"
