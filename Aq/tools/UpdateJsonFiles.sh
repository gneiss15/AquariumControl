#!/usr/bin/env bash
# UpdateJsonFiles.sh, Version: 1.11

set -Eeuo pipefail

ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=_TrashPaths.sh
source "$ScriptDir/_TrashPaths.sh"
ProjectDir="$(pwd -P)"
PrjsDir="$(dirname -- "$ProjectDir")"
PrjsPrivateDir="${PRJS_PRIVATE_DIR:-$(dirname -- "$(dirname -- "$PrjsDir")")/PrjsPrivate}"
ConfigDefinition="${1:-}"
ConfigFileName="${2:-Config.json}"
EntriesFileName="${3:-ConfigEntries.json}"
OutputDir="${4:-$ProjectDir/data}"
JsonStyle="${5:-aligned}"
SecretsFile="${PRJS_SECRETS_FILE:-$PrjsPrivateDir/Secrets.local.json}"
DeviceConfigFile="${PRJS_DEVICE_CONFIG_FILE:-}"
ProjectName="$(basename -- "$ProjectDir")"
TempDir=""

PauseOnError()
{
  if [ -t 0 ]; then
    read -r -p "Press enter to continue (exit)" _ || true
  fi
}

Fail()
{
  local Message="$1"

  trap - ERR
  echo -e "$Message" >&2
  PauseOnError
  exit 1
}

UnexpectedError()
{
  local ExitCode=$?
  local LineNr="$1"

  trap - ERR
  echo "Unexpected error in UpdateJsonFiles.sh at line $LineNr" >&2
  PauseOnError
  exit "$ExitCode"
}

Cleanup()
{
  if [ -n "$TempDir" ]; then
    rm -rf -- "$TempDir"
  fi
}

trap 'UnexpectedError "$LINENO"' ERR
trap Cleanup EXIT

command -v gcc >/dev/null 2>&1 || Fail "gcc not installed or not in PATH"
command -v python3 >/dev/null 2>&1 || Fail "python3 not installed or not in PATH"
[ -f "$ScriptDir/UpdateJsonFiles.py" ] || Fail "Python helper not found: $ScriptDir/UpdateJsonFiles.py"

if [ -z "$ConfigDefinition" ]; then
  mapfile -d '' ConfigDefinitions < <(
    find "$ProjectDir" -maxdepth 2 -type f -name '*Config.inc.h' -print0
  )
  if [ "${#ConfigDefinitions[@]}" -ne 1 ]; then
    Fail "Number of *Config.inc.h files is not 1.\nSpecify the config definition explicitly.\nCurrent directory: $ProjectDir"
  fi
  ConfigDefinition="${ConfigDefinitions[0]}"
fi

ConfigDefinition="$(readlink -f -- "$ConfigDefinition")"
[ -f "$ConfigDefinition" ] || Fail "Config definition not found: $ConfigDefinition"

mkdir -p -- "$OutputDir"
TempDir="$(mktemp -d)"

EntriesHeader="$TempDir/Entries.h"
EntriesRaw="$TempDir/ConfigEntries.raw.json"
ConfigHeader="$TempDir/Config.h"
ConfigRaw="$TempDir/Config.raw.json"

cat >"$EntriesHeader" <<EOF_ENTRIES
#define CD_( htmlType, typ, name, defValue, extraHtmlText, ... ) :#name:[#htmlType,extraHtmlText],
#define CD( htmlType, typ, name, defValue, ... ) CD_( htmlType, typ, name, defValue, ##__VA_ARGS__, "" )
#define MkStr( c ) #c
#define C2( c ) MkStr( T ## c )
#define C( c ) C2( c )
#define CD_Div :C( __COUNTER__ ):["d",""],
#include "$ConfigDefinition"
EOF_ENTRIES

printf '{%s"TLast":["d",""]}\n' "$(
  gcc -E "$EntriesHeader" |
    sed -e '/^:/!d;s/^://g' |
    tr -d '\n'
)" >"$EntriesRaw"

cat >"$ConfigHeader" <<EOF_CONFIG
#define CD( htmlType, typ, name, defValue, ... ) :#name:defValue,
#define CD_Div
#define WIFI_OFF 0
#define WIFI_STA 1
#define WIFI_AP 2
#define WIFI_AP_STA 3
#include "$ConfigDefinition"
EOF_CONFIG

printf '{\n%s\n}\n' "$(
  gcc -E "$ConfigHeader" |
    sed -e '/^:/!d;s/^:/ /g' |
    sed '$ s/.$//'
)" >"$ConfigRaw"

python3 "$ScriptDir/UpdateJsonFiles.py" \
  "$EntriesRaw" \
  "$OutputDir/$EntriesFileName" \
  "$ConfigRaw" \
  "$OutputDir/$ConfigFileName" \
  "$SecretsFile" \
  "$ProjectName" \
  "$JsonStyle" \
  "$DeviceConfigFile"

printf 'Generated: %s\n' "$OutputDir/$EntriesFileName"
printf 'Generated: %s\n' "$OutputDir/$ConfigFileName"
if [ -f "$SecretsFile" ]; then
  printf 'Secrets applied from: %s\n' "$SecretsFile"
else
  printf 'No secrets file found: %s\n' "$SecretsFile"
fi
