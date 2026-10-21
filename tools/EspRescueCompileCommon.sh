# EspRescueCompileCommon.sh, Version: 1.17
# Common implementation sourced by project-specific EspRescueCompile.sh scripts.
# Do not execute this file directly.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "EspRescueCompileCommon.sh must be sourced by a project EspRescueCompile.sh." >&2
  exit 2
fi

: "${ProjectDir:?ProjectDir not set}"
: "${PrjsDir:?PrjsDir not set}"
: "${ProjectName:?ProjectName not set}"
: "${FQBN:?FQBN not set}"
: "${ConfigDefinition:?ConfigDefinition not set}"

COMPILE_VERBOSE="${COMPILE_VERBOSE:-}"
declare -p ArduinoLibraries >/dev/null 2>&1 || ArduinoLibraries=()
declare -p ExtraBuildProperties >/dev/null 2>&1 || ExtraBuildProperties=( "build.float=" )

MountDir="$(dirname -- "$(dirname -- "$PrjsDir")")"
CLI="${ARDUINO_CLI_WRAPPER:-${ARDUINO2CLI_START:-$MountDir/Start_Arduino2cli.sh}}"

BuildDir="${PRJS_RESCUE_BUILD_DIR:-$ProjectDir/rescue/build}"
BinDir="${PRJS_RESCUE_BIN_DIR:-$ProjectDir/rescue/bin}"
PreserveSharedBinArtifacts="${PRJS_PRESERVE_SHARED_BIN_ARTIFACTS:-false}"
CompileDir="$BuildDir/esp8266"
SketchDir="$BuildDir/sketch/EspRescueUpdate"
LogFile="$BuildDir/Compile.log"

PrjsPrivateDir="${PRJS_PRIVATE_DIR:-$(dirname -- "$(dirname -- "$PrjsDir")")/PrjsPrivate}"
PrivateKey="${PrivateKey:-$PrjsPrivateDir/private.key}"
PublicKey="${PublicKey:-$PrjsPrivateDir/public.key}"
PythonHelper="${PythonHelper:-$ProjectDir/EspCompile.py}"
RescueSource="${RescueSource:-$PrjsDir/EspRescueUpdate/Sw/EspRescueUpdate.ino}"
RescueConfigHelper="${RescueConfigHelper:-$PrjsDir/EspRescueUpdate/EspRescueConfig.py}"
RescueHardwareDefinition="${RescueHardwareDefinition:-$PrjsDir/EspRescueUpdate/Sw/EspRescueHardware.h}"
RescueSigningHelper="${RescueSigningHelper:-$PrjsDir/Libs/Common/EspUpdate/EspUpdateSigning.py}"
UpdateCommonDir="${UpdateCommonDir:-$PrjsDir/Libs/Common/EspUpdate}"
DeviceTypeHelper="${DeviceTypeHelper:-$PrjsDir/tools/DeviceTypeArtifacts.py}"
DeviceTypeFile="${DeviceTypeFile:-$ProjectDir/DeviceType.txt}"
SecretsFile="${PRJS_SECRETS_FILE:-${SecretsFile:-$PrjsPrivateDir/Secrets.local.json}}"

BuildMode="incremental"
BuildNoIdArtifacts=false

ShowUsage()
{
  cat <<EOF
Usage: $(basename -- "$0") [--build | --clean | --full-clean] [--NoId]

Without a build-mode option, an incremental rescue build is performed.

Options:
  --build       clean rescue compiler/build cache, then build
  --clean       remove rescue compiler/build cache and exit; keep rescue artifacts
  --full-clean  remove rescue compiler/build cache and rescue binary artifacts
  --NoId        additionally create a one-time legacy-format rescue artifact
  --help, -h    show this help
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
  echo "Unexpected error in EspRescueCompile.sh at line $LineNr" >&2
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

ShowMemorySummary()
{
  local MemorySummary

  MemorySummary="$(
    grep -E \
      'Sketch uses|Global variables use|Der Sketch verwendet|Globale Variablen verwenden|^[[:space:]]*(IROM|IRAM|RODATA|DATA)[[:space:]]*:' \
      "$LogFile" \
      || true
  )"
  if [ -n "$MemorySummary" ]; then
    echo
    echo "Memory summary:"
    printf '%s\n' "$MemorySummary"
  fi
}

CleanBuild()
{
  rm -rf "$BuildDir"
  echo "Rescue build cache removed: $BuildDir"
}

FullClean()
{
  rm -rf "$BuildDir"
  if "$PreserveSharedBinArtifacts"; then
    find "$BinDir" -mindepth 1 -maxdepth 1 -type f \
      \( -name "$ProjectName.Rescue.ino*" \
      -o -name "$ProjectName.NoId.Rescue.ino*" \) -delete 2>/dev/null || true
  else
    rm -rf "$BinDir"
  fi
  echo "Rescue build cache removed: $BuildDir"
  echo "Rescue binary output removed: $BinDir"
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
    echo "Internal error: unknown rescue build mode: $BuildMode" >&2
    exit 2
    ;;
esac

# Refresh only the generated rescue sketch. Keep the compiler path for
# incremental builds.
mkdir -p "$CompileDir" "$SketchDir" "$BinDir"
: >"$LogFile"

[ -x "$CLI" ] || ShowFailure "Arduino CLI wrapper not executable: $CLI"
[ -f "$PrivateKey" ] || ShowFailure "Private key not found: $PrivateKey"
[ -f "$PublicKey" ] || ShowFailure "Public key not found: $PublicKey"
[ -f "$PythonHelper" ] || ShowFailure "Python helper not found: $PythonHelper"
[ -f "$RescueSource" ] || ShowFailure "Rescue source not found: $RescueSource"
[ -f "$RescueConfigHelper" ] || ShowFailure "Rescue config helper not found: $RescueConfigHelper"
[ -f "$RescueHardwareDefinition" ] || ShowFailure "Rescue hardware definition not found: $RescueHardwareDefinition"
[ -f "$RescueSigningHelper" ] || ShowFailure "Rescue signing helper not found: $RescueSigningHelper"
[ -f "$ConfigDefinition" ] || ShowFailure "Config definition not found: $ConfigDefinition"
[ -f "$UpdateCommonDir/EspWebUpdate.h" ] || ShowFailure "EspWebUpdate.h not found"
[ -f "$UpdateCommonDir/EspWebUpdate.cpp" ] || ShowFailure "EspWebUpdate.cpp not found"
[ -f "$DeviceTypeHelper" ] || ShowFailure "DeviceType helper not found: $DeviceTypeHelper"
[ -f "$DeviceTypeFile" ] || ShowFailure "DeviceType file not found: $DeviceTypeFile"
command -v python3 >/dev/null 2>&1 || ShowFailure "python3 not found"
command -v openssl >/dev/null 2>&1 || ShowFailure "openssl not found"
command -v gzip >/dev/null 2>&1 || ShowFailure "gzip not found"

if ! openssl pkey -in "$PrivateKey" -check -noout >>"$LogFile" 2>&1; then
  ShowFailure "Invalid private key: $PrivateKey"
fi
if ! openssl pkey -pubin -in "$PublicKey" -noout >>"$LogFile" 2>&1; then
  ShowFailure "Invalid public key: $PublicKey"
fi
if ! openssl pkey -in "$PrivateKey" -pubout -outform DER \
  -out "$BuildDir/private-public.der" >>"$LogFile" 2>&1
then
  ShowFailure "Could not derive public key from private key"
fi
if ! openssl pkey -pubin -in "$PublicKey" -outform DER \
  -out "$BuildDir/public.der" >>"$LogFile" 2>&1
then
  ShowFailure "Could not normalize public key"
fi
if ! cmp -s "$BuildDir/private-public.der" "$BuildDir/public.der"; then
  ShowFailure "private.key and public.key do not form a key pair"
fi
rm -f "$BuildDir/private-public.der" "$BuildDir/public.der"

if ! CliConfig="$("$CLI" config dump --format json 2>>"$LogFile")"; then
  ShowFailure "Could not read Arduino CLI configuration"
fi
printf '%s\n' "$CliConfig" >>"$LogFile"
if ! ArduinoDataDir="$(printf '%s\n' "$CliConfig" | python3 "$PythonHelper" arduino-data-dir)"
then
  ShowFailure "Could not determine Arduino data directory"
fi

Esp8266HardwareDir="$ArduinoDataDir/packages/esp8266/hardware/esp8266"
Esp8266PythonDir="$ArduinoDataDir/packages/esp8266/tools/python3"
[ -d "$Esp8266HardwareDir" ] || ShowFailure "ESP8266 hardware directory not found"

Esp8266CoreDir="$(
  find "$Esp8266HardwareDir" \
    -mindepth 1 -maxdepth 1 -type d -print 2>>"$LogFile" \
    | sort -V \
    | tail -n 1
)"
[ -n "$Esp8266CoreDir" ] || ShowFailure "ESP8266 core not found"
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

python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$RescueSource" "$SketchDir/EspRescueUpdate.ino"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$RescueHardwareDefinition" "$SketchDir/EspRescueHardware.h"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$UpdateCommonDir/EspWebUpdate.h" "$SketchDir/EspWebUpdate.h"
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" file "$UpdateCommonDir/EspWebUpdate.cpp" "$SketchDir/EspWebUpdate.cpp"
python3 "$DeviceTypeHelper" prepare \
  "$DeviceTypeFile" \
  "$SketchDir/DeviceType.inc.h" \
  "$BuildDir/DeviceType.txt"
python3 "$RescueConfigHelper" \
  "$ConfigDefinition" \
  "$SecretsFile" \
  "$ProjectName" \
  "$SketchDir/EspRescueProject.h"
python3 "$RescueSigningHelper" \
  "$PublicKey" \
  "$SketchDir/EspUpdateSigning.h"

CompileArgs=(
  compile
  --no-color
  --fqbn "$FQBN"
  --build-path "$CompileDir"
)
if [ -n "$COMPILE_VERBOSE" ]; then
  CompileArgs+=( "$COMPILE_VERBOSE" )
fi
for BuildProperty in "${ExtraBuildProperties[@]}"; do
  CompileArgs+=( --build-property "$BuildProperty" )
done
# IMPORTANT: keep project libraries exact. Never collapse them to their parent
# directory via --libraries; that reintroduces duplicate library discovery.
for Library in "${ArduinoLibraries[@]}"; do
  [ -d "$Library" ] || ShowFailure "Arduino library not found: $Library"
  CompileArgs+=( --library "$Library" )
done

if ! "$CLI" "${CompileArgs[@]}" "$SketchDir" >>"$LogFile" 2>&1; then
  ShowFailure "ESP8266 rescue build failed"
fi

Firmware="$CompileDir/EspRescueUpdate.ino.bin"
FirmwareTyped="$CompileDir/EspRescueUpdate.ino.bin.device"
FirmwareSigned="$CompileDir/EspRescueUpdate.ino.bin.signed"
FirmwareNoIdSigned="$CompileDir/EspRescueUpdate.NoId.ino.bin.signed"
FirmwareGz="$CompileDir/EspRescueUpdate.ino.bin.gz"
FirmwareGzTyped="$CompileDir/EspRescueUpdate.ino.bin.gz.device"
FirmwareGzSigned="$CompileDir/EspRescueUpdate.ino.bin.gz.signed"
FirmwareGzNoIdSigned="$CompileDir/EspRescueUpdate.NoId.ino.bin.gz.signed"
[ -f "$Firmware" ] || ShowFailure "Rescue firmware binary not generated: $Firmware"

SignAndVerifyRescueFile()
{
  local SourceFile="$1"
  local SignedFile="$2"
  local VerifyPrefix="$BuildDir/.verify.$(basename "$SignedFile")"
  local VerifyPayload="$VerifyPrefix.payload"
  local VerifySignature="$VerifyPrefix.signature"

  if ! "$SigningPython" "$SigningTool" \
    --mode sign \
    --privatekey "$PrivateKey" \
    --bin "$SourceFile" \
    --out "$SignedFile" >>"$LogFile" 2>&1
  then
    ShowFailure "Rescue firmware signing failed: $SourceFile"
  fi
  [ -f "$SignedFile" ] || ShowFailure "Signed rescue firmware not generated: $SignedFile"

  if ! python3 "$PythonHelper" extract-signature \
    "$SourceFile" "$SignedFile" "$VerifyPayload" "$VerifySignature" \
    >>"$LogFile" 2>&1
  then
    ShowFailure "Rescue signed-file structure check failed: $SignedFile"
  fi

  if ! openssl dgst -sha256 -verify "$PublicKey" -signature "$VerifySignature" \
    "$VerifyPayload" >>"$LogFile" 2>&1
  then
    ShowFailure "Rescue signature verification failed: $SignedFile"
  fi

  rm -f "$VerifyPayload" "$VerifySignature"
}

if ! python3 "$DeviceTypeHelper" wrap \
  "$DeviceTypeFile" firmware "$Firmware" "$FirmwareTyped" >>"$LogFile" 2>&1
then
  ShowFailure "Could not create typed rescue firmware"
fi
SignAndVerifyRescueFile "$FirmwareTyped" "$FirmwareSigned"

if "$BuildNoIdArtifacts"; then
  SignAndVerifyRescueFile "$Firmware" "$FirmwareNoIdSigned"
fi

if ! gzip -n -9 -c "$Firmware" >"$FirmwareGz"; then
  ShowFailure "Rescue firmware compression failed"
fi
gzip -t "$FirmwareGz" >>"$LogFile" 2>&1 || ShowFailure "Rescue firmware gzip check failed"
if ! python3 "$DeviceTypeHelper" wrap \
  "$DeviceTypeFile" firmware "$FirmwareGz" "$FirmwareGzTyped" >>"$LogFile" 2>&1
then
  ShowFailure "Could not create typed compressed rescue firmware"
fi
SignAndVerifyRescueFile "$FirmwareGzTyped" "$FirmwareGzSigned"

if "$BuildNoIdArtifacts"; then
  SignAndVerifyRescueFile "$FirmwareGz" "$FirmwareGzNoIdSigned"
fi

OutputFirmware="$BinDir/$ProjectName.Rescue.ino.bin"
OutputFirmwareSigned="$BinDir/$ProjectName.Rescue.ino.bin.signed"
OutputFirmwareGz="$BinDir/$ProjectName.Rescue.ino.bin.gz"
OutputFirmwareGzSigned="$BinDir/$ProjectName.Rescue.ino.bin.gz.signed"
OutputFirmwareNoIdSigned="$BinDir/$ProjectName.NoId.Rescue.ino.bin.signed"
OutputFirmwareGzNoIdSigned="$BinDir/$ProjectName.NoId.Rescue.ino.bin.gz.signed"
rm -f "$BinDir/$ProjectName.Rescue.ino"* "$BinDir/$ProjectName.NoId.Rescue.ino"*
install -m 0644 "$Firmware" "$OutputFirmware"
install -m 0644 "$FirmwareSigned" "$OutputFirmwareSigned"
install -m 0644 "$FirmwareGz" "$OutputFirmwareGz"
install -m 0644 "$FirmwareGzSigned" "$OutputFirmwareGzSigned"
if "$BuildNoIdArtifacts"; then
  install -m 0644 "$FirmwareNoIdSigned" "$OutputFirmwareNoIdSigned"
  install -m 0644 "$FirmwareGzNoIdSigned" "$OutputFirmwareGzNoIdSigned"
fi
rm -f "$FirmwareTyped" "$FirmwareGzTyped"

ShowMemorySummary

echo
echo "Rescue firmware:"
printf '  %s (%s bytes)\n' "$OutputFirmwareSigned" "$(stat -c '%s' "$OutputFirmwareSigned")"
printf '  %s (%s bytes)\n' "$OutputFirmwareGzSigned" "$(stat -c '%s' "$OutputFirmwareGzSigned")"
if "$BuildNoIdArtifacts"; then
  echo
  echo "One-time NoId rescue artifacts:"
  printf '  %s (%s bytes)\n' "$OutputFirmwareNoIdSigned" "$(stat -c '%s' "$OutputFirmwareNoIdSigned")"
  printf '  %s (%s bytes)\n' "$OutputFirmwareGzNoIdSigned" "$(stat -c '%s' "$OutputFirmwareGzNoIdSigned")"
fi
echo
echo "Recommended upload: the smaller .bin.gz.signed rescue firmware."
