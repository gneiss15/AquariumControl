#!/usr/bin/env bash
# EspRescueCompile.sh, Version: 1.04
set -Eeuo pipefail
ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
source "$ProjectDir/EspBoardConfig.sh"
source "$PrjsDir/tools/_TrashPaths.sh"
BuildRoot="$ProjectDir/generated/build/rescue"
BinDir="$ProjectDir/generated/bin/$BinSubDir"
BuildMode=incremental
for Arg in "$@"; do
  case "$Arg" in
    --build|--clean|--full-clean)
      [ "$BuildMode" = incremental ] || { echo 'Only one build mode may be specified.' >&2; exit 2; }
      BuildMode="$Arg" ;;
    --help|-h) echo 'Usage: EspRescueCompile.sh [--build | --clean | --full-clean]'; exit 0 ;;
    *) echo "Unknown option: $Arg" >&2; exit 2 ;;
  esac
done
if [ "$BuildMode" != incremental ]; then rm -rf -- "$BuildRoot"; fi
if [ "$BuildMode" = --full-clean ]; then
  rm -rf -- "$BinDir/TmrSwShelly.Rescue.rescue_signed" \
    "$BinDir/TmrSwShelly.Rescue.ino.bin_signed"
fi
case "$BuildMode" in --clean|--full-clean) exit 0 ;; esac
bash "$ProjectDir/Sw/Rescue/BuildRescue.sh"
