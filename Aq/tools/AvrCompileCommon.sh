# AvrCompileCommon.sh, Version: 1.05
# Common implementation sourced by project-specific AvrCompile.sh scripts.
# Do not execute this file directly.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "AvrCompileCommon.sh must be sourced by a project AvrCompile.sh." >&2
  exit 2
fi

source "$PrjsDir/tools/_TrashPaths.sh"

: "${ProjectDir:?ProjectDir not set}"
: "${PrjsDir:?PrjsDir not set}"
: "${ProjectName:?ProjectName not set}"
: "${FQBN:?FQBN not set}"
: "${BinSubDir:?BinSubDir not set}"

COMPILE_VERBOSE="${COMPILE_VERBOSE:-}"
BuildMode="incremental"

BuildRoot="$ProjectDir/generated/build"
CompileDir="$BuildRoot/avr"
SketchRoot="$BuildRoot/sketch"
SketchDir="$SketchRoot/$ProjectName"
LibraryRoot="$BuildRoot/libraries"
GnBasicLibBuildDir="$LibraryRoot/GnBasicLib"
BinDir="$ProjectDir/generated/bin/$BinSubDir"
LogFile="$CompileDir/Compile.log"
MountDir="$(dirname -- "$(dirname -- "$PrjsDir")")"
CLI="${ARDUINO_CLI_WRAPPER:-${ARDUINO2CLI_START:-$MountDir/Start_Arduino2cli.sh}}"
GnBasicLibDir="${GnBasicLibDir:-$PrjsDir/Libs/GnBasicLib}"

ShowUsage()
{
  cat <<EOF2
Usage: $(basename -- "$0") [--build | --clean | --full-clean]

Without a build-mode option, an incremental build is performed.

Options:
  --build       clean compiler/build cache, then build
  --clean       remove compiler/build cache and exit; keep generated/bin
  --full-clean  remove compiler/build cache and generated/bin, then exit
  --help, -h    show this help
EOF2
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

UnexpectedError()
{
  local ExitCode=$?
  local LineNr="$1"

  trap - ERR
  if declare -F RestoreDir >/dev/null; then
    RestoreDir
  fi
  echo "Unexpected error in AvrCompile.sh at line $LineNr" >&2
  if [ -f "$LogFile" ]; then
    echo >&2
    echo "Relevant end of $LogFile:" >&2
    tail -n 80 "$LogFile" >&2 || true
  fi
  PauseOnError
  exit "$ExitCode"
}

trap 'UnexpectedError "$LINENO"' ERR

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
      'Sketch uses|Global variables use|Der Sketch verwendet|Globale Variablen verwenden|Program:|Data:' \
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
  local SourceFile InputStage
  InputStage="$(mktemp -d "$SketchRoot/.inputs.XXXXXX")"
  while IFS= read -r -d '' SourceFile; do
    cp -p "$SourceFile" "$InputStage/$(basename "$SourceFile")"
  done < <(
    find "$ProjectDir" -maxdepth 1 -type f \
      \( -name '*.ino' -o -name '*.cpp' -o -name '*.c' -o -name '*.h' \
         -o -name '*.hpp' -o -name '*.S' \) -print0
  )
  python3 "$PrjsDir/tools/SyncGeneratedFiles.py" tree "$InputStage" "$SketchDir"
  rm -rf -- "$InputStage"
}

CreateAvrGnBasicLib()
{
  local SourceFile InputStage
  mkdir -p "$LibraryRoot"
  InputStage="$(mktemp -d "$LibraryRoot/.inputs.XXXXXX")"
  for SourceFile in GnPlatform.h GnBasics.h GnBasicsAVR.h Config.h library.properties; do
    [ -f "$GnBasicLibDir/$SourceFile" ] || ShowFailure "GnBasicLib file not found: $SourceFile"
    cp -p "$GnBasicLibDir/$SourceFile" "$InputStage/$SourceFile"
  done
  python3 "$PrjsDir/tools/SyncGeneratedFiles.py" tree "$InputStage" "$GnBasicLibBuildDir"
  rm -rf -- "$InputStage"
}

RunArduinoCompile()
{
  local -a CompileArgs
  local CompileOutput

  CompileArgs=( compile )
  if [ -n "$COMPILE_VERBOSE" ]; then
    CompileArgs+=( "$COMPILE_VERBOSE" )
  fi
  CompileArgs+=(
    --no-color
    --fqbn "$FQBN"
    --build-path "$CompileDir"
    --library "$GnBasicLibBuildDir"
    "$SketchDir"
  )

  CompileOutput="$(mktemp "${TMPDIR:-/tmp}/${ProjectName}.ArduinoCompile.XXXXXX.out")" || \
    ShowFailure "Could not create temporary compiler-output file"

  if ! "$CLI" "${CompileArgs[@]}" >"$CompileOutput" 2>&1; then
    cat "$CompileOutput" >>"$LogFile"
    trap - ERR
    echo "Error: AVR build failed" >&2
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

InstallOutputs()
{
  local Firmware="$CompileDir/$ProjectName.ino.hex"
  local OutputFile="$BinDir/$ProjectName.ino.hex"

  [ -f "$Firmware" ] || ShowFailure "Firmware HEX not generated: $Firmware"

  rm -rf -- "$BinDir/$ProjectName.ino"*
  install -m 0644 "$Firmware" "$OutputFile" || ShowFailure "Could not copy firmware HEX"

  ShowMemorySummary
  ShowWarnings

  echo
  echo "Output file:"
  printf '  %s (%s bytes)\n' "$OutputFile" "$(stat -c '%s' "$OutputFile")"
}

CleanBuild()
{
  rm -rf -- "$BuildRoot"
  echo "Build cache removed: $BuildRoot"
}

FullClean()
{
  rm -rf -- "$BuildRoot" "$BinDir"
  echo "Build cache removed: $BuildRoot"
  echo "Binary output removed: $BinDir"
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

mkdir -p "$CompileDir" "$SketchDir" "$BinDir"
: >"$LogFile"

[ -x "$CLI" ] || ShowFailure "Arduino CLI wrapper not executable: $CLI"
[ -d "$GnBasicLibDir" ] || ShowFailure "GnBasicLib directory not found: $GnBasicLibDir"

CopySketchSources
CreateAvrGnBasicLib
RunArduinoCompile
InstallOutputs
