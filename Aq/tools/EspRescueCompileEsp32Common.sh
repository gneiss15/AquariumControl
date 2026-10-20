# EspRescueCompileEsp32Common.sh, Version: 1.11
# Common ESP32 rescue build implementation sourced by project-specific scripts.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "EspRescueCompileEsp32Common.sh must be sourced by a project EspRescueCompile.sh." >&2
  exit 2
fi

: "${ProjectDir:?ProjectDir not set}"
: "${PrjsDir:?PrjsDir not set}"
: "${ProjectName:?ProjectName not set}"
: "${FQBN:?FQBN not set}"
: "${ConfigDefinition:?ConfigDefinition not set}"

COMPILE_VERBOSE="${COMPILE_VERBOSE:-}"
declare -p ArduinoLibraries >/dev/null 2>&1 || ArduinoLibraries=()
declare -p ExtraBuildProperties >/dev/null 2>&1 || ExtraBuildProperties=()

PrjsPrivateDir="${PRJS_PRIVATE_DIR:-$(dirname -- "$(dirname -- "$PrjsDir")")/PrjsPrivate}"
MountDir="$(dirname -- "$(dirname -- "$PrjsDir")")"
CLI="${ARDUINO_CLI_WRAPPER:-${ARDUINO2CLI_START:-$MountDir/Start_Arduino2cli.sh}}"
RescueDir="$ProjectDir/rescue"
BuildDir="$RescueDir/build"
CompileDir="$BuildDir/esp32"
SketchDir="$BuildDir/sketch/EspRescueUpdate"
BinDir="$RescueDir/bin"
LogFile="$BuildDir/Compile.log"
RescueSource="${RescueSource:-$PrjsDir/EspRescueUpdate/Sw/EspRescueUpdate.ino}"
RescueConfigHelper="${RescueConfigHelper:-$PrjsDir/EspRescueUpdate/EspRescueConfig.py}"
RescueHardwareDefinition="${RescueHardwareDefinition:-$PrjsDir/EspRescueUpdate/Sw/EspRescueHardware.h}"
PartitionDefinition="${PartitionDefinition:-}"
RescuePartitionSize="${RescuePartitionSize:-}"
UpdateCommonDir="${UpdateCommonDir:-$PrjsDir/Libs/Common/EspUpdate}"
DeviceTypeHelper="${DeviceTypeHelper:-$PrjsDir/tools/DeviceTypeArtifacts.py}"
DeviceTypeFile="${DeviceTypeFile:-$ProjectDir/DeviceType.txt}"
SecretsFile="${PRJS_SECRETS_FILE:-${SecretsFile:-$PrjsPrivateDir/Secrets.local.json}}"
BuildMode=incremental
BuildNoIdArtifacts=false

ShowUsage()
{
  cat <<EOF2
Usage: $(basename -- "$0") [--build | --clean | --full-clean] [--NoId]

Without an option, an incremental rescue build is performed.

--NoId additionally creates a one-time legacy-format rescue application.
EOF2
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --build) BuildMode=clean-build ;;
    --clean) rm -rf "$BuildDir"; echo "Rescue build cache removed: $BuildDir"; exit 0 ;;
    --full-clean) rm -rf "$BuildDir" "$BinDir"; echo "Rescue build/output removed"; exit 0 ;;
    --NoId) BuildNoIdArtifacts=true ;;
    --help|-h) ShowUsage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; ShowUsage >&2; exit 2 ;;
  esac
  shift
done

[ "$BuildMode" != clean-build ] || rm -rf "$BuildDir"
mkdir -p "$CompileDir" "$SketchDir" "$BinDir"
: >"$LogFile"

Fail()
{
  echo "Error: $1" >&2
  [ ! -f "$LogFile" ] || tail -n 80 "$LogFile" >&2 || true
  exit 1
}

[ -x "$CLI" ] || Fail "Arduino CLI wrapper not executable: $CLI"
[ -f "$RescueSource" ] || Fail "Rescue source not found: $RescueSource"
[ -f "$RescueConfigHelper" ] || Fail "Rescue config helper not found: $RescueConfigHelper"
[ -f "$RescueHardwareDefinition" ] || Fail "Rescue hardware definition not found: $RescueHardwareDefinition"
if [ -n "$PartitionDefinition" ]; then
  [ -f "$PartitionDefinition" ] || Fail "Partition definition not found: $PartitionDefinition"
fi
[ -f "$ConfigDefinition" ] || Fail "Config definition not found: $ConfigDefinition"
[ -f "$UpdateCommonDir/EspWebUpdate.h" ] || Fail "EspWebUpdate.h not found"
[ -f "$UpdateCommonDir/EspWebUpdate.cpp" ] || Fail "EspWebUpdate.cpp not found"
[ -f "$DeviceTypeHelper" ] || Fail "DeviceType helper not found: $DeviceTypeHelper"
[ -f "$DeviceTypeFile" ] || Fail "DeviceType file not found: $DeviceTypeFile"

python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$RescueSource" "$SketchDir/EspRescueUpdate.ino"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$RescueHardwareDefinition" "$SketchDir/EspRescueHardware.h"
if [ -n "$PartitionDefinition" ]; then
  python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$PartitionDefinition" "$SketchDir/partitions.csv"
fi
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$UpdateCommonDir/EspWebUpdate.h" "$SketchDir/EspWebUpdate.h"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$UpdateCommonDir/EspWebUpdate.cpp" "$SketchDir/EspWebUpdate.cpp"
python3 "$DeviceTypeHelper" prepare \
  "$DeviceTypeFile" \
  "$SketchDir/DeviceType.inc.h" \
  "$BuildDir/DeviceType.txt"
python3 "$RescueConfigHelper" "$ConfigDefinition" "$SecretsFile" "$ProjectName" \
  "$SketchDir/EspRescueProject.h"

CompileArgs=( compile --no-color --fqbn "$FQBN" --build-path "$CompileDir" )
[ -z "$COMPILE_VERBOSE" ] || CompileArgs+=( "$COMPILE_VERBOSE" )
for Property in "${ExtraBuildProperties[@]}"; do
  CompileArgs+=( --build-property "$Property" )
done

# IMPORTANT: keep project libraries exact. Never collapse them to their parent
# directory via --libraries; that reintroduces duplicate library discovery.
for Library in "${ArduinoLibraries[@]}"; do
  [ -d "$Library" ] || Fail "Arduino library not found: $Library"
  CompileArgs+=( --library "$Library" )
done

if ! "$CLI" "${CompileArgs[@]}" "$SketchDir" >>"$LogFile" 2>&1; then
  Fail "ESP32 rescue build failed"
fi

Firmware="$CompileDir/EspRescueUpdate.ino.bin"
TypedFirmware="$CompileDir/EspRescueUpdate.ino.bin.device"
OutputFirmware="$BinDir/$ProjectName.Rescue.bin"
OutputNoIdFirmware="$BinDir/$ProjectName.NoId.Rescue.bin"
[ -f "$Firmware" ] || Fail "Rescue application binary not generated: $Firmware"

FirmwareSize="$(stat -c '%s' "$Firmware")"
if [ -n "$RescuePartitionSize" ]; then
  RescuePartitionSizeValue=$(( RescuePartitionSize ))
  if (( FirmwareSize > RescuePartitionSizeValue )); then
    Fail "ESP32 rescue application is too large: $FirmwareSize bytes, partition has $RescuePartitionSizeValue bytes"
  fi
  echo "ESP32 rescue raw size: $FirmwareSize / $RescuePartitionSizeValue bytes"
fi

if ! python3 "$DeviceTypeHelper" wrap \
  "$DeviceTypeFile" firmware "$Firmware" "$TypedFirmware" >>"$LogFile" 2>&1
then
  Fail "Could not create typed ESP32 rescue application"
fi

rm -f "$OutputFirmware" "$OutputNoIdFirmware"
install -m 0644 "$TypedFirmware" "$OutputFirmware"
if "$BuildNoIdArtifacts"; then
  install -m 0644 "$Firmware" "$OutputNoIdFirmware"
fi
rm -f "$TypedFirmware"

echo "ESP32 rescue application: $OutputFirmware"
if "$BuildNoIdArtifacts"; then
  echo "One-time NoId rescue application: $OutputNoIdFirmware"
fi
echo "These outputs are application-only; they do not contain or replace bootloader/partition table."
