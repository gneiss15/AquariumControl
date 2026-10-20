#!/usr/bin/env bash
# ShowVersions.sh, Version: 1.09

set -euo pipefail

PrjsDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
DirPushed=0

RestoreDir()
{
  if (( DirPushed )); then
    popd >/dev/null 2>&1 || true
    DirPushed=0
  fi
}

OnError()
{
  local ExitCode=$?
  local LineNr="$1"
  trap - ERR
  RestoreDir
  echo "Unexpected error in ShowVersions.sh at line $LineNr" >&2
  exit "$ExitCode"
}

trap RestoreDir EXIT
trap 'OnError "$LINENO"' ERR

pushd "$PrjsDir" >/dev/null
DirPushed=1

while IFS= read -r VersionsFile; do
  RelativePath="${VersionsFile#./}"
  RelativePath="${RelativePath%/Versions.txt}"
  printf '%s:' "$RelativePath"
  while IFS='=' read -r Name Revision; do
    [[ -n "$Name" && "$Name" != \#* ]] || continue
    printf ' %s=%s' "$Name" "$Revision"
  done < "$VersionsFile"
  printf '\n'
done < <(find Aq ShellyNousTakeover -type f -name Versions.txt \
  -not -path '*/ShellyMigration/RescueLayout/NodeTest/*' \
  -not -path '*/generated/*' -not -path '*/data/*' -not -path '*/WebSrc/*' \
  -not -path '*/managed_components/*' -print | sort)

RestoreDir
