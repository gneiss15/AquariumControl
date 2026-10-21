# _EspRescueCompileEsp32Common.sh, Version: 2.12
# Common ESP32 rescue V2 build implementation sourced by project-specific scripts.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "_EspRescueCompileEsp32Common.sh must be sourced by a project EspRescueCompile.sh." >&2
  exit 2
fi

: "${ProjectDir:?ProjectDir not set}"
: "${PrjsDir:?PrjsDir not set}"
# shellcheck source=../../../tools/_TrashPaths.sh
source "$PrjsDir/tools/_TrashPaths.sh"
: "${ProjectName:?ProjectName not set}"
: "${FQBN:?FQBN not set}"
: "${ConfigDefinition:?ConfigDefinition not set}"

COMPILE_VERBOSE="${COMPILE_VERBOSE:-}"
declare -p ArduinoLibraries >/dev/null 2>&1 || ArduinoLibraries=()
declare -p ExtraBuildProperties >/dev/null 2>&1 || ExtraBuildProperties=()

CommonDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
MountDir="$(dirname -- "$(dirname -- "$PrjsDir")")"
CLI="${ARDUINO_CLI_WRAPPER:-${ARDUINO2CLI_START:-$MountDir/Start_Arduino2cli.sh}}"
RescueDir="$ProjectDir/rescue"
BuildDir="${PRJS_RESCUE_BUILD_DIR:-$RescueDir/build}"
CompileDir="$BuildDir/esp32"
SketchDir="$BuildDir/sketch/EspRescueUpdate"
BinDir="${PRJS_RESCUE_BIN_DIR:-$RescueDir/bin}"
PreserveSharedBinArtifacts="${PRJS_PRESERVE_SHARED_BIN_ARTIFACTS:-false}"
LogFile="$BuildDir/Compile.log"
RescueSource="$CommonDir/EspRescueUpdate.ino"
RescueUpdaterHeader="$CommonDir/EspRescueUpdater.h"
RescueUpdaterSource="$CommonDir/EspRescueUpdater.cpp"
RescueConfigHelper="$CommonDir/EspRescueConfig.py"
RescueHardwareDefinition="${RescueHardwareDefinition:-$CommonDir/EspRescueHardware.h}"
PartitionDefinition="${PartitionDefinition:-}"
RescuePartitionSize="${RescuePartitionSize:-}"
DeviceTypeHelper="${DeviceTypeHelper:-$PrjsDir/tools/DeviceTypeArtifacts.py}"
DeviceTypeFile="${DeviceTypeFile:-$ProjectDir/DeviceType.txt}"
PrjsPrivateDir="${PRJS_PRIVATE_DIR:-$(dirname -- "$(dirname -- "$PrjsDir")")/PrjsPrivate}"
PrivateKey="${PrivateKey:-$PrjsPrivateDir/private.key}"
PublicKey="${PublicKey:-$PrjsPrivateDir/public.key}"
UpdateCommonDir="${UpdateCommonDir:-$PrjsDir/Libs/Common/EspUpdate}"
SecretsFile="${PRJS_SECRETS_FILE:-${SecretsFile:-$PrjsPrivateDir/Secrets.local.json}}"
VersionsFile="${VersionsFile:-$ProjectDir/Versions.txt}"
BuildMode=incremental
BuildNoIdArtifacts=false

CleanRescueOutputs()
{
  if "$PreserveSharedBinArtifacts"; then
    rm -rf -- "$BinDir/$ProjectName.Rescue.rescue_signed" "$BinDir/$ProjectName.NoId.Rescue.bin"
  else
    rm -rf -- "$BinDir"
  fi
}

ShowUsage()
{
  cat <<EOF2
Usage: $(basename -- "$0") [--build | --clean | --full-clean] [--NoId]

Without an option, an incremental rescue build is performed.

--NoId additionally creates a raw application binary without DeviceType header.
EOF2
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --build) BuildMode=clean-build ;;
    --clean) rm -rf -- "$BuildDir"; echo "Rescue build cache removed: $BuildDir"; exit 0 ;;
    --full-clean) rm -rf -- "$BuildDir"; CleanRescueOutputs; echo "Rescue build/output removed"; exit 0 ;;
    --NoId) BuildNoIdArtifacts=true ;;
    --help|-h) ShowUsage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; ShowUsage >&2; exit 2 ;;
  esac
  shift
done

[ "$BuildMode" != clean-build ] || rm -rf -- "$BuildDir"
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
[ -f "$RescueUpdaterHeader" ] || Fail "Rescue updater header not found: $RescueUpdaterHeader"
[ -f "$RescueUpdaterSource" ] || Fail "Rescue updater source not found: $RescueUpdaterSource"
[ -f "$RescueConfigHelper" ] || Fail "Rescue config helper not found: $RescueConfigHelper"
[ -f "$RescueHardwareDefinition" ] || Fail "Rescue hardware definition not found: $RescueHardwareDefinition"
if [ -n "$PartitionDefinition" ]; then
  [ -f "$PartitionDefinition" ] || Fail "Partition definition not found: $PartitionDefinition"
fi
[ -f "$ConfigDefinition" ] || Fail "Config definition not found: $ConfigDefinition"
[ -f "$DeviceTypeHelper" ] || Fail "DeviceType helper not found: $DeviceTypeHelper"
[ -f "$DeviceTypeFile" ] || Fail "DeviceType file not found: $DeviceTypeFile"
[ -f "$PrivateKey" ] || Fail "Private key not found: $PrivateKey"
[ -f "$PublicKey" ] || Fail "Public key not found: $PublicKey"
[ -f "$UpdateCommonDir/EspSignedArtifact.h" ] || Fail "EspSignedArtifact.h not found"
[ -f "$UpdateCommonDir/EspSignedArtifact.cpp" ] || Fail "EspSignedArtifact.cpp not found"
[ -f "$VersionsFile" ] || Fail "Versions file not found: $VersionsFile"
RescueVersion="$(sed -n 's/^Rescue=//p' "$VersionsFile" | head -n1)"
[ -n "$RescueVersion" ] || Fail "Rescue version missing in $VersionsFile"

python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$RescueSource" "$SketchDir/EspRescueUpdate.ino"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$RescueUpdaterHeader" "$SketchDir/EspRescueUpdater.h"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$RescueUpdaterSource" "$SketchDir/EspRescueUpdater.cpp"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$RescueHardwareDefinition" "$SketchDir/EspRescueHardware.h"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$UpdateCommonDir/EspSignedArtifact.h" "$SketchDir/EspSignedArtifact.h"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$UpdateCommonDir/EspSignedArtifact.cpp" "$SketchDir/EspSignedArtifact.cpp"
if [ -n "$PartitionDefinition" ]; then
  python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$PartitionDefinition" "$SketchDir/partitions.csv"
fi
python3 "$DeviceTypeHelper" prepare \
  "$DeviceTypeFile" \
  "$SketchDir/DeviceType.inc.h" \
  "$BuildDir/DeviceType.txt"
python3 "$DeviceTypeHelper" public-key-header \
  "$PublicKey" "$SketchDir/EspUpdatePublicKey.h"
python3 "$RescueConfigHelper" "$ConfigDefinition" "$SecretsFile" "$ProjectName" \
  "$RescueVersion" "$SketchDir/EspRescueProject.h"

CompileArgs=( compile --no-color --fqbn "$FQBN" --build-path "$CompileDir" )
[ -z "$COMPILE_VERBOSE" ] || CompileArgs+=( "$COMPILE_VERBOSE" )
CppFlagsDefined=false
for Property in "${ExtraBuildProperties[@]}"; do
  if [[ "$Property" == compiler.cpp.extra_flags=* ]]; then
    Property="$Property -DNO_GLOBAL_SERIAL"
    CppFlagsDefined=true
  fi
  CompileArgs+=( --build-property "$Property" )
done
if ! "$CppFlagsDefined"; then
  CompileArgs+=( --build-property "compiler.cpp.extra_flags=-DNO_GLOBAL_SERIAL" )
fi
for Library in "${ArduinoLibraries[@]}"; do
  [ -d "$Library" ] || Fail "Arduino library not found: $Library"
  CompileArgs+=( --library "$Library" )
done

if ! "$CLI" "${CompileArgs[@]}" "$SketchDir" >>"$LogFile" 2>&1; then
  Fail "ESP32 rescue V2 build failed"
fi

Firmware="$CompileDir/EspRescueUpdate.ino.bin"
TypedFirmware="$CompileDir/EspRescueUpdate.ino.bin.device"
SignedFirmware="$CompileDir/EspRescueUpdate.rescue_signed"
OutputFirmware="$BinDir/$ProjectName.Rescue.rescue_signed"
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
  "$DeviceTypeFile" rescue "$Firmware" "$TypedFirmware" >>"$LogFile" 2>&1
then
  Fail "Could not create typed ESP32 rescue application"
fi
if ! python3 "$DeviceTypeHelper" sign \
  "$TypedFirmware" "$PrivateKey" "$SignedFirmware" >>"$LogFile" 2>&1 || \
   ! python3 "$DeviceTypeHelper" verify \
  "$SignedFirmware" "$PublicKey" >>"$LogFile" 2>&1
then
  Fail "Could not sign/verify ESP32 rescue application"
fi

rm -rf -- "$OutputFirmware" "$OutputNoIdFirmware"
install -m 0644 "$SignedFirmware" "$OutputFirmware"
if "$BuildNoIdArtifacts"; then
  install -m 0644 "$Firmware" "$OutputNoIdFirmware"
fi
rm -rf -- "$TypedFirmware" "$SignedFirmware"

echo "Rescue revision: $RescueVersion"
echo "Signed ESP32 rescue V2 application: $OutputFirmware"
if "$BuildNoIdArtifacts"; then
  echo "Raw NoId rescue application: $OutputNoIdFirmware"
fi
echo "These outputs are application-only; they do not contain or replace bootloader/partition table."
