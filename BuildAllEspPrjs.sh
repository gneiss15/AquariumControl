#!/usr/bin/env bash
# BuildAllEspPrjs.sh, Version: 2.39

set -euo pipefail

BuildNoId=false

ShowUsage()
{
  cat <<EOF
Usage: $(basename -- "$0") [--NoId]

Options:
  --NoId       additionally create one-time legacy-format firmware/rescue artifacts
  --help, -h   show this help
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --NoId)
      BuildNoId=true
      ;;
    --help|-h)
      ShowUsage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      ShowUsage >&2
      exit 2
      ;;
  esac
  shift
done

PrjsDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
pushd "$PrjsDir" >/dev/null

BuildArgs=( --build )
if "$BuildNoId"; then
  BuildArgs+=( --NoId )
fi

for i in TmrSwA8T TmrSwShelly Feeder FeederSimple; do
  echo
  echo "########################################"
  echo "Starte $i"
  echo "########################################"
  "$i/EspRescueCompile.sh" "${BuildArgs[@]}"
  "$i/EspCompile.sh" "${BuildArgs[@]}"
done
popd >/dev/null
