#!/usr/bin/env bash
# EspRescueCompile.sh, Version: 1.03

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
# shellcheck source=/dev/null
source "$BoardConfig"
ConfigDefinition="$ProjectDir/Sw/FeederConfig.inc.h"
PRJS_RESCUE_BUILD_DIR="$ProjectDir/generated/build/rescue"
PRJS_RESCUE_BIN_DIR="$ProjectDir/generated/bin/$BinSubDir"
PRJS_PRESERVE_SHARED_BIN_ARTIFACTS=true

ArduinoLibraries=( "$PrjsDir/Libs/GnBasicLib" )
ExtraBuildProperties=( "build.float=" )

# shellcheck source=../tools/EspRescueCompileCommon.sh
source "$PrjsDir/tools/EspRescueCompileCommon.sh"
RestoreDir
