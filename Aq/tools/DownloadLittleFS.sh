#!/usr/bin/env bash
# DownloadLittleFS.sh, Version: 1.15

set -Eeuo pipefail

ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
ProjectDir="$(pwd -P)"
DeviceHost="${1:-}"
DevStateRoot="$ProjectDir/DevState"
FtpRootDir="$ProjectDir/generated/ftp"
FtpReadableRootDir="$ProjectDir/generated/ftp-readable"
FtpDevStateReadableRootDir="$ProjectDir/generated/ftp-devstate-readable"
InstancesRoot="${PRJS_INSTANCES_DIR:-}"
ReadableHelper="$ScriptDir/CreateReadableLittleFS.py"
TempDir=""

PauseOnError()
{
  if [ -t 0 ]; then
    read -r -p "Press enter to continue (exit)" _ || true
  fi
}

Fail()
{
  trap - ERR
  echo "Error: $1" >&2
  PauseOnError
  exit 1
}

UnexpectedError()
{
  local ExitCode=$?
  trap - ERR
  echo "Unexpected error in DownloadLittleFS.sh at line $1" >&2
  PauseOnError
  exit "$ExitCode"
}

TrashPaths()
{
  local Path
  for Path in "$@"; do
    if [ -e "$Path" ] || [ -L "$Path" ]; then
      gio trash -- "$Path" || return $?
    fi
  done
}

Cleanup()
{
  [ -z "$TempDir" ] || TrashPaths "$TempDir"
}

StartCompare()
{
  local Left="$1"
  local Right="$2"
  if command -v bco >/dev/null 2>&1; then
    bco "$Left" "$Right"
  elif command -v bcompare >/dev/null 2>&1; then
    bcompare "$Left" "$Right"
  else
    bash -ic 'bco "$1" "$2"' bash "$Left" "$Right"
  fi
}

trap 'UnexpectedError "$LINENO"' ERR
trap Cleanup EXIT

command -v python3 >/dev/null 2>&1 || Fail "python3 not installed or not in PATH"
command -v gio >/dev/null 2>&1 || Fail "gio not installed; recoverable snapshot replacement requires Trash"
[ -f "$ScriptDir/DownloadLittleFS.py" ] || Fail "Python helper not found: $ScriptDir/DownloadLittleFS.py"
[ -f "$ReadableHelper" ] || Fail "Readable-snapshot helper not found: $ReadableHelper"
[ -d "$DevStateRoot" ] || Fail "DevState directory not found: $DevStateRoot"

if [ -z "$DeviceHost" ]; then
  mapfile -t Devices < <(find "$DevStateRoot" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort)
  if [ "${#Devices[@]}" -ne 1 ]; then
    Message="Specify device name; ${#Devices[@]} device states found"
    if [ "${#Devices[@]}" -gt 0 ]; then
      Message+=$'\nAvailable devices:'
      for Device in "${Devices[@]}"; do
        Message+=$'\n  '"$Device"
      done
      Message+=$'\nUsage: tools/DownloadLittleFS.sh <device name>'
    fi
    Fail "$Message"
  fi
  DeviceHost="${Devices[0]}"
fi
[[ "$DeviceHost" =~ ^[A-Za-z0-9._-]+$ ]] || Fail "Invalid device hostname: $DeviceHost"

DeviceStateDir="$DevStateRoot/$DeviceHost"
if [ -n "$InstancesRoot" ]; then
  InstanceDir="$InstancesRoot/$DeviceHost"
  DataDir="$InstanceDir/LittleFS"
  DataReadableDir="$InstanceDir/LittleFS-readable"
  FtpDir="$InstanceDir/ftp"
  FtpReadableDir="$InstanceDir/ftp-readable"
  FtpDevStateReadableDir="$InstanceDir/ftp-devstate-readable"
else
  DataDir="$ProjectDir/generated/$DeviceHost/data"
  DataReadableDir="$ProjectDir/generated/$DeviceHost/data-readable"
  FtpDir="$FtpRootDir/$DeviceHost"
  FtpReadableDir="$FtpReadableRootDir/$DeviceHost"
  FtpDevStateReadableDir="$FtpDevStateReadableRootDir/$DeviceHost"
fi
ConfigFile="$DataDir/Config.json"

[ -d "$DeviceStateDir" ] || Fail "Device state not found: $DeviceStateDir"
[ -f "$ConfigFile" ] || Fail "Generated device Config.json not found; run EspCompile.sh first: $ConfigFile"
[ -d "$DataReadableDir" ] || Fail "Generated data-readable snapshot not found; run EspCompile.sh first: $DataReadableDir"

if [ -n "$InstancesRoot" ]; then
  mkdir -p -- "$InstanceDir"
else
  mkdir -p -- "$FtpRootDir" "$FtpReadableRootDir" "$FtpDevStateReadableRootDir"
fi
TempDir="$(mktemp -d "$ProjectDir/.littlefs-download.XXXXXX")"
DownloadDir="$TempDir/files"
ReadableDownloadDir="$TempDir/readable"
mkdir -p -- "$DownloadDir"

DeviceAddress="$(python3 "$ScriptDir/ResolveDeviceAddress.py" "$DeviceHost")" || Fail "Cannot resolve current function address"
python3 "$ScriptDir/DownloadLittleFS.py" "$ConfigFile" "$DownloadDir" "$DeviceAddress" || Fail "LittleFS FTP download failed"
python3 "$ReadableHelper" "$DownloadDir" "$ReadableDownloadDir" || Fail "Could not create readable FTP snapshot"
# Runtime-only binary diagnostics remain in generated/ftp but must not pollute source/build comparisons.
TrashPaths "$ReadableDownloadDir/OneWireDiag.bin"

TrashPaths "$FtpDir" "$FtpReadableDir"
mv -- "$DownloadDir" "$FtpDir"
mv -- "$ReadableDownloadDir" "$FtpReadableDir"
printf 'LittleFS snapshot updated: %s\n' "$FtpDir"
printf 'Readable LittleFS snapshot updated: %s\n' "$FtpReadableDir"

# Refresh both readable snapshots with the same formatter; build left, device right.
python3 "$ReadableHelper" "$DataDir" "$DataReadableDir" || Fail "Could not refresh readable build snapshot"
StartCompare "$DataReadableDir" "$FtpReadableDir"

if [ -t 0 ]; then
  read -r -p "Compare downloaded state with DevState/$DeviceHost? [Y/n] " Answer || true
else
  Answer="n"
fi
case "${Answer:-Y}" in
  [Nn]*)
    exit 0
    ;;
esac

# Create the second snapshot only on request and include only DevState source files.
TrashPaths "$FtpDevStateReadableDir"
mkdir -p -- "$FtpDevStateReadableDir"
while IFS= read -r -d '' SourceFile; do
  Relative="${SourceFile#"$DeviceStateDir/"}"
  DownloadedFile="$FtpReadableDir/$Relative"
  [ -f "$DownloadedFile" ] || continue
  mkdir -p -- "$FtpDevStateReadableDir/$(dirname -- "$Relative")"
  install -m 0644 "$DownloadedFile" "$FtpDevStateReadableDir/$Relative"
done < <(find "$DeviceStateDir" -type f -print0 | sort -z)

StartCompare "$DeviceStateDir" "$FtpDevStateReadableDir"
