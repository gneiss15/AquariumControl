#!/usr/bin/env bash
# CleanGeneratedConfig.sh, Version: 1.10

set -Eeuo pipefail

PrjsDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

PauseOnError()
{
  if [ -t 0 ]; then
    read -r -p "Press enter to continue (exit)" _ || true
  fi
}

UnexpectedError()
{
  local ExitCode=$?
  local LineNr="$1"

  trap - ERR
  echo "Unexpected error in CleanGeneratedConfig.sh at line $LineNr" >&2
  PauseOnError
  exit "$ExitCode"
}

trap 'UnexpectedError "$LINENO"' ERR

for Project in Feeder FeederSimple TmrSwA8T TmrSwShelly MultiTimer; do
  GeneratedDir="$PrjsDir/$Project/generated"
  [ -d "$GeneratedDir" ] || continue
  find "$GeneratedDir" -type f \
    \( -name Config.json -o -name ConfigEntries.json -o -name ESPConfig.json -o -name 'ChatGPT Config.json' \) \
    -delete
done

rm -f -- \
  "$PrjsDir/MultiTimer/data/Config.json" \
  "$PrjsDir/MultiTimer/data/ConfigEntries.json" \
  "$PrjsDir/MultiTimer/data/ESPConfig.json" \
  "$PrjsDir/MultiTimer/data/ChatGPT Config.json"

printf 'Generated configuration files removed.\n'
