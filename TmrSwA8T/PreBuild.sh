#!/usr/bin/env bash
# PreBuild.sh, Version: 1.05
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
BuildWebSrcDir="$BuildRoot/WebSrc"
WebSrcDir="$PrjsDir/Libs/Common/TmrSw/WebSrc"
PrepareWebSrc="$PrjsDir/Libs/Common/EspUpdate/PrepareWebSrc.py"
ValidateJsonFiles="$PrjsDir/tools/ValidateJsonFiles.py"

[ -d "$WebSrcDir" ] || { echo "Web source directory not found: $WebSrcDir" >&2; exit 1; }
[ -d "$DeviceStateDir" ] || { echo "Device state directory not found: $DeviceStateDir" >&2; exit 1; }
[ -f "$DeviceStateDir/Config.json" ] || { echo "Device Config.json not found: $DeviceStateDir/Config.json" >&2; exit 1; }
python3 "$ValidateJsonFiles" "$WebSrcDir" "$DeviceStateDir"
python3 "$PrepareWebSrc" "$WebSrcDir" "$BuildWebSrcDir" --platform esp32-rescue

mkdir -p -- "$DataDir"

pushd "$ProjectDir" >/dev/null
PRJS_WEB_SRC_DIR="$BuildWebSrcDir" PRJS_CONFIG_DIR="" PRJS_DATA_DIR="$DataDir" bash "$PrjsDir/tools/UpdateWebFiles.sh"
for SourceFile in "$DeviceStateDir"/*.json; do
  [ -f "$SourceFile" ] || continue
  [ "$(basename -- "$SourceFile")" = "Config.json" ] && continue
  install -m 0644 "$SourceFile" "$DataDir/$(basename -- "$SourceFile")"
done
PRJS_DATA_DIR="$DataDir" PRJS_DEVICE_CONFIG_FILE="$DeviceStateDir/Config.json" \
  bash "$PrjsDir/tools/UpdateJsonFiles.sh" "$ProjectDir/Sw/TmrSwConfig.inc.h" Config.json ConfigEntries.json "$DataDir" aligned
# Keep centrally synchronized data empty in the build image; retain channel assignments.
python3 - "$DataDir" <<'PYDUMMY'
from pathlib import Path
import json
import sys
root=Path(sys.argv[1])
for name in ("SwitchTimes.json", "Intervals.json"):
  path=root/name
  if path.is_file():path.write_text("[]\n")
path=root/"Channels.json"
data=json.loads(path.read_text())
data["Profiles"]=[]
path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n")
path=root/"SwitchTimes.sync.json"
if path.is_file():
  data=json.loads(path.read_text())
  data["TimeStamp"]="1970-01-01T00:00:00.000Z"
  data["Synced"]=False
  path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n")
PYDUMMY
PRJS_DATA_DIR="$DataDir" python3 "$PrjsDir/tools/UpdateVersionFiles.py" "$ProjectDir"
popd >/dev/null

# Publish only final differences; intermediate formatting must not touch live outputs.
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" tree "$DataDir" "$FinalDataDir"
