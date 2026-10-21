#!/usr/bin/env bash
# EspRescueCompile.sh, Version: 1.06
set -euo pipefail

COMPILE_VERBOSE=
ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
DirPushed=0
RestoreDir(){ if (( DirPushed )); then popd >/dev/null 2>&1 || true; DirPushed=0; fi; }
trap RestoreDir EXIT
pushd "$ProjectDir" >/dev/null
DirPushed=1

ProjectName="TmrSwA8T"
# shellcheck source=EspBoardConfig.sh
source "$ProjectDir/EspBoardConfig.sh"
PRJS_RESCUE_BUILD_DIR="$ProjectDir/generated/build/rescue"
PRJS_RESCUE_BIN_DIR="$ProjectDir/generated/bin/$BinSubDir"
PRJS_PRESERVE_SHARED_BIN_ARTIFACTS=true
ConfigDefinition="$ProjectDir/Sw/TmrSwConfig.inc.h"
RescueHardwareDefinition="$ProjectDir/Sw/TmrSwA8THardware.h"
ExtraBuildProperties=( "compiler.cpp.extra_flags=-I$ProjectDir/Sw" )

RescueTestNode=false
FilteredArgs=()
for Arg in "$@"; do
  if [ "$Arg" = "--TestNode" ]; then
    RescueTestNode=true
  else
    FilteredArgs+=( "$Arg" )
  fi
done
set -- "${FilteredArgs[@]}"

if $RescueTestNode; then
  ExtraBuildProperties=(
    "compiler.cpp.extra_flags=-I$ProjectDir/Sw -DTmrSwA8THardwareSelect=TmrSwA8THardware_TestNode"
  )
fi

# shellcheck source=../Libs/Common/EspRescue/_EspRescueCompileEsp32Common.sh
source "$PrjsDir/Libs/Common/EspRescue/_EspRescueCompileEsp32Common.sh"
RestoreDir
