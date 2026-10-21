#!/usr/bin/env bash
# PreBuild.sh, Version: 1.10

set -Eeuo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
# shellcheck source=../tools/_TrashPaths.sh
source "$PrjsDir/tools/_TrashPaths.sh"

WebSrcDir="$ProjectDir/WebSrc"
DataDir="$ProjectDir/data"
FinalDataDir="$DataDir"
mkdir -p -- "$(dirname -- "$FinalDataDir")"
DataDir="$(mktemp -d "${FinalDataDir}.stage.XXXXXX")"
StagingDataDir="$DataDir"
CleanupPreBuild()
{
  if declare -F RestoreDir >/dev/null; then RestoreDir; fi
  [ -z "$StagingDataDir" ] || rm -rf -- "$StagingDataDir"
}
trap CleanupPreBuild EXIT

[ -d "$WebSrcDir" ] || { echo "Web source directory not found: $WebSrcDir" >&2; exit 1; }
[ -f "$PrjsDir/tools/ValidateJsonFiles.py" ] || { echo "JSON validator not found: $PrjsDir/tools/ValidateJsonFiles.py" >&2; exit 1; }
python3 "$PrjsDir/tools/UpdateVersionFiles.py" "$ProjectDir"

JsonValidationTargets=( "$WebSrcDir" "$ProjectDir/Config.json" )
python3 "$PrjsDir/tools/ValidateJsonFiles.py" "${JsonValidationTargets[@]}"


mkdir -p -- "$DataDir"
cp -a -- "$WebSrcDir/." "$DataDir/"

DirPushed=0

RestoreDir()
{
  if (( DirPushed )); then
    popd >/dev/null 2>&1 || true
    DirPushed=0
  fi
}

trap CleanupPreBuild EXIT

pushd "$ProjectDir" >/dev/null
DirPushed=1
bash "$PrjsDir/tools/UpdateJsonFiles.sh" "$ProjectDir/include/ConfigDefaults.inc.h" Config.json ConfigEntries.json "$DataDir" aligned
RestoreDir

# Publish only final differences; intermediate formatting must not touch live outputs.
python3 "$PrjsDir/tools/SyncGeneratedFiles.py" tree "$DataDir" "$FinalDataDir"
