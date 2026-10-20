#!/usr/bin/env bash
# DownloadLittleFS.sh, Version: 1.04

set -Eeuo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
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
export PRJS_INSTANCES_DIR="$ProjectDir/generated/Instances"
bash "$PrjsDir/tools/DownloadLittleFS.sh" "$@"
RestoreDir
