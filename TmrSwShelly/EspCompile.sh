#!/usr/bin/env bash
# EspCompile.sh, Version: 1.07
set -Eeuo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
source "$PrjsDir/tools/_PruneDeviceFsArtifacts.sh"
source "$ProjectDir/EspBoardConfig.sh"
BuildRoot="$ProjectDir/generated/build/main"
MainBuildDir="$BuildRoot/esp32/idf"
MainInputDir="$BuildRoot/sketch/TmrSwShelly"
IdfProjectDir="$BuildRoot/IdfProject"
BinDir="$ProjectDir/generated/bin/$BinSubDir"
InstancesDir="$ProjectDir/generated/Instances"
BuildMode="incremental"
ShowStatus=false
SelectedDevice=""
EnableLittleFs="${PRJS_ENABLE_LITTLEFS:-true}"
[[ "$EnableLittleFs" == true || "$EnableLittleFs" == false ]] || { echo "PRJS_ENABLE_LITTLEFS must be true or false." >&2; exit 2; }

ShowUsage()
{
  cat <<'HELP'
Usage: EspCompile.sh [--build | --clean | --full-clean] [--status] [--Device NAME]

Without a build-mode option, an incremental build is performed.
Without --Device, firmware is built once and LittleFS for every DevState device.
  --build       move compiler/build cache to Trash, then build
  --clean       move compiler/build cache to Trash and exit; keep artifacts
  --full-clean  additionally move Main/LittleFS artifacts to Trash; keep data
  --status      show build phases and a heartbeat during firmware compilation
  --Device NAME build LittleFS for this device only
  NAME          equivalent to --Device NAME (legacy invocation)
  --help, -h    show this help

PRJS_ENABLE_LITTLEFS=false builds firmware only (no filesystem images).
HELP
}

SetBuildMode()
{
  if [ "$BuildMode" != incremental ] && [ "$BuildMode" != "$1" ]; then
    echo "Only one of --build, --clean and --full-clean may be specified." >&2
    exit 2
  fi
  BuildMode="$1"
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --build) SetBuildMode clean-build ;;
    --clean) SetBuildMode clean-only ;;
    --full-clean) SetBuildMode full-clean-only ;;
    --status) ShowStatus=true ;;
    --Device)
      if [ "$#" -lt 2 ] || [ -n "$SelectedDevice" ]; then ShowUsage >&2; exit 2; fi
      SelectedDevice="$2"
      shift
      ;;
    --help|-h) ShowUsage; exit 0 ;;
    -*) echo "Unknown option: $1" >&2; ShowUsage >&2; exit 2 ;;
    *)
      [ -z "$SelectedDevice" ] || { ShowUsage >&2; exit 2; }
      SelectedDevice="$1"
      ;;
  esac
  shift
done

DeviceNames=()
if [ -n "$SelectedDevice" ]; then
  [[ "$SelectedDevice" =~ ^[A-Za-z][A-Za-z0-9_-]*$ ]] || { echo "Invalid device name." >&2; exit 2; }
  [ -d "$ProjectDir/DevState/$SelectedDevice" ] || { echo "Device state not found: $SelectedDevice" >&2; exit 2; }
  DeviceNames=( "$SelectedDevice" )
else
  mapfile -t DeviceNames < <(find "$ProjectDir/DevState" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort)
fi
[ "${#DeviceNames[@]}" -gt 0 ] || { echo "No DevState devices found." >&2; exit 1; }
if "$EnableLittleFs"; then
  python3 "$PrjsDir/tools/DeviceFsArtifactName.py" --check-directory "$ProjectDir/DevState"
fi


case "$BuildMode" in
  clean-build|clean-only|full-clean-only) rm -rf -- "$BuildRoot" ;;
esac
if [ "$BuildMode" = full-clean-only ]; then
  rm -rf -- "$BinDir/TmrSwShelly.ino.bin_signed"
  for DeviceName in "${DeviceNames[@]}"; do
    rm -rf -- "$InstancesDir/$DeviceName/LittleFS.NoId.bin" \
      "$BinDir/$(FsArtifactName "$ProjectDir/DevState/$DeviceName/Config.json").ino.littlefs_signed" \
      "$InstancesDir/$DeviceName/main-build-verification.json"
  done
fi

case "$BuildMode" in
  clean-only|full-clean-only) exit 0 ;;
esac

Status()
{
  if "$ShowStatus"; then printf 'Status: %s\n' "$1"; fi
}

HeartbeatPid=""
StopHeartbeat()
{
  if [ -n "$HeartbeatPid" ]; then
    kill "$HeartbeatPid" 2>/dev/null || true
    wait "$HeartbeatPid" 2>/dev/null || true
    HeartbeatPid=""
  fi
}
trap StopHeartbeat EXIT

RunFirmwareBuild()
{
  local Result=0
  if "$ShowStatus"; then
    (
      SleepPid=""
      trap 'if [ -n "$SleepPid" ]; then kill "$SleepPid" 2>/dev/null || true; fi; exit 0' TERM INT
      while true; do
        sleep 10 &
        SleepPid=$!
        wait "$SleepPid" || exit 0
        SleepPid=""
        printf 'Status: firmware compilation still running\n'
      done
    ) &
    HeartbeatPid=$!
  fi
  idf.py -C "$IdfProjectDir" -B "$MainBuildDir" -D "SDKCONFIG=$MainBuildDir/sdkconfig" \
    -D "PRJS_IDF_DEPENDENCIES=$BuildRoot/MainDependencies" \
    -D "PRJS_GENERATED_INCLUDE=$MainInputDir" \
    -D "PRJS_VERSION_FILE=$BuildRoot/ProjectVersion.cmake" build || Result=$?
  StopHeartbeat
  return "$Result"
}

PortableDir="${ARDUINO_PORTABLE_DIR:-$PrjsDir/../../Portables/Arduino2/portable}"
CoreDir="${ARDUINO_CORE_DIR:-$PortableDir/.arduino15/packages/esp32/hardware/esp32/3.3.11}"
PrivateDir="${PRJS_PRIVATE_DIR:-$PrjsDir/../../PrjsPrivate}"
Status "Preparing Shelly-specific Arduino/ESP-IDF sources"
mkdir -p "$MainInputDir" "$BinDir"
python3 "$ProjectDir/PrepareMain.py" "$CoreDir" "$PortableDir/Sketchbook/libraries"
DeviceTypeFile="${TMR_SW_DEVICE_TYPE_FILE:-$ProjectDir/DeviceType.txt}"
if [[ -n "${TMR_SW_DEVICE_TYPE+x}" ]]; then
  [[ "$TMR_SW_DEVICE_TYPE" =~ ^[A-Za-z][A-Za-z0-9]{0,30}$ ]] || { echo 'Invalid TMR_SW_DEVICE_TYPE.' >&2; exit 2; }
  python3 - "$PrjsDir/tools" "$TMR_SW_DEVICE_TYPE" "$MainInputDir/DeviceType.input.txt" <<'PYTYPE'
import sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from SyncGeneratedFiles import write_text_if_changed
write_text_if_changed(Path(sys.argv[3]),sys.argv[2]+'\n')
PYTYPE
  DeviceTypeFile="$MainInputDir/DeviceType.input.txt"
fi
python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" prepare "$DeviceTypeFile" "$MainInputDir/DeviceType.inc.h" "$MainInputDir/DeviceType.txt"
python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" public-key-header "$PrivateDir/public.key" "$MainInputDir/EspUpdatePublicKey.h"
python3 "$PrjsDir/tools/UpdateVersionFiles.py" "$ProjectDir"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$ProjectDir/generated/ProjectVersion.cmake" "$BuildRoot/ProjectVersion.cmake"
python3 "$PrjsDir/tools/PrepareIdfProject.py" "$ProjectDir" "$IdfProjectDir"
for DeviceName in "${DeviceNames[@]}"; do
  Status "Preparing LittleFS data for $DeviceName"
  DeviceStateDir="$ProjectDir/DevState/$DeviceName"
  ConfigFile="$DeviceStateDir/Config.json"
  if [[ ! -f "$ConfigFile" ]]; then
    [[ "$DeviceName" == Shelly1 ]] || { echo "Device Config.json required" >&2; exit 1; }
    # Bootstrap only the new service instance from external private defaults.
    ConfigInput="$InstancesDir/$DeviceName/config-input"
    (cd "$ProjectDir"; bash "$PrjsDir/tools/UpdateJsonFiles.sh" "$ProjectDir/Sw/TmrSwConfig.inc.h" Config.json ConfigEntries.json "$ConfigInput" aligned)
    ConfigFile="$ConfigInput/Config.json"
  fi
  PRJS_DEVICE_NAME="$DeviceName" PRJS_DEVICE_CONFIG_FILE="$ConfigFile" PRJS_BUILD_ROOT="$BuildRoot" bash "$ProjectDir/PreBuild.sh"
  python3 "$PrjsDir/tools/CreateReadableLittleFS.py" "$InstancesDir/$DeviceName/LittleFS" "$InstancesDir/$DeviceName/LittleFS-readable"
done
# shellcheck source=/dev/null
source "${ESP_IDF_ENABLE_SCRIPT:-$PrjsDir/../../Portables/EnableEspIdf.sh}"
export PATH="${ESP_IDF_COMPILER_BIN:-$IDF_TOOLS_PATH/tools/riscv32-esp-elf/esp-14.2.0_20241119/riscv32-esp-elf/bin}:$PATH"
cd "$ProjectDir"
Status "Compiling shared Shelly firmware"
RunFirmwareBuild
Mklittlefs="${MKLITTLEFS:-$PortableDir/My/tools/mklittlefs/4.0.2-db0513a/mklittlefs}"
for DeviceName in "${DeviceNames[@]}"; do
  Status "Creating and verifying artifacts for $DeviceName"
  if "$EnableLittleFs"; then "$Mklittlefs" -c "$InstancesDir/$DeviceName/LittleFS" -b "$LittleFsBlockSize" -p "$LittleFsPageSize" -s "$LittleFsSize" "$InstancesDir/$DeviceName/LittleFS.NoId.bin" > "$InstancesDir/$DeviceName/littlefs-build.log"; fi
  for Kind in firmware filesystem; do
    if [[ "$Kind" == filesystem && "$EnableLittleFs" == false ]]; then continue; fi
    if [[ "$Kind" == firmware ]]; then
      Input="$MainBuildDir/TmrSwShelly.bin"
      Output="$BinDir/TmrSwShelly.ino.bin_signed"
    else
      Input="$InstancesDir/$DeviceName/LittleFS.NoId.bin"
      Output="$BinDir/$(FsArtifactName "$ProjectDir/DevState/$DeviceName/Config.json").ino.littlefs_signed"
    fi
    python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" wrap "$DeviceTypeFile" "$Kind" "$Input" "$Output.unsigned"
    python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" sign "$Output.unsigned" "$PrivateDir/private.key" "$Output"
    python3 "$PrjsDir/tools/DeviceTypeArtifacts.py" verify "$Output" "$PrivateDir/public.key"
    rm -rf -- "$Output.unsigned"
  done
  VerifyArgs=()
  if ! "$EnableLittleFs"; then VerifyArgs=( --firmware-only ); fi
  python3 "$ProjectDir/VerifyMain.py" "$DeviceName" "${VerifyArgs[@]}"
done

if "$EnableLittleFs"; then
  source "$PrjsDir/tools/_PruneDeviceFsArtifacts.sh"
  PruneDeviceFsArtifacts "$BinDir" "$ProjectDir/DevState"
fi

VersionArgs=()
if "$EnableLittleFs"; then VersionArgs=( --filesystem --devices "${#DeviceNames[@]}" ); fi
python3 "$PrjsDir/tools/PrintBuiltVersions.py" "$ProjectDir" "${VersionArgs[@]}"
