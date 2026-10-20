# EspCompileCommon.sh, Version: 1.37
# Common implementation sourced by project-specific EspCompile.sh scripts.
# Do not execute this file directly.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "EspCompileCommon.sh must be sourced by a project EspCompile.sh." >&2
  exit 2
fi

: "${ProjectDir:?ProjectDir not set}"
: "${PrjsDir:?PrjsDir not set}"
# shellcheck source=_TrashPaths.sh
source "$PrjsDir/tools/_TrashPaths.sh"
source "$PrjsDir/tools/_PruneDeviceFsArtifacts.sh"
: "${ProjectName:?ProjectName not set}"
: "${Platform:?Platform not set}"
: "${FQBN:?FQBN not set}"
: "${BinSubDir:?BinSubDir not set}"

COMPILE_VERBOSE="${COMPILE_VERBOSE:-}"
EnableSigning="${EnableSigning:-false}"
EnableLittleFs="${PRJS_ENABLE_LITTLEFS:-${EnableLittleFs:-true}}"
[[ "$EnableLittleFs" == true || "$EnableLittleFs" == false ]] || { echo "PRJS_ENABLE_LITTLEFS must be true or false." >&2; exit 2; }
EnableCompactUpdate="${EnableCompactUpdate:-false}"
EnableGzipArtifacts="${EnableGzipArtifacts:-false}"
EnableConfigRecoveryArtifact="${EnableConfigRecoveryArtifact:-true}"
ShowRescueReserve="${ShowRescueReserve:-false}"
CustomPartitionsFile="${CustomPartitionsFile:-}"
MaxApplicationSize="${MaxApplicationSize:-0}"
PreBuildSuccessMessage="${PreBuildSuccessMessage:-Pre-build completed}"

BuildRoot="${PRJS_BUILD_ROOT:-$ProjectDir/generated/build}"
CompileDir="$BuildRoot/$Platform"
SketchRoot="$BuildRoot/sketch"
SketchDir="$SketchRoot/$ProjectName"
SourceDir="$ProjectDir"
if [ -d "$ProjectDir/Sw" ]; then SourceDir="$ProjectDir/Sw"; fi
BinDir="$ProjectDir/generated/bin/$BinSubDir"
ConfiguredRescueBinDir="${PRJS_RESCUE_BIN_DIR:-$ProjectDir/rescue/bin}"
PreserveSharedBinArtifacts="${PRJS_PRESERVE_SHARED_BIN_ARTIFACTS:-false}"
LogFile="$CompileDir/Compile.log"
DevStateRoot="${PRJS_DEV_STATE_ROOT:-$ProjectDir/DevState}"

MountDir="$(dirname -- "$(dirname -- "$PrjsDir")")"
CLI="${ARDUINO_CLI_WRAPPER:-${ARDUINO2CLI_START:-$MountDir/Start_Arduino2cli.sh}}"
PreBuildScript="${PreBuildScript:-$ProjectDir/PreBuild.sh}"
PythonHelper="${PythonHelper:-$ProjectDir/EspCompile.py}"
ReadableHelper="${ReadableHelper:-$PrjsDir/tools/CreateReadableLittleFS.py}"
LittleFsUpdateHelper="${LittleFsUpdateHelper:-$PrjsDir/tools/CreateLittleFsUpdate.py}"
CombinedUpdateHelper="${CombinedUpdateHelper:-$PrjsDir/tools/CreateEspUpdate.py}"
DeviceTypeHelper="${DeviceTypeHelper:-$PrjsDir/tools/DeviceTypeArtifacts.py}"
DeviceTypeFile="${DeviceTypeFile:-$ProjectDir/DeviceType.txt}"
PrjsPrivateDir="${PRJS_PRIVATE_DIR:-$(dirname -- "$(dirname -- "$PrjsDir")")/PrjsPrivate}"
SecretsFile="${PRJS_SECRETS_FILE:-${SecretsFile:-$PrjsPrivateDir/Secrets.local.json}}"
PrivateKey="${PrivateKey:-$PrjsPrivateDir/private.key}"
PublicKey="${PublicKey:-$PrjsPrivateDir/public.key}"
Esp8266SigningHeaderHelper="${Esp8266SigningHeaderHelper:-$PrjsDir/Libs/Common/EspUpdate/EspUpdateSigning.py}"
SimpleFtpServerPrepareHelper="${SimpleFtpServerPrepareHelper:-$PrjsDir/tools/PrepareSimpleFtpServer.py}"

declare -p RequiredDataFiles >/dev/null 2>&1 || RequiredDataFiles=()
declare -p RequiredGeneratedFiles >/dev/null 2>&1 || RequiredGeneratedFiles=()
declare -p RequiredProjectFiles >/dev/null 2>&1 || RequiredProjectFiles=()
declare -p ArduinoLibraries >/dev/null 2>&1 || ArduinoLibraries=()
declare -p ExtraBuildProperties >/dev/null 2>&1 || ExtraBuildProperties=()
declare -p CommonSourceDirs >/dev/null 2>&1 || CommonSourceDirs=()
declare -p SketchSubDirs >/dev/null 2>&1 || SketchSubDirs=()

BuildGzip="$EnableGzipArtifacts"
BuildMode="incremental"
BuildNoIdArtifacts=false
BuildTransitionArtifact=false
DeviceTypeEnabled=false
declare -a DeviceNames=()
DeviceStateMode=false
ShowStatus=false
StatusInterval="${ESP_COMPILE_STATUS_INTERVAL:-10}"

DeviceDataDir()
{
  if "$DeviceStateMode"; then
    if [ -n "${PRJS_INSTANCES_DIR:-}" ]; then
      printf '%s/%s/LittleFS\n' "$PRJS_INSTANCES_DIR" "$1"
      return
    fi
    printf '%s\n' "$ProjectDir/generated/$1/data"
  else
    printf '%s\n' "$ProjectDir/data"
  fi
}

DeviceReadableDir()
{
  if "$DeviceStateMode"; then
    if [ -n "${PRJS_INSTANCES_DIR:-}" ]; then
      printf '%s/%s/LittleFS-readable\n' "$PRJS_INSTANCES_DIR" "$1"
      return
    fi
    printf '%s\n' "$ProjectDir/generated/$1/data-readable"
  else
    printf '%s\n' "$ProjectDir/generated/data-readable"
  fi
}

LoadDeviceNames()
{
  DeviceNames=()
  DeviceStateMode=false
  if [ -d "$DevStateRoot" ]; then
    while IFS= read -r -d '' DeviceDir; do
      DeviceNames+=( "$(basename -- "$DeviceDir")" )
    done < <(find "$DevStateRoot" -mindepth 1 -maxdepth 1 -type d -print0 | sort -z)
  fi
  if [ "${#DeviceNames[@]}" -gt 0 ]; then
    DeviceStateMode=true
  else
    DeviceNames=( "$ProjectName" )
  fi
}

ShowUsage()
{
  cat <<EOF
Usage: $(basename -- "$0") [--build | --clean | --full-clean] [--NoId] [--Transition] [--status]

Without a build-mode option, an incremental build is performed.

Options:
  --build       move compiler/build cache to Trash, then build
  --clean       move compiler/build cache to Trash and exit; keep generated/bin
  --full-clean  move compiler/build cache and this build's binary artifacts to Trash
  --NoId        additionally create raw firmware artifacts for serial/service use
  --Transition   additionally create legacy typed ESP32 Main/LittleFS artifacts
                 for upgrading an already-migrated device with an old Rescue
  --status      print phase changes and a heartbeat every 10 s during long steps
  --help, -h    show this help

PRJS_ENABLE_LITTLEFS=false builds firmware only (no filesystem images).
EOF
}

SetBuildMode()
{
  local NewMode="$1"

  if [ "$BuildMode" != "incremental" ] && [ "$BuildMode" != "$NewMode" ]; then
    echo "Only one of --build, --clean and --full-clean may be specified." >&2
    ShowUsage >&2
    exit 2
  fi
  BuildMode="$NewMode"
}

Now()
{
  date '+%H:%M:%S'
}

Status()
{
  "$ShowStatus" || return 0
  printf '[status %s] %s\n' "$(Now)" "$*"
}

RunLongStepToFile()
{
  local Label="$1"
  local OutputFile="$2"
  shift 2

  local Start=$SECONDS
  local Pid
  local Rc=0

  Status "$Label started"
  "$@" >>"$OutputFile" 2>&1 &
  Pid=$!

  if "$ShowStatus"; then
    while kill -0 "$Pid" 2>/dev/null; do
      sleep "$StatusInterval"
      if kill -0 "$Pid" 2>/dev/null; then
        printf '[status %s] %s still running, elapsed %ds\n' \
          "$(Now)" "$Label" "$(( SECONDS - Start ))"
      fi
    done
  fi

  wait "$Pid" || Rc=$?
  if (( Rc != 0 )); then
    if "$ShowStatus"; then
      printf '[status %s] %s FAILED after %ds (exit %d)\n' \
        "$(Now)" "$Label" "$(( SECONDS - Start ))" "$Rc" >&2
    fi
    return "$Rc"
  fi

  if "$ShowStatus"; then
    printf '[status %s] %s done after %ds\n' \
      "$(Now)" "$Label" "$(( SECONDS - Start ))"
  fi
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --build)
      SetBuildMode "clean-build"
      ;;
    --clean)
      SetBuildMode "clean-only"
      ;;
    --full-clean)
      SetBuildMode "full-clean-only"
      ;;
    --NoId)
      BuildNoIdArtifacts=true
      ;;
    --Transition)
      BuildTransitionArtifact=true
      ;;
    --status)
      ShowStatus=true
      ;;
    --help|-h)
      ShowUsage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      ShowUsage >&2
      exit 2
      ;;
  esac
  shift
done

if ! [[ "$StatusInterval" =~ ^[1-9][0-9]*$ ]]; then
  echo "Invalid ESP_COMPILE_STATUS_INTERVAL: $StatusInterval" >&2
  exit 2
fi

PauseOnError()
{
  if [ -t 0 ]; then
    read -r -p "Press enter to continue (exit)" _ || true
  fi
}

UnexpectedError()
{
  local ExitCode=$?
  local LineNr="$1"

  trap - ERR
  if declare -F RestoreDir >/dev/null; then
    RestoreDir
  fi
  echo "Unexpected error in EspCompile.sh at line $LineNr" >&2
  if [ -f "$LogFile" ]; then
    echo >&2
    echo "Relevant end of $LogFile:" >&2
    tail -n 80 "$LogFile" >&2 || true
  fi
  PauseOnError
  exit "$ExitCode"
}

trap 'UnexpectedError "$LINENO"' ERR

ShowFailure()
{
  local Message="$1"

  trap - ERR
  if declare -F RestoreDir >/dev/null; then
    RestoreDir
  fi
  echo "Error: $Message" >&2
  if [ -f "$LogFile" ]; then
    echo >&2
    echo "Relevant end of $LogFile:" >&2
    tail -n 80 "$LogFile" >&2 || true
  fi
  PauseOnError
  exit 1
}

ShowWarnings()
{
  local Warnings

  Warnings="$(grep -E '(^|[[:space:]:])warning:' "$LogFile" || true)"
  if [ -n "$Warnings" ]; then
    echo
    echo "Warnings:"
    printf '%s\n' "$Warnings"
  fi
}

ShowMemorySummary()
{
  local MemorySummary

  MemorySummary="$(
    grep -E \
      'Sketch uses|Global variables use|Der Sketch verwendet|Globale Variablen verwenden|^[[:space:]]*(IROM|IRAM|RODATA|DATA)[[:space:]]*:|RAM:|Flash:' \
      "$LogFile" \
      || true
  )"
  if [ -n "$MemorySummary" ]; then
    echo
    echo "Memory summary:"
    printf '%s\n' "$MemorySummary"
  fi
}

CopySketchSources()
{
  local SourceFile
  local SubDir
  local CommonDir
  local InputStage
  InputStage="$(mktemp -d "$SketchRoot/.inputs.XXXXXX")"
  while IFS= read -r -d '' SourceFile; do
    cp -p "$SourceFile" "$InputStage/$(basename "$SourceFile")"
  done < <(find "$SourceDir" -maxdepth 1 -type f \
    \( -name '*.ino' -o -name '*.cpp' -o -name '*.c' -o -name '*.h' \
    -o -name '*.hpp' -o -name '*.S' \) -print0)
  for SubDir in "${SketchSubDirs[@]}"; do
    [ -d "$ProjectDir/$SubDir" ] || ShowFailure "Sketch subdirectory not found: $ProjectDir/$SubDir"
    cp -a "$ProjectDir/$SubDir" "$InputStage/$SubDir"
  done
  if [ -n "$CustomPartitionsFile" ]; then
    [ "$Platform" = "esp32" ] || ShowFailure "CustomPartitionsFile is supported only for ESP32 builds"
    [ -f "$CustomPartitionsFile" ] || ShowFailure "Custom partition CSV not found: $CustomPartitionsFile"
    install -m 0644 "$CustomPartitionsFile" "$InputStage/partitions.csv"
  fi
  python3 "$PrjsDir/tools/SyncGeneratedFiles.py" tree "$InputStage" "$SketchDir" \
    --keep EspUpdatePublicKey.h --keep EspUpdateSigning.h
  rm -rf -- "$InputStage"
  for CommonDir in "${CommonSourceDirs[@]}"; do
    [ -d "$PrjsDir/$CommonDir" ] || ShowFailure "Common source directory not found: $PrjsDir/$CommonDir"
    python3 "$PrjsDir/tools/SyncGeneratedFiles.py" tree "$PrjsDir/$CommonDir" "$SketchRoot/$CommonDir"
    python3 "$PrjsDir/tools/SyncGeneratedFiles.py" tree "$PrjsDir/$CommonDir" "$CompileDir/$CommonDir"
  done
}

VerifySigningKeys()
{
  if ! openssl pkey -in "$PrivateKey" -check -noout >>"$LogFile" 2>&1; then
    ShowFailure "Invalid private key: $PrivateKey"
  fi
  if ! openssl pkey -pubin -in "$PublicKey" -noout >>"$LogFile" 2>&1; then
    ShowFailure "Invalid public key: $PublicKey"
  fi
  if ! openssl pkey -in "$PrivateKey" -pubout -outform DER \
    -out "$CompileDir/private-public.der" >>"$LogFile" 2>&1
  then
    ShowFailure "Could not derive public key from private key"
  fi
  if ! openssl pkey -pubin -in "$PublicKey" -outform DER \
    -out "$CompileDir/public.der" >>"$LogFile" 2>&1
  then
    ShowFailure "Could not normalize public key"
  fi
  if ! cmp -s "$CompileDir/private-public.der" "$CompileDir/public.der"; then
    ShowFailure "private.key and public.key do not form a key pair"
  fi
  rm -rf -- "$CompileDir/private-public.der" "$CompileDir/public.der"
  echo "Signing keys: verified"
}

VerifySignedFile()
{
  local SourceFile="$1"
  local SignedFile="$2"
  local VerifyPrefix="$CompileDir/.verify.$(basename "$SignedFile")"
  local VerifyPayload="$VerifyPrefix.payload"
  local VerifySignature="$VerifyPrefix.signature"

  if ! python3 "$PythonHelper" extract-signature \
    "$SourceFile" "$SignedFile" "$VerifyPayload" "$VerifySignature" \
    >>"$LogFile" 2>&1
  then
    ShowFailure "Signed-file structure check failed: $SignedFile"
  fi

  if ! openssl dgst -sha256 -verify "$PublicKey" -signature "$VerifySignature" \
    "$VerifyPayload" >>"$LogFile" 2>&1
  then
    ShowFailure "Signature verification failed: $SignedFile"
  fi

  rm -rf -- "$VerifyPayload" "$VerifySignature"
}

SignFile()
{
  local SourceFile="$1"
  local SignedFile="$2"

  if ! "$SigningPython" "$SigningTool" \
    --mode sign \
    --privatekey "$PrivateKey" \
    --bin "$SourceFile" \
    --out "$SignedFile" >>"$LogFile" 2>&1
  then
    ShowFailure "Signing failed: $SourceFile"
  fi

  VerifySignedFile "$SourceFile" "$SignedFile"
  echo "Signed and verified: $(basename "$SignedFile")"
}

PrepareDeviceTypeFiles()
{
  local FirstDataDir
  local DeviceName

  if [ ! -f "$DeviceTypeFile" ]; then
    if "$BuildNoIdArtifacts"; then
      ShowFailure "--NoId requires DeviceType.txt: $DeviceTypeFile"
    fi
    if "$BuildTransitionArtifact"; then
      ShowFailure "--Transition requires DeviceType.txt: $DeviceTypeFile"
    fi
    DeviceTypeEnabled=false
    return
  fi

  [ -f "$DeviceTypeHelper" ] || ShowFailure "DeviceType helper not found: $DeviceTypeHelper"
  FirstDataDir="$(DeviceDataDir "${DeviceNames[0]}")"
  if ! python3 "$DeviceTypeHelper" prepare \
    "$DeviceTypeFile" \
    "$SourceDir/DeviceType.inc.h" \
    "$FirstDataDir/DeviceType.txt" >>"$LogFile" 2>&1
  then
    ShowFailure "Could not prepare DeviceType files"
  fi
  for DeviceName in "${DeviceNames[@]:1}"; do
    install -m 0644 "$FirstDataDir/DeviceType.txt" "$(DeviceDataDir "$DeviceName")/DeviceType.txt"
  done
  DeviceTypeEnabled=true
}

CreateDeviceTypeArtifact()
{
  local ArtifactType="$1"
  local SourceFile="$2"
  local OutputFile="$3"

  if ! "$DeviceTypeEnabled"; then
    cp -p "$SourceFile" "$OutputFile"
    return
  fi

  if ! python3 "$DeviceTypeHelper" wrap \
    "$DeviceTypeFile" "$ArtifactType" "$SourceFile" "$OutputFile" \
    >>"$LogFile" 2>&1
  then
    ShowFailure "Could not create DeviceType $ArtifactType artifact"
  fi
}

FindMkLittleFs()
{
  local MkLittleFsDir="$ArduinoDataDir/../My/tools"

  [ -d "$MkLittleFsDir" ] || ShowFailure "mklittlefs directory not found"
  MkLittleFs="$(
    find "$MkLittleFsDir" \
      -mindepth 2 -maxdepth 2 -type f -name mklittlefs -print 2>>"$LogFile" \
      | sort -V \
      | tail -n 1
  )"
  [ -x "$MkLittleFs" ] || ShowFailure "mklittlefs not found"
}

SetupEsp8266Tools()
{
  local Esp8266HardwareDir="$ArduinoDataDir/packages/esp8266/hardware/esp8266"
  local Esp8266PythonDir="$ArduinoDataDir/packages/esp8266/tools/python3"

  [ -d "$Esp8266HardwareDir" ] || ShowFailure "ESP8266 hardware directory not found"

  Esp8266CoreDir="$(
    find "$Esp8266HardwareDir" \
      -mindepth 1 -maxdepth 1 -type d -print 2>>"$LogFile" \
      | sort -V \
      | tail -n 1
  )"
  [ -n "$Esp8266CoreDir" ] || ShowFailure "ESP8266 core not found"

  if "$EnableSigning"; then
    SigningTool="$Esp8266CoreDir/tools/signing.py"
    [ -f "$SigningTool" ] || ShowFailure "ESP8266 signing tool not found: $SigningTool"

    SigningPython=""
    if [ -d "$Esp8266PythonDir" ]; then
      SigningPython="$(
        find "$Esp8266PythonDir" \
          -mindepth 2 -maxdepth 2 -type f -name python3 -print 2>>"$LogFile" \
          | sort -V \
          | tail -n 1
      )"
    fi
    if [ -z "$SigningPython" ]; then
      SigningPython="$(command -v python3)"
    fi
    [ -x "$SigningPython" ] || ShowFailure "Python for ESP8266 signing not executable"
  fi

  if "$EnableLittleFs"; then
    FindMkLittleFs
  fi
}

GenerateEsp8266SigningHeader()
{
  if ! python3 "$Esp8266SigningHeaderHelper" \
    "$PublicKey" "$SketchDir/EspUpdateSigning.h" >>"$LogFile" 2>&1
  then
    ShowFailure "Could not generate ESP8266 RSA signing header"
  fi
}

UsesGnFtpServer()
{
  local Library

  for Library in "${ArduinoLibraries[@]}"; do
    if [ "$(basename -- "$Library")" = "GnFtpServer" ]; then
      return 0
    fi
  done
  return 1
}

PrepareSimpleFtpServer()
{
  local ArduinoUserDir
  local SourceDir
  local PrepareOutput

  [ -f "$SimpleFtpServerPrepareHelper" ] || \
    ShowFailure "SimpleFTPServer prepare helper not found: $SimpleFtpServerPrepareHelper"

  if ! ArduinoUserDir="$(
    printf '%s\n' "$CliConfig" | python3 -c '
import json
import sys
config = json.load(sys.stdin)
config = config.get("config", config)
print(config["directories"]["user"])
'
  )"
  then
    ShowFailure "Could not determine Arduino user directory"
  fi

  SourceDir="$ArduinoUserDir/libraries/SimpleFTPServer"
  [ -d "$SourceDir" ] || ShowFailure "SimpleFTPServer library not found: $SourceDir"

  if ! PrepareOutput="$(
    python3 "$SimpleFtpServerPrepareHelper" "$SourceDir" 2>&1
  )"
  then
    printf '%s\n' "$PrepareOutput" >>"$LogFile"
    ShowFailure "Could not verify/patch SimpleFTPServer configuration"
  fi

  printf '%s\n' "$PrepareOutput" | tee -a "$LogFile"
}

SetupEsp32Tools()
{
  local Esp32HardwareDir="$ArduinoDataDir/packages/esp32/hardware/esp32"

  [ -d "$Esp32HardwareDir" ] || ShowFailure "ESP32 hardware directory not found"
  Esp32CoreDir="$(
    find "$Esp32HardwareDir" -mindepth 1 -maxdepth 1 -type d -print 2>>"$LogFile" \
      | sort -V \
      | tail -n 1
  )"
  [ -n "$Esp32CoreDir" ] || ShowFailure "ESP32 core not found"

  GenPartTool="$Esp32CoreDir/tools/gen_esp32part.py"
  if [ -f "$GenPartTool" ]; then
    GenPartCommand=( python3 "$GenPartTool" )
   elif [ -x "$Esp32CoreDir/tools/gen_esp32part" ]; then
    GenPartCommand=( "$Esp32CoreDir/tools/gen_esp32part" )
   else
    ShowFailure "ESP32 partition tool not found"
  fi

  if "$EnableLittleFs"; then
    FindMkLittleFs
  fi
}

RunIdfCompile()
{
  local PortableDir="${ARDUINO_PORTABLE_DIR:-$MountDir/Portables/Arduino2/portable}"
  local CoreDir="${ARDUINO_CORE_DIR:-$Esp32CoreDir}"
  local IdfBuildDir="$CompileDir/idf"
  local IdfProjectDir="$BuildRoot/IdfProject"
  local Dependencies="$BuildRoot/MainDependencies"
  python3 "$PrjsDir/tools/PrepareArduinoIdf.py" "$CoreDir" "$PortableDir/Sketchbook/libraries" "$Dependencies" >>"$LogFile" 2>&1 || \
    ShowFailure "Could not prepare Arduino ESP-IDF component"
  python3 - "$PrjsDir/tools" "$ProjectDir" "$BuildRoot/ProjectVersion.cmake" <<'PYIDF'
import sys
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from UpdateVersionFiles import read_versions
from SyncGeneratedFiles import write_text_if_changed
version=read_versions(Path(sys.argv[2])/'Versions.txt')['Firmware']
write_text_if_changed(Path(sys.argv[3]),'set(PROJECT_VER "'+version+'")\n')
PYIDF
  python3 "$PrjsDir/tools/PrepareIdfProject.py" "$ProjectDir" "$IdfProjectDir"
  : >"$CompileDir/IdfCompile.log"
  (
    source "${ESP_IDF_ENABLE_SCRIPT:-$MountDir/Portables/EnableEspIdf.sh}"
    export PATH="${ESP_IDF_COMPILER_BIN:-$IDF_TOOLS_PATH/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin}:$PATH"
    command -v xtensa-esp32-elf-gcc >/dev/null || ShowFailure "ESP32 IDF compiler not found"
    RunLongStepToFile "ESP-IDF compile for $ProjectName" "$CompileDir/IdfCompile.log" \
      idf.py -C "$IdfProjectDir" -B "$IdfBuildDir" \
        -D "SDKCONFIG=$IdfBuildDir/sdkconfig" \
        -D "PRJS_IDF_DEPENDENCIES=$Dependencies" \
        -D "PRJS_GENERATED_INCLUDE=$SketchDir" \
        -D "PRJS_VERSION_FILE=$BuildRoot/ProjectVersion.cmake" build
  ) || { cat "$CompileDir/IdfCompile.log" >>"$LogFile"; ShowFailure "ESP-IDF build failed"; }
  cat "$CompileDir/IdfCompile.log" >>"$LogFile"
  python3 "$ProjectDir/VerifyIdfBuild.py" "$IdfBuildDir" >>"$LogFile" 2>&1 || \
    ShowFailure "ESP-IDF image or SDK geometry verification failed"
  python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$IdfBuildDir/$ProjectName.bin" "$CompileDir/$ProjectName.ino.bin"
  python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$IdfBuildDir/partition_table/partition-table.bin" "$CompileDir/$ProjectName.ino.partitions.bin"
}

RunArduinoCompile()
{
  if [ "${CompileBackend:-arduino}" = idf ]; then RunIdfCompile; return; fi
  local Property
  local LibraryDir
  local Library
  local ExpandedProperty
  local -a CompileArgs

  CompileArgs=( compile )
  if [ -n "$COMPILE_VERBOSE" ]; then
    CompileArgs+=( "$COMPILE_VERBOSE" )
  fi
  CompileArgs+=(
    --no-color
    --fqbn "$FQBN"
    --build-path "$CompileDir"
  )

  for Property in "${ExtraBuildProperties[@]}"; do
    ExpandedProperty="${Property//@SKETCH_DIR@/$SketchDir}"
    CompileArgs+=( --build-property "$ExpandedProperty" )
  done

  # Project libraries are always passed as exact library roots.
  # Do NOT replace these with --libraries <parent-dir>: doing so lets Arduino CLI
  # discover the same project library more than once and produces ambiguous
  # "Multiple libraries were found" diagnostics.
  for Library in "${ArduinoLibraries[@]}"; do
    [ -d "$Library" ] || ShowFailure "Arduino library not found: $Library"
    CompileArgs+=( --library "$Library" )
  done


  local CompileOutput

  CompileOutput="$(mktemp "${TMPDIR:-/tmp}/${ProjectName}.ArduinoCompile.XXXXXX.out")" || \
    ShowFailure "Could not create temporary compiler-output file"

  if ! RunLongStepToFile \
    "Arduino ${Platform^^} compile for $ProjectName" \
    "$CompileOutput" \
    "$CLI" "${CompileArgs[@]}" "$SketchDir"
  then
    cat "$CompileOutput" >>"$LogFile"
    trap - ERR
    echo "Error: ${Platform^^} build failed" >&2
    echo >&2
    echo "Arduino compiler output:" >&2
    cat "$CompileOutput" >&2
    rm -f -- "$CompileOutput"
    PauseOnError
    exit 1
  fi
  cat "$CompileOutput" >>"$LogFile"
  rm -f -- "$CompileOutput"
}

ShowRescueOtaReserveInfo()
{
  local SectorSize=4096
  local FlashLayout
  local FlashLayoutLower
  local LinkerScript
  local FsStartHex
  local FsStart
  local OtaEnd
  local FirmwareSize
  local FirmwareRounded
  local RescueBinDir="$ConfiguredRescueBinDir"
  local RescueNormal="$RescueBinDir/$ProjectName.Rescue.ino.bin.signed"
  local RescueGzip="$RescueBinDir/$ProjectName.Rescue.ino.bin.gz.signed"
  local FoundRescue=false

  FlashLayout="${FQBN#*eesz=}"
  if [ "$FlashLayout" = "$FQBN" ]; then
    echo
    echo "Rescue OTA reserve:"
    echo "  Not available: FQBN contains no eesz flash layout."
    return
  fi
  FlashLayout="${FlashLayout%%,*}"
  FlashLayoutLower="$(printf '%s' "$FlashLayout" | tr '[:upper:]' '[:lower:]')"
  LinkerScript="$Esp8266CoreDir/tools/sdk/ld/eagle.flash.$FlashLayoutLower.ld"
  if [ ! -f "$LinkerScript" ]; then
    echo
    echo "Rescue OTA reserve:"
    printf '  Not available: linker script not found: %s\n' "$LinkerScript"
    return
  fi

  FsStartHex="$(
    sed -nE 's/.*PROVIDE \( _FS_start = (0x[0-9A-Fa-f]+) \);.*/\1/p' \
      "$LinkerScript" \
      | head -n 1
  )"
  if [ -z "$FsStartHex" ]; then
    echo
    echo "Rescue OTA reserve:"
    printf '  Not available: _FS_start not found in %s\n' "$LinkerScript"
    return
  fi

  FsStart=$(( FsStartHex ))
  OtaEnd=$(( FsStart - 0x40200000 ))
  FirmwareSize="$(stat -c '%s' "$Firmware")"
  FirmwareRounded=$(( ( FirmwareSize + SectorSize - 1 ) & ~( SectorSize - 1 ) ))

  echo
  echo "Rescue OTA reserve:"
  printf '  OTA area to LittleFS: %d bytes (FS start 0x%05X)\n' \
    "$OtaEnd" "$OtaEnd"
  printf '  Firmware:             %d bytes -> %d bytes sector-rounded\n' \
    "$FirmwareSize" "$FirmwareRounded"

  ShowRescueCandidateReserve()
  {
    local Label="$1"
    local RescueFile="$2"
    local RescueSize
    local RescueRounded
    local Reserve
    local Shortage
    local ShortageSectors

    [ -f "$RescueFile" ] || return 0
    FoundRescue=true

    RescueSize="$(stat -c '%s' "$RescueFile")"
    RescueRounded=$(( ( RescueSize + SectorSize - 1 ) & ~( SectorSize - 1 ) ))
    Reserve=$(( OtaEnd - FirmwareRounded - RescueRounded ))

    printf '  %-20s %d bytes -> %d bytes sector-rounded\n' \
      "$Label:" "$RescueSize" "$RescueRounded"
    if (( Reserve >= 0 )); then
      printf '  %-20s %d bytes (%d sectors)\n' \
        "Reserve:" "$Reserve" "$(( Reserve / SectorSize ))"
    else
      Shortage=$(( -Reserve ))
      ShortageSectors=$(( ( Shortage + SectorSize - 1 ) / SectorSize ))
      printf '  %-20s -%d bytes\n' "Reserve:" "$Shortage"
      printf '  WARNING: Rescue OTA no longer fits; short by %d bytes (%d sectors).\n' \
        "$Shortage" "$ShortageSectors"
    fi
  }

  ShowRescueCandidateReserve "Rescue signed" "$RescueNormal"
  ShowRescueCandidateReserve "Rescue gzip signed" "$RescueGzip"

  if ! "$FoundRescue"; then
    printf '  Not available: no signed rescue firmware in %s\n' "$RescueBinDir"
    echo "  Run ./EspRescueCompile.sh first."
  fi
}

PrepareMainBinDir()
{
  local DeviceStateDir
  local DeviceName
  local Artifact
  local ArtifactName
  if ! "$PreserveSharedBinArtifacts"; then
    if "$EnableLittleFs" || [ "$BuildMode" = full-clean-only ]; then
      rm -rf -- "$BinDir"
    elif [ -d "$BinDir" ]; then
      # A firmware-only build keeps the last published filesystem images.
      while IFS= read -r -d '' Artifact; do
        [[ "${Artifact##*/}" == *.littlefs* ]] && continue
        rm -rf -- "$Artifact"
      done < <(find "$BinDir" -mindepth 1 -maxdepth 1 -print0)
    fi
    mkdir -p -- "$BinDir"
    return
  fi

  mkdir -p -- "$BinDir"
  local -a OldArtifacts=()
  while IFS= read -r -d '' Artifact; do
    OldArtifacts+=( "$Artifact" )
  done < <(find "$BinDir" -mindepth 1 -maxdepth 1 -type f \
    \( -name "$ProjectName.ino.*" -o -name "$ProjectName.NoId.ino.*" \
    -o -name "$ProjectName.Transition.ino.*" \) -print0)
  if [ -d "$DevStateRoot" ]; then
    for DeviceStateDir in "$DevStateRoot"/*; do
      [ -d "$DeviceStateDir" ] || continue
      DeviceName="${DeviceStateDir##*/}"
      ArtifactName="$(FsArtifactName "$DeviceStateDir/Config.json")" || return $?
      while IFS= read -r -d '' Artifact; do
        OldArtifacts+=( "$Artifact" )
      done < <(find "$BinDir" -mindepth 1 -maxdepth 1 -type f \
        \( -name "$DeviceName.ino.*" -o -name "$DeviceName.Config.json.update*" \
        -o -name "$DeviceName.Transition.ino.*" -o -name "$ArtifactName.ino.littlefs*" \) -print0)
    done
  fi
  for Artifact in "${OldArtifacts[@]}"; do
    if ! "$EnableLittleFs" && [ "$BuildMode" != full-clean-only ] &&
       [[ "${Artifact##*/}" == *.littlefs* ]]; then
      continue
    fi
    rm -rf -- "$Artifact"
  done
}

BuildSignedEsp8266Outputs()
{
  local FirmwareSigned
  local FirmwareTyped
  local FirmwareNoIdSigned
  local FirmwareGz=""
  local FirmwareGzTyped=""
  local FirmwareGzSigned=""
  local FirmwareGzNoIdSigned=""
  local CombinedFirmware=""
  local DeviceName
  local DataDir
  local DataReadableDir
  local ConfigSource
  local ConfigTyped
  local ConfigSigned
  local LittleFs
  local LittleFsTyped
  local LittleFsSigned
  local LittleFsUpdate
  local CombinedUpdate
  local CombinedUpdateTyped
  local CombinedUpdateSigned
  local OutputFile
  local -a OutputFiles=()
  local -a IntermediateFiles=()

  Firmware="$CompileDir/$ProjectName.ino.bin"
  FirmwareTyped="$CompileDir/$ProjectName.ino.bin.device"
  FirmwareSigned="$CompileDir/$ProjectName.ino.bin.signed"
  FirmwareNoIdSigned="$CompileDir/$ProjectName.NoId.ino.bin.signed"

  [ -f "$Firmware" ] || ShowFailure "Firmware binary not generated: $Firmware"
  CreateDeviceTypeArtifact firmware "$Firmware" "$FirmwareTyped"
  SignFile "$FirmwareTyped" "$FirmwareSigned"
  OutputFiles+=( "$Firmware" "$FirmwareSigned" )
  IntermediateFiles+=( "$FirmwareTyped" )

  if "$BuildNoIdArtifacts"; then
    SignFile "$Firmware" "$FirmwareNoIdSigned"
    OutputFiles+=( "$FirmwareNoIdSigned" )
  fi

  if "$BuildGzip"; then
    FirmwareGz="$CompileDir/$ProjectName.ino.bin.gz"
    FirmwareGzTyped="$CompileDir/$ProjectName.ino.bin.gz.device"
    FirmwareGzSigned="$CompileDir/$ProjectName.ino.bin.gz.signed"
    FirmwareGzNoIdSigned="$CompileDir/$ProjectName.NoId.ino.bin.gz.signed"
    gzip -n -9 -c "$Firmware" >"$FirmwareGz" || ShowFailure "Firmware compression failed"
    gzip -t "$FirmwareGz" >>"$LogFile" 2>&1 || ShowFailure "Firmware gzip check failed"
    CreateDeviceTypeArtifact firmware "$FirmwareGz" "$FirmwareGzTyped"
    SignFile "$FirmwareGzTyped" "$FirmwareGzSigned"
    OutputFiles+=( "$FirmwareGz" "$FirmwareGzSigned" )
    IntermediateFiles+=( "$FirmwareGzTyped" )
    if "$BuildNoIdArtifacts"; then
      SignFile "$FirmwareGz" "$FirmwareGzNoIdSigned"
      OutputFiles+=( "$FirmwareGzNoIdSigned" )
    fi
  fi

  if "$EnableCompactUpdate"; then
    CombinedFirmware="$CompileDir/$ProjectName.ino.bin.update.gz"
    gzip -n -9 -c "$Firmware" >"$CombinedFirmware" || ShowFailure "Combined firmware compression failed"
    gzip -t "$CombinedFirmware" >>"$LogFile" 2>&1 || ShowFailure "Combined firmware gzip check failed"
    IntermediateFiles+=( "$CombinedFirmware" )
  fi

  for DeviceName in "${DeviceNames[@]}"; do
    DataDir="$(DeviceDataDir "$DeviceName")"
    DataReadableDir="$(DeviceReadableDir "$DeviceName")"

    if "$DeviceTypeEnabled"; then
      ConfigSource="$DataDir/Config.json"
      ConfigTyped="$CompileDir/$DeviceName.Config.json.update.device"
      ConfigSigned="$CompileDir/$DeviceName.Config.json.update.signed"
      [ -f "$ConfigSource" ] || ShowFailure "Config recovery source not found: $ConfigSource"
      CreateDeviceTypeArtifact config "$ConfigSource" "$ConfigTyped"
      SignFile "$ConfigTyped" "$ConfigSigned"
      OutputFiles+=( "$ConfigSigned" )
      IntermediateFiles+=( "$ConfigTyped" )
    fi

    if "$EnableLittleFs"; then
      LittleFs="$CompileDir/$DeviceName.ino.littlefs"
      LittleFsTyped="$CompileDir/$DeviceName.ino.littlefs.device"
      LittleFsSigned="$CompileDir/$(FsArtifactName "$DataDir/Config.json").ino.littlefs.signed"
      if ! "$MkLittleFs" -c "$DataDir" -p "$LittleFsPageSize" -b "$LittleFsBlockSize" \
        -s "$LittleFsSize" "$LittleFs" >>"$LogFile" 2>&1
      then
        ShowFailure "LittleFS image creation failed for $DeviceName"
      fi
      [ "$(stat -c '%s' "$LittleFs")" -eq "$LittleFsSize" ] || \
        ShowFailure "LittleFS image has an unexpected size for $DeviceName"
      CreateDeviceTypeArtifact filesystem "$LittleFs" "$LittleFsTyped"
      SignFile "$LittleFsTyped" "$LittleFsSigned"
      OutputFiles+=( "$LittleFs" "$LittleFsSigned" )
      IntermediateFiles+=( "$LittleFsTyped" )

      if ! python3 "$ReadableHelper" --strict-json "$DataDir" "$DataReadableDir" >>"$LogFile" 2>&1; then
        ShowFailure "Could not create readable LittleFS snapshot for $DeviceName"
      fi
    fi

    if "$EnableCompactUpdate"; then
      LittleFsUpdate="$CompileDir/$DeviceName.ino.littlefs.update"
      CombinedUpdate="$CompileDir/$DeviceName.ino.update"
      CombinedUpdateTyped="$CompileDir/$DeviceName.ino.update.device"
      CombinedUpdateSigned="$CompileDir/$DeviceName.ino.update.signed"
      if ! python3 "$LittleFsUpdateHelper" "$DataDir" "$LittleFsSize" "$LittleFsUpdate" >>"$LogFile" 2>&1; then
        ShowFailure "Compact LittleFS update package creation failed for $DeviceName"
      fi
      if ! python3 "$CombinedUpdateHelper" "$CombinedFirmware" "$LittleFsUpdate" "$CombinedUpdate" >>"$LogFile" 2>&1; then
        ShowFailure "Combined update package creation failed for $DeviceName"
      fi
      CreateDeviceTypeArtifact combined "$CombinedUpdate" "$CombinedUpdateTyped"
      SignFile "$CombinedUpdateTyped" "$CombinedUpdateSigned"
      OutputFiles+=( "$CombinedUpdateSigned" )
      IntermediateFiles+=( "$LittleFsUpdate" "$CombinedUpdate" "$CombinedUpdateTyped" )
    fi
  done

  PrepareMainBinDir
  for OutputFile in "${OutputFiles[@]}"; do
    install -m 0644 "$OutputFile" "$BinDir/$(basename "$OutputFile")" || \
      ShowFailure "Could not copy output file: $OutputFile"
  done
  rm -rf -- "${IntermediateFiles[@]}"

  ShowMemorySummary
  ShowWarnings
  if "$ShowRescueReserve"; then
    ShowRescueOtaReserveInfo
  fi

  echo
  echo "Firmware built once for project: $ProjectName"
  printf '  %s\n' "$BinDir/$(basename "$FirmwareSigned")"
  for DeviceName in "${DeviceNames[@]}"; do
    echo "Device artifacts: $DeviceName"
    if "$EnableCompactUpdate"; then
      printf '  Combined: %s/%s.ino.update.signed\n' "$BinDir" "$DeviceName"
    fi
    if "$EnableLittleFs"; then
      printf '  LittleFS: %s/%s.ino.littlefs.signed\n' "$BinDir" "$(FsArtifactName "$(DeviceDataDir "$DeviceName")/Config.json")"
      printf '  Readable: %s\n' "$(DeviceReadableDir "$DeviceName")"
    fi
    if "$DeviceTypeEnabled"; then
      printf '  Config:   %s/%s.Config.json.update.signed\n' "$BinDir" "$DeviceName"
    fi
  done

  if "$BuildNoIdArtifacts"; then
    echo
    echo "One-time NoId migration artifacts:"
    printf '  Firmware: %s\n' "$BinDir/$(basename "$FirmwareNoIdSigned")"
    if "$BuildGzip"; then
      printf '  Firmware: %s (smaller)\n' "$BinDir/$(basename "$FirmwareGzNoIdSigned")"
    fi
  fi
}

BuildEsp32Outputs()
{
  local PartitionsBin="$CompileDir/$ProjectName.ino.partitions.bin"
  local PartitionsCsv="$CompileDir/$ProjectName.ino.partitions.csv"
  local ExpectedPartitionsBin="$CompileDir/$ProjectName.expected.partitions.bin"
  local ApplicationFirmware="$CompileDir/$ProjectName.ino.bin"
  local TypedApplicationFirmware="$CompileDir/$ProjectName.ino.bin.device"
  local SignedApplicationFirmware="$CompileDir/$ProjectName.ino.bin_signed"
  local NoIdApplicationOutput="$BinDir/$ProjectName.NoId.ino.bin"
  local TransitionApplicationOutput="$BinDir/$ProjectName.Transition.ino.bin"
  local DeviceName
  local DataDir
  local DataReadableDir
  local ConfigSource
  local ConfigTyped
  local LittleFs
  local TypedLittleFs
  local SignedLittleFs
  local FirmwareFile
  local OutputFile
  local LittleFsOffset
  local LittleFsSizeDetected
  local -a FirmwareFiles
  local -a OutputFiles=()
  local -a IntermediateFiles=()

  if [ "${CompileBackend:-arduino}" = idf ]; then
    # Partition tables stay in the build cache for verification, not OTA selection.
    # Never publish stale Arduino bootloader/merged outputs as IDF artifacts.
    FirmwareFiles=( "$ApplicationFirmware" )
  else
    mapfile -d '' FirmwareFiles < <(
      find "$CompileDir" -maxdepth 1 -type f \( -name '*.bin' -o -name '*.uf2' \) -print0 | sort -z
    )
  fi
  [ "${#FirmwareFiles[@]}" -gt 0 ] || ShowFailure "No ESP32 firmware files generated"
  [ -f "$ApplicationFirmware" ] || ShowFailure "ESP32 application firmware not generated: $ApplicationFirmware"
  if (( MaxApplicationSize > 0 )) && (( $(stat -c '%s' "$ApplicationFirmware") > MaxApplicationSize )); then
    ShowFailure "ESP32 application exceeds configured slot size: $(stat -c '%s' "$ApplicationFirmware") > $MaxApplicationSize bytes"
  fi

  if [ -n "$CustomPartitionsFile" ]; then
    [ -f "$PartitionsBin" ] || ShowFailure "ESP32 partition table not generated: $PartitionsBin"
    if ! "${GenPartCommand[@]}" "$CustomPartitionsFile" "$ExpectedPartitionsBin" >>"$LogFile" 2>&1; then
      ShowFailure "Could not compile custom ESP32 partition table"
    fi
    if ! cmp -s "$ExpectedPartitionsBin" "$PartitionsBin"; then
      ShowFailure "Arduino build did not use the configured custom partition table"
    fi
    rm -rf -- "$ExpectedPartitionsBin"
    echo "Custom ESP32 partition table: verified"
  fi

  if "$DeviceTypeEnabled"; then
    CreateDeviceTypeArtifact firmware "$ApplicationFirmware" "$TypedApplicationFirmware"
    IntermediateFiles+=( "$TypedApplicationFirmware" )
    if "$EnableSigning"; then
      if ! python3 "$DeviceTypeHelper" sign \
        "$TypedApplicationFirmware" "$PrivateKey" "$SignedApplicationFirmware" >>"$LogFile" 2>&1 || \
         ! python3 "$DeviceTypeHelper" verify \
        "$SignedApplicationFirmware" "$PublicKey" >>"$LogFile" 2>&1
      then
        ShowFailure "Could not sign/verify ESP32 application firmware"
      fi
      IntermediateFiles+=( "$SignedApplicationFirmware" )
    fi
  fi

  if "$EnableLittleFs"; then
    [ -f "$PartitionsBin" ] || ShowFailure "ESP32 partition table not generated: $PartitionsBin"
    "${GenPartCommand[@]}" "$PartitionsBin" "$PartitionsCsv" >>"$LogFile" 2>&1 || \
      ShowFailure "Could not decode ESP32 partition table"
    if ! read -r LittleFsOffset LittleFsSizeDetected < <(python3 "$PythonHelper" littlefs-partition "$PartitionsCsv"); then
      ShowFailure "Could not determine ESP32 LittleFS partition"
    fi
  fi

  PrepareMainBinDir
  for FirmwareFile in "${FirmwareFiles[@]}"; do
    OutputFile="$BinDir/$(basename "$FirmwareFile")"
    if "$DeviceTypeEnabled" && [ "$FirmwareFile" = "$ApplicationFirmware" ]; then
      if "$EnableSigning"; then
        OutputFile="$BinDir/$ProjectName.ino.bin_signed"
        install -m 0644 "$SignedApplicationFirmware" "$OutputFile"
      else
        install -m 0644 "$TypedApplicationFirmware" "$OutputFile"
      fi
    else
      install -m 0644 "$FirmwareFile" "$OutputFile"
    fi
    OutputFiles+=( "$OutputFile" )
  done

  if "$BuildTransitionArtifact"; then
    [ "$DeviceTypeEnabled" = true ] || ShowFailure "--Transition requires DeviceType.txt"
    [ "$EnableSigning" = true ] || ShowFailure "--Transition is only intended for signed ESP32 migration"
    install -m 0644 "$TypedApplicationFirmware" "$TransitionApplicationOutput"
    OutputFiles+=( "$TransitionApplicationOutput" )
  fi

  if "$BuildNoIdArtifacts"; then
    install -m 0644 "$ApplicationFirmware" "$NoIdApplicationOutput"
    OutputFiles+=( "$NoIdApplicationOutput" )
  fi

  for DeviceName in "${DeviceNames[@]}"; do
    DataDir="$(DeviceDataDir "$DeviceName")"
    DataReadableDir="$(DeviceReadableDir "$DeviceName")"

    if "$DeviceTypeEnabled" && "$EnableConfigRecoveryArtifact"; then
      ConfigSource="$DataDir/Config.json"
      ConfigTyped="$CompileDir/$DeviceName.Config.json.update"
      [ -f "$ConfigSource" ] || ShowFailure "Config recovery source not found: $ConfigSource"
      CreateDeviceTypeArtifact config "$ConfigSource" "$ConfigTyped"
      install -m 0644 "$ConfigTyped" "$BinDir/$DeviceName.Config.json.update"
      OutputFiles+=( "$BinDir/$DeviceName.Config.json.update" )
      IntermediateFiles+=( "$ConfigTyped" )
    fi

    if "$EnableLittleFs"; then
      LittleFs="$CompileDir/$DeviceName.ino.littlefs"
      if ! "$MkLittleFs" -c "$DataDir" -p 256 -b 4096 -s "$LittleFsSizeDetected" "$LittleFs" >>"$LogFile" 2>&1; then
        ShowFailure "ESP32 LittleFS image creation failed for $DeviceName"
      fi
      [ "$(stat -c '%s' "$LittleFs")" -eq "$LittleFsSizeDetected" ] || \
        ShowFailure "ESP32 LittleFS image has an unexpected size for $DeviceName"
      if "$DeviceTypeEnabled"; then
        TypedLittleFs="$CompileDir/$DeviceName.ino.littlefs.device"
        CreateDeviceTypeArtifact filesystem "$LittleFs" "$TypedLittleFs"
        IntermediateFiles+=( "$TypedLittleFs" )
        if "$EnableSigning"; then
          SignedLittleFs="$CompileDir/$DeviceName.ino.littlefs_signed"
          if ! python3 "$DeviceTypeHelper" sign \
            "$TypedLittleFs" "$PrivateKey" "$SignedLittleFs" >>"$LogFile" 2>&1 || \
             ! python3 "$DeviceTypeHelper" verify \
            "$SignedLittleFs" "$PublicKey" >>"$LogFile" 2>&1
          then
            ShowFailure "Could not sign/verify ESP32 LittleFS image for $DeviceName"
          fi
          install -m 0644 "$SignedLittleFs" "$BinDir/$(FsArtifactName "$DataDir/Config.json").ino.littlefs_signed"
          OutputFiles+=( "$BinDir/$(FsArtifactName "$DataDir/Config.json").ino.littlefs_signed" )
          IntermediateFiles+=( "$SignedLittleFs" )
          if "$BuildTransitionArtifact"; then
            install -m 0644 "$TypedLittleFs" "$BinDir/$DeviceName.Transition.ino.littlefs"
            OutputFiles+=( "$BinDir/$DeviceName.Transition.ino.littlefs" )
          fi
        else
          install -m 0644 "$TypedLittleFs" "$BinDir/$DeviceName.ino.littlefs"
          OutputFiles+=( "$BinDir/$DeviceName.ino.littlefs" )
        fi
      else
        install -m 0644 "$LittleFs" "$BinDir/$DeviceName.ino.littlefs"
        OutputFiles+=( "$BinDir/$DeviceName.ino.littlefs" )
      fi
      if ! python3 "$ReadableHelper" --strict-json "$DataDir" "$DataReadableDir" >>"$LogFile" 2>&1; then
        ShowFailure "Could not create readable LittleFS snapshot for $DeviceName"
      fi
    fi
  done

  rm -rf -- "${IntermediateFiles[@]}"
  ShowMemorySummary
  ShowWarnings

  echo
  echo "Firmware built once for project: $ProjectName"
  if "$EnableSigning"; then
    printf '  Firmware: %s/%s.ino.bin_signed\n' "$BinDir" "$ProjectName"
  else
    printf '  Firmware: %s/%s.ino.bin\n' "$BinDir" "$ProjectName"
  fi
  if "$BuildTransitionArtifact"; then
    printf '  Transition: %s/%s.Transition.ino.bin (legacy typed, one-time use only)\n' "$BinDir" "$ProjectName"
  fi
  if "$BuildNoIdArtifacts"; then
    printf '  Raw/NoId:   %s/%s.NoId.ino.bin\n' "$BinDir" "$ProjectName"
  fi
  for DeviceName in "${DeviceNames[@]}"; do
    echo "Device artifacts: $DeviceName"
    if "$EnableLittleFs"; then
      if "$EnableSigning"; then
        printf '  LittleFS: %s/%s.ino.littlefs_signed (flash offset %s)\n' "$BinDir" "$(FsArtifactName "$(DeviceDataDir "$DeviceName")/Config.json")" "$LittleFsOffset"
        if "$BuildTransitionArtifact"; then
          printf '  Transition FS: %s/%s.Transition.ino.littlefs (legacy typed, one-time use only)\n' "$BinDir" "$DeviceName"
        fi
      else
        printf '  LittleFS: %s/%s.ino.littlefs (flash offset %s)\n' "$BinDir" "$DeviceName" "$LittleFsOffset"
      fi
      printf '  Readable: %s\n' "$(DeviceReadableDir "$DeviceName")"
    fi
    if "$DeviceTypeEnabled" && "$EnableConfigRecoveryArtifact"; then
      printf '  Config:   %s/%s.Config.json.update\n' "$BinDir" "$DeviceName"
    fi
  done
}

CleanBuild()
{
  rm -rf -- "$BuildRoot"
  echo "Build cache removed: $BuildRoot"
}

FullClean()
{
  rm -rf -- "$BuildRoot"
  PrepareMainBinDir
  echo "Build cache removed: $BuildRoot"
  echo "Main binary output removed from: $BinDir"
}

case "$BuildMode" in
  clean-only)
    CleanBuild
    exit 0
    ;;
  full-clean-only)
    FullClean
    exit 0
    ;;
  clean-build)
    CleanBuild
    ;;
  incremental)
    ;;
  *)
    echo "Internal error: unknown build mode: $BuildMode" >&2
    exit 2
    ;;
esac

if "$ShowStatus"; then
  echo "Build mode:       $BuildMode"
  echo "Status heartbeat: every ${StatusInterval}s"
  echo "Project:          $ProjectDir"
  echo "Platform:         $Platform"
fi

# Preserve unchanged generated sources and compiler objects for incremental builds.
# Explicit clean modes remove the build root before reaching this point.
mkdir -p "$CompileDir" "$SketchDir" "$BinDir"
: >"$LogFile"

[ -f "$PreBuildScript" ] || ShowFailure "Pre-build script not found: $PreBuildScript"
[ -f "$PythonHelper" ] || ShowFailure "Python helper not found: $PythonHelper"
[ -x "$CLI" ] || ShowFailure "Arduino CLI wrapper not executable: $CLI"
command -v python3 >/dev/null 2>&1 || ShowFailure "python3 not found"

if "$EnableLittleFs"; then
  [ -f "$SecretsFile" ] || ShowFailure "Secrets file not found: $SecretsFile"
  [ -f "$ReadableHelper" ] || ShowFailure "Readable-snapshot helper not found: $ReadableHelper"
fi

if "$EnableSigning"; then
  command -v openssl >/dev/null 2>&1 || ShowFailure "openssl not found"
  [ -f "$PrivateKey" ] || ShowFailure "Private key not found: $PrivateKey"
  [ -f "$PublicKey" ] || ShowFailure "Public key not found: $PublicKey"
  if [ "$Platform" = "esp8266" ]; then
    [ -f "$Esp8266SigningHeaderHelper" ] || \
      ShowFailure "ESP8266 signing-header helper not found: $Esp8266SigningHeaderHelper"
  fi
fi
if "$EnableCompactUpdate" || "$EnableGzipArtifacts"; then
  command -v gzip >/dev/null 2>&1 || ShowFailure "gzip not found"
fi
if "$EnableCompactUpdate"; then
  [ -f "$LittleFsUpdateHelper" ] || ShowFailure "Compact LittleFS update helper not found: $LittleFsUpdateHelper"
  [ -f "$CombinedUpdateHelper" ] || ShowFailure "Combined update helper not found: $CombinedUpdateHelper"
fi

LoadDeviceNames
if "$EnableLittleFs" && [ -d "$DevStateRoot" ]; then
  python3 "$PrjsDir/tools/DeviceFsArtifactName.py" --check-directory "$DevStateRoot"
fi
for DeviceName in "${DeviceNames[@]}"; do
  DataDir="$(DeviceDataDir "$DeviceName")"
  if "$DeviceStateMode"; then
    if ! RunLongStepToFile \
      "$ProjectName pre-build for $DeviceName" \
      "$LogFile" \
      env PRJS_SECRETS_FILE="$SecretsFile" \
      PRJS_DEVICE_NAME="$DeviceName" \
      PRJS_DEVICE_STATE_DIR="$DevStateRoot/$DeviceName" \
      PRJS_DATA_DIR="$DataDir" \
      PRJS_BUILD_ROOT="$BuildRoot" \
      bash "$PreBuildScript"
    then
      ShowFailure "$ProjectName pre-build failed for $DeviceName"
    fi
  else
    if ! RunLongStepToFile \
      "$ProjectName pre-build" \
      "$LogFile" \
      env PRJS_SECRETS_FILE="$SecretsFile" bash "$PreBuildScript"
    then
      ShowFailure "$ProjectName pre-build failed"
    fi
  fi
  for RequiredDataFile in "${RequiredDataFiles[@]}"; do
    [ -f "$DataDir/$RequiredDataFile" ] || \
      ShowFailure "Pre-build did not generate $DeviceName/data/$RequiredDataFile"
  done
done
PrepareDeviceTypeFiles
for RequiredGeneratedFile in "${RequiredGeneratedFiles[@]}"; do
  [ -f "$SourceDir/$RequiredGeneratedFile" ] || \
    ShowFailure "Pre-build did not generate $RequiredGeneratedFile"
done
for RequiredProjectFile in "${RequiredProjectFiles[@]}"; do
  [ -f "$SourceDir/$RequiredProjectFile" ] || \
    ShowFailure "Required project file not found: $RequiredProjectFile"
done
echo "$PreBuildSuccessMessage"

if "$EnableSigning"; then
  VerifySigningKeys
fi

Status "Reading Arduino CLI configuration"
if ! CliConfig="$("$CLI" config dump --format json 2>>"$LogFile")"; then
  ShowFailure "Could not read Arduino CLI configuration"
fi
printf '%s\n' "$CliConfig" >>"$LogFile"
if ! ArduinoDataDir="$(printf '%s\n' "$CliConfig" | python3 "$PythonHelper" arduino-data-dir)"
then
  ShowFailure "Could not determine Arduino data directory"
fi

if UsesGnFtpServer; then
  PrepareSimpleFtpServer
fi

case "$Platform" in
  esp8266)
    SetupEsp8266Tools
    ;;
  esp32)
    SetupEsp32Tools
    ;;
  *)
    ShowFailure "Unsupported platform: $Platform"
    ;;
esac

Status "Refreshing generated sketch and build inputs"
CopySketchSources
if [ "$Platform" = "esp8266" ] && "$EnableSigning"; then
  GenerateEsp8266SigningHeader
elif [ "$Platform" = "esp32" ] && "$EnableSigning"; then
  if ! python3 "$DeviceTypeHelper" public-key-header \
    "$PublicKey" "$SketchDir/EspUpdatePublicKey.h" >>"$LogFile" 2>&1
  then
    ShowFailure "Could not generate ESP32 public-key header"
  fi
fi
RunArduinoCompile

Status "Creating output artifacts"
case "$Platform" in
  esp8266)
    if "$EnableSigning"; then
      BuildSignedEsp8266Outputs
    else
      ShowFailure "Unsigned ESP8266 common output path is not implemented"
    fi
    ;;
  esp32)
    BuildEsp32Outputs
    ;;
esac

if "$EnableLittleFs"; then
  source "$PrjsDir/tools/_PruneDeviceFsArtifacts.sh"
  PruneDeviceFsArtifacts "$BinDir" "$ProjectDir/DevState"
fi

VersionArgs=()
if "$EnableLittleFs"; then VersionArgs=( --filesystem --devices "${#DeviceNames[@]}" ); fi
python3 "$PrjsDir/tools/PrintBuiltVersions.py" "$ProjectDir" "${VersionArgs[@]}"
