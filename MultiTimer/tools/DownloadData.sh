#!/usr/bin/env bash
# DownloadData.sh, Version: 1.12

set -Eeuo pipefail
ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
RemoteHost="${1:-${RASPI_HOST:-pi@192.168.99.51}}"
RemoteProjectDir="${2:-${RASPI_PRJS_DIR:-/home/pi/Prjs}/MultiTimer}"
InstanceDir="$ProjectDir/generated/Instances/GN-Pi"
RasPiDir="$InstanceDir/raspi"
RasPiReadableDir="$InstanceDir/raspi-readable"
LocalReadableDir="$InstanceDir/local-readable"
ReadableHelper="$ProjectDir/../tools/CreateReadableLittleFS.py"
ScheduleValidator="$ProjectDir/tools/FormatScheduleJson.py"
source "$ProjectDir/../tools/_TrashPaths.sh"
TempDir=""
SnapshotsCommitted=false

Fail()
{
  echo "Error: $1" >&2
  exit 1
}
Cleanup()
{
  if ! "$SnapshotsCommitted" && declare -p Previous >/dev/null 2>&1; then
    for I in 0 1 2; do
      if [ -e "${Previous[I]}" ]; then
        TrashPaths "${Targets[I]}" || return $?
        mv -- "${Previous[I]}" "${Targets[I]}" || return $?
      elif "${Published[I]}"; then
        TrashPaths "${Targets[I]}" || return $?
      fi
    done
  fi
  [ -z "$TempDir" ] || TrashPaths "$TempDir"
}
StartCompare()
{
  if command -v bco >/dev/null 2>&1; then
    bco "$1" "$2"
  elif command -v bcompare >/dev/null 2>&1; then
    bcompare "$1" "$2"
  else
    bash -ic 'bco "$1" "$2"' bash "$1" "$2"
  fi
}
trap Cleanup EXIT

command -v rsync >/dev/null 2>&1 || Fail "rsync not found"
command -v python3 >/dev/null 2>&1 || Fail "python3 not found"
command -v gio >/dev/null 2>&1 || Fail "gio not found; recoverable replacement requires Trash"
[[ "$RemoteHost" =~ ^[A-Za-z0-9._@-]+$ && "$RemoteHost" != -* ]] || Fail "Invalid remote host"
[[ "$RemoteProjectDir" =~ ^/[A-Za-z0-9._/-]+$ ]] || Fail "Invalid remote project directory"
for Path in "$InstanceDir" "$RasPiDir" "$RasPiReadableDir" "$LocalReadableDir"; do
  [ ! -L "$Path" ] || Fail "Snapshot path must not be a symbolic link: $Path"
done
[ -f "$ReadableHelper" ] || Fail "Readable formatter not found"
[ -f "$ProjectDir/Config.json" ] || Fail "Native local Config.json not found"
[ -d "$ProjectDir/data" ] || Fail "Local data directory not found"
mkdir -p "$InstanceDir"
TempDir="$(mktemp -d "$InstanceDir/.download-data.XXXXXX")"
DownloadDir="$TempDir/raspi"
ReadableDir="$TempDir/raspi-readable"
LocalDir="$TempDir/local"
LocalFormattedDir="$TempDir/local-readable"
mkdir -p "$DownloadDir" "$LocalDir"

# Native application configuration is distinct from data/Config.json.
rsync -a -- "$RemoteHost:$RemoteProjectDir/data/" "$DownloadDir/" || Fail "Pi data download failed"
rsync -a -- "$RemoteHost:$RemoteProjectDir/Config.json" "$DownloadDir/NativeConfig.json" || Fail "Native Pi Config download failed"
for Name in Channels.json SwitchTimes.json NativeConfig.json; do
  [ -s "$DownloadDir/$Name" ] || Fail "Downloaded $Name is missing or empty"
done
python3 "$ScheduleValidator" "$DownloadDir/SwitchTimes.json" "$TempDir/validated-schedule.json"
python3 "$ReadableHelper" "$DownloadDir" "$ReadableDir"
cp -a -- "$ProjectDir/data/." "$LocalDir/"
cp -p -- "$ProjectDir/Config.json" "$LocalDir/NativeConfig.json"
python3 "$ReadableHelper" "$LocalDir" "$LocalFormattedDir"

# Keep rollback copies until all three snapshots have been published.
Targets=( "$RasPiDir" "$RasPiReadableDir" "$LocalReadableDir" )
Sources=( "$DownloadDir" "$ReadableDir" "$LocalFormattedDir" )
Previous=( "$TempDir/previous-raw" "$TempDir/previous-readable" "$TempDir/previous-local" )
Published=( false false false )
for I in 0 1 2; do
  [ ! -e "${Targets[I]}" ] || mv -- "${Targets[I]}" "${Previous[I]}"
done
for I in 0 1 2; do
  if ! mv -- "${Sources[I]}" "${Targets[I]}"; then
    Fail "Could not publish Pi comparison snapshots"
  fi
  Published[I]=true
done
SnapshotsCommitted=true
TrashPaths "${Previous[@]}"
printf 'Pi raw snapshot: %s\nPi readable snapshot: %s\n' "$RasPiDir" "$RasPiReadableDir"
printf 'BCO: local left, downloaded Pi right; NativeConfig.json represents project-root Config.json.\n'
StartCompare "$LocalReadableDir" "$RasPiReadableDir"
printf 'No Pi files or local build/runtime sources were changed.\n'
