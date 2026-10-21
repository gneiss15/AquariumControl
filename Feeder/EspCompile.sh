#!/usr/bin/env bash
# EspCompile.sh, Version: 2.54

set -euo pipefail

#COMPILE_VERBOSE=--verbose
COMPILE_VERBOSE=

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
BoardConfig="$ProjectDir/EspBoardConfig.sh"
DirPushed=0

RestoreDir()
{
  if (( DirPushed )); then
    popd >/dev/null 2>&1 || true
    DirPushed=0
  fi
}

trap RestoreDir EXIT

pushd "$ProjectDir" >/dev/null
DirPushed=1

ProjectName="Feeder"
Platform="esp8266"
# shellcheck source=/dev/null
source "$BoardConfig"

PRJS_BUILD_ROOT="$ProjectDir/generated/build/main"
PRJS_INSTANCES_DIR="$ProjectDir/generated/Instances"
PRJS_RESCUE_BIN_DIR="$ProjectDir/generated/bin/$BinSubDir"
PRJS_PRESERVE_SHARED_BIN_ARTIFACTS=true

EnableSigning=true
EnableLittleFs=true
EnableCompactUpdate=true
EnableGzipArtifacts=true
ShowRescueReserve=false

RequiredDataFiles=( Config.json ConfigEntries.json FeedTimes.json ServoSteps.json Versions.txt )
RequiredGeneratedFiles=( Version.inc.h )
PreBuildSuccessMessage="Pre-build: configuration JSON and revision updated"

ArduinoLibraries=(
  "$PrjsDir/Libs/GnBasicLib"
  "$PrjsDir/Libs/GnFtpServer"
  "$PrjsDir/Libs/GnWifiKit8Lib"
)
ExtraBuildProperties=( "compiler.cpp.extra_flags=-I@SKETCH_DIR@ -I$PrjsDir" "build.float=" )
CommonSourceDirs=( Libs/Common/Feeder Libs/Common/EspUpdate )

# shellcheck source=../tools/EspCompileCommon.sh
source "$PrjsDir/tools/EspCompileCommon.sh"
RestoreDir
