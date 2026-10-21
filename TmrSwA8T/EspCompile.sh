#!/usr/bin/env bash
# EspCompile.sh, Version: 1.07
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
Platform="esp32"
CompileBackend="idf"
# shellcheck source=EspBoardConfig.sh
source "$ProjectDir/EspBoardConfig.sh"
PRJS_BUILD_ROOT="$ProjectDir/generated/build/main"
PRJS_INSTANCES_DIR="$ProjectDir/generated/Instances"
PRJS_RESCUE_BIN_DIR="$ProjectDir/generated/bin/$BinSubDir"
PRJS_PRESERVE_SHARED_BIN_ARTIFACTS=true

EnableSigning=true
EnableLittleFs="${TMR_SW_ENABLE_LITTLEFS:-true}"
EnableCompactUpdate=false
EnableGzipArtifacts=false
EnableConfigRecoveryArtifact=false
ShowRescueReserve=false
CustomPartitionsFile="$PartitionDefinition"
MaxApplicationSize="$MainPartitionSize"

RequiredDataFiles=( Channels.json Config.json ConfigEntries.json Index.html Intervals.json SwitchTimes.json Translations.json Versions.txt )
RequiredGeneratedFiles=( Version.inc.h )
RequiredProjectFiles=( TmrSwConfig.inc.h TmrSwA8THardware.h TmrSwProject.h )
PreBuildSuccessMessage="Pre-build: A8T web/config files and revision updated"

ArduinoLibraries=(
  "$PrjsDir/Libs/GnBasicLib"
  "$PrjsDir/Libs/GnFtpServer"
)
ExtraBuildProperties=(
  "compiler.cpp.extra_flags=-I@SKETCH_DIR@"
  "upload.maximum_size=$MaxApplicationSize"
)
CommonSourceDirs=( Libs/Common/EspUpdate Libs/Common/TmrSw )

# shellcheck source=../tools/EspCompileCommon.sh
source "$PrjsDir/tools/EspCompileCommon.sh"
RestoreDir
