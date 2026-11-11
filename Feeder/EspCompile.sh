#!/usr/bin/env bash
# EspCompile.sh, Version: 2.62

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
  "$PrjsDir/Libs/GnAqPrjsLib"
  "$PrjsDir/Libs/GnFtpServer"
)
OledBuildFlag=""
if [[ "${FEEDER_NO_OLED:-false}" == true ]]; then
  OledBuildFlag="-DFEEDER_NO_OLED"
  BinSubDir="$BinSubDir-NoOled"
  PRJS_BUILD_ROOT="$ProjectDir/generated/build/no-oled"
  PRJS_INSTANCES_DIR="$ProjectDir/generated/Instances-NoOled"
else
  ArduinoLibraries+=( "$PrjsDir/Libs/GnWifiKit8Lib" )
fi
# Smaller JSON allocator blocks reduce peak RAM for concurrently used documents.
ExtraBuildProperties=( "compiler.cpp.extra_flags=-DGN_FEEDER_UDP=1 -DGN_FTP_FIXED_BUFFER -DFTP_BUF_SIZE=512 -DARDUINOJSON_POOL_CAPACITY=32 $OledBuildFlag -I@SKETCH_DIR@ -I$PrjsDir" "build.float=" "compiler.c.elf.extra_flags=-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=pvPortMalloc,--wrap=sdk3_pvPortMalloc,--wrap=pvPortRealloc,--wrap=pvPortCalloc,--wrap=pvPortCallocIram,--wrap=pvPortZalloc,--wrap=pvPortZallocIram" )
CommonSourceDirs=( Common/Feeder Common/EspUpdate )

# shellcheck source=../tools/EspCompileCommon.sh
source "$PrjsDir/tools/EspCompileCommon.sh"
RestoreDir
