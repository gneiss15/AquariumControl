#!/usr/bin/env bash
# EspCompile.sh, Version: 1.29

set -euo pipefail

#COMPILE_VERBOSE=--verbose
COMPILE_VERBOSE=

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
bash "$PrjsDir/Common/Feeder/GenerateJs.sh"
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

if [ ! -f "$BoardConfig" ]; then
  echo "Error: Board configuration not found: $BoardConfig" >&2
  exit 1
fi
# shellcheck source=/dev/null
source "$BoardConfig"

: "${FQBN:?FQBN is not configured in $BoardConfig}"
: "${BoardId:?BoardId is not configured in $BoardConfig}"
: "${LittleFsSize:?LittleFsSize is not configured in $BoardConfig}"
: "${LittleFsBlockSize:?LittleFsBlockSize is not configured in $BoardConfig}"
: "${LittleFsPageSize:?LittleFsPageSize is not configured in $BoardConfig}"

ProjectName="FeederSimple"
Platform="esp8266"
BinSubDir="${BoardId}_Arduino2"

PRJS_BUILD_ROOT="$ProjectDir/generated/build/main"
PRJS_INSTANCES_DIR="$ProjectDir/generated/Instances"
PRJS_RESCUE_BIN_DIR="$ProjectDir/generated/bin/$BinSubDir"
PRJS_PRESERVE_SHARED_BIN_ARTIFACTS=true

EnableSigning=true
EnableLittleFs=true
EnableCompactUpdate=true
EnableGzipArtifacts=true
ShowRescueReserve=false

RequiredDataFiles=( Config.json ConfigEntries.json FeedTimes.json Versions.txt )
RequiredGeneratedFiles=( Version.inc.h )
PreBuildSuccessMessage="Pre-build: configuration JSON and revision updated"

ArduinoLibraries=(
  "$PrjsDir/Libs/GnBasicLib"
  "$PrjsDir/Libs/GnAqPrjsLib"
  "$PrjsDir/Libs/GnFtpServer"
)
ExtraBuildProperties=( "compiler.cpp.extra_flags=-DGN_FEEDER_UDP=1 -DGN_FTP_FIXED_BUFFER -DFTP_BUF_SIZE=512 -DARDUINOJSON_POOL_CAPACITY=32 -I@SKETCH_DIR@ -I$PrjsDir" "build.float=" "compiler.c.elf.extra_flags=-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=pvPortMalloc,--wrap=sdk3_pvPortMalloc,--wrap=pvPortRealloc,--wrap=pvPortCalloc,--wrap=pvPortCallocIram,--wrap=pvPortZalloc,--wrap=pvPortZallocIram" )
CommonSourceDirs=( Common/Feeder Common/EspUpdate )

# shellcheck source=../tools/EspCompileCommon.sh
source "$PrjsDir/tools/EspCompileCommon.sh"
RestoreDir
