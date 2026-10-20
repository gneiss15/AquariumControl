#!/usr/bin/env bash
# CopyToRasPi.sh, Version: 1.32

set -euo pipefail

ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(cd -- "$ScriptDir/.." && pwd -P)"
RemoteHost="${RASPI_HOST:-pi@192.168.99.51}"
RemotePrjsDir="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
RemoteDir="$RemotePrjsDir/Dashboard"
RemoteDataDir="$RemoteDir/data"
CommonLibDir="$PrjsDir/Libs/GnBasicLib"
RemoteCommonLibDir="$RemotePrjsDir/Libs/GnBasicLib"
ProjectDataDir="$ScriptDir/data"
RasPiSnapshotDir="$ScriptDir/generated/raspi"
RasPiReadableDir="$ScriptDir/generated/raspi-readable"
BuildReadableDir="$ScriptDir/generated/build-readable"
SourcesOnly=false
HasCentralDataDifferences=false
HasAutoTimestampUpdates=false
DataBackupDir=""
DownloadTempDir=""

Cleanup()
{
  if [[ -n "$DownloadTempDir" ]]; then
    rm -rf -- "$DownloadTempDir"
  fi
}
trap Cleanup EXIT

if [[ $# == 1 && "$1" == --sources-only ]]; then
  SourcesOnly=true
elif [[ $# -gt 0 ]]; then
  echo "Usage: $0 [--sources-only]" >&2
  exit 2
fi

Fail()
{
  echo "Error: $*" >&2
  exit 1
}

DownloadRasPiData()
{
  command -v rsync >/dev/null 2>&1 || Fail "rsync not installed or not in PATH"
  command -v python3 >/dev/null 2>&1 || Fail "python3 not installed or not in PATH"
  [[ -d "$ProjectDataDir" ]] || Fail "Project data directory not found: $ProjectDataDir"
  [[ ! -L "$RasPiSnapshotDir" ]] || Fail "Raspberry Pi snapshot must not be a symbolic link: $RasPiSnapshotDir"
  [[ ! -L "$RasPiReadableDir" ]] || Fail "Readable Raspberry Pi snapshot must not be a symbolic link: $RasPiReadableDir"

  mkdir -p -- "$ScriptDir/generated"
  DownloadTempDir="$(mktemp -d "$ScriptDir/generated/.download-raspi.XXXXXX")"
  local DownloadDir="$DownloadTempDir/raspi"
  local ReadableDir="$DownloadTempDir/raspi-readable"
  local PreviousRasPiDir="$DownloadTempDir/previous-raspi"
  local PreviousReadableDir="$DownloadTempDir/previous-readable"
  mkdir -p -- "$DownloadDir" "$ReadableDir"

  rsync -a -- "$RemoteHost:$RemoteDataDir/" "$DownloadDir/" || \
    Fail "Could not download $RemoteHost:$RemoteDataDir/"
  [[ -s "$DownloadDir/Times.json" ]] || Fail "Downloaded Times.json is missing or empty"
  [[ -s "$DownloadDir/HeatProfiles.json" ]] || Fail "Downloaded HeatProfiles.json is missing or empty"

  python3 - "$DownloadDir" "$ReadableDir" <<'PY'
import json
import pathlib
import sys

source_dir, target_dir = map(pathlib.Path, sys.argv[1:3])
for name in ("Times.json", "HeatProfiles.json"):
    source = source_dir / name
    with source.open("r", encoding="utf-8") as source_file:
        data = json.load(source_file)
    if name == "Times.json" and (
        not isinstance(data, dict)
        or not isinstance(data.get("Sets"), list)
        or not isinstance(data.get("Times"), list)
    ):
        raise ValueError("Times.json does not contain valid Sets[] and Times[]")
    with (target_dir / name).open("w", encoding="utf-8") as target_file:
        json.dump(data, target_file, ensure_ascii=False, indent=2)
        target_file.write("\n")
PY
  python3 "$ScriptDir/tools/ValidateHeatProfiles.py" "$DownloadDir/HeatProfiles.json" || \
    Fail "Downloaded HeatProfiles.json is invalid"

  if [[ -e "$RasPiSnapshotDir" ]]; then
    mv -- "$RasPiSnapshotDir" "$PreviousRasPiDir"
  fi
  if [[ -e "$RasPiReadableDir" ]]; then
    mv -- "$RasPiReadableDir" "$PreviousReadableDir"
  fi
  if ! mv -- "$DownloadDir" "$RasPiSnapshotDir"; then
    [[ ! -e "$PreviousRasPiDir" ]] || mv -- "$PreviousRasPiDir" "$RasPiSnapshotDir"
    [[ ! -e "$PreviousReadableDir" ]] || mv -- "$PreviousReadableDir" "$RasPiReadableDir"
    Fail "Could not replace Raspberry Pi snapshot directory"
  fi
  if ! mv -- "$ReadableDir" "$RasPiReadableDir"; then
    rm -rf -- "$RasPiSnapshotDir"
    [[ ! -e "$PreviousRasPiDir" ]] || mv -- "$PreviousRasPiDir" "$RasPiSnapshotDir"
    [[ ! -e "$PreviousReadableDir" ]] || mv -- "$PreviousReadableDir" "$RasPiReadableDir"
    Fail "Could not replace readable Raspberry Pi snapshot directory"
  fi
  rm -rf -- "$PreviousRasPiDir" "$PreviousReadableDir"

  printf 'Raspberry Pi Dashboard data snapshot updated: %s\n' "$RasPiSnapshotDir"
  printf 'Readable central Pi data updated: %s\n' "$RasPiReadableDir"
  printf 'Project data remains unchanged: %s\n' "$ProjectDataDir"
}

StartCompare()
{
  local CompareExit=0

  if command -v bco >/dev/null 2>&1; then
    bco "$BuildReadableDir" "$RasPiReadableDir" || true
    return 0
  fi

  bash -ic 'type -t bco >/dev/null 2>&1 || exit 127; bco "$1" "$2"' \
    bash "$BuildReadableDir" "$RasPiReadableDir" || CompareExit=$?
  if [ "$CompareExit" -ne 127 ]; then
    return 0
  fi

  if command -v bcompare >/dev/null 2>&1; then
    bcompare "$BuildReadableDir" "$RasPiReadableDir" || true
    return 0
  fi

  Fail "Beyond Compare command bco or bcompare not found"
}

PrepareBuildReadable()
{
  python3 - "$ProjectDataDir" "$BuildReadableDir" <<'PY'
import json
import pathlib
import shutil
import sys

source_dir, target_dir = map(pathlib.Path, sys.argv[1:3])
shutil.rmtree(target_dir, ignore_errors=True)
target_dir.mkdir(parents=True, exist_ok=True)
for name in ("Times.json", "HeatProfiles.json"):
    source = source_dir / name
    if not source.is_file():
        raise SystemExit(f"Required central build data is missing: {source}")
    with source.open("r", encoding="utf-8") as source_file:
        data = json.load(source_file)
    with (target_dir / name).open("w", encoding="utf-8") as target_file:
        json.dump(data, target_file, ensure_ascii=False, indent=2)
        target_file.write("\n")
PY
}

MergePiTimestamps()
{
  python3 - "$BuildReadableDir/Times.json" "$RasPiReadableDir/Times.json" <<'PY'
import datetime
import json
import pathlib
import sys

build_path, pi_path = map(pathlib.Path, sys.argv[1:3])
build = json.loads(build_path.read_text(encoding="utf-8"))
pi = json.loads(pi_path.read_text(encoding="utf-8"))
changed = 0

def parse_timestamp(value):
    if not isinstance(value, str):
        return None
    try:
        result = datetime.datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None
    if result.tzinfo is None:
        return None
    return result.astimezone(datetime.timezone.utc)

def newer_timestamp(build_record, pi_record):
    build_value = build_record.get("TimeStamp")
    pi_value = pi_record.get("TimeStamp")
    build_time = parse_timestamp(build_value)
    pi_time = parse_timestamp(pi_value)
    if build_time is None or pi_time is None or pi_time <= build_time:
        return False
    build_content = {key: value for key, value in build_record.items() if key != "TimeStamp"}
    pi_content = {key: value for key, value in pi_record.items() if key != "TimeStamp"}
    if build_content != pi_content:
        return False
    build_record["TimeStamp"] = pi_value
    return True

def unique_set_map(value):
    rows = value.get("Sets") if isinstance(value, dict) else None
    if not isinstance(rows, list):
        return {}
    result = {}
    duplicates = set()
    for row in rows:
        if not isinstance(row, dict):
            continue
        key = (row.get("Device"), row.get("Type"))
        if not all(isinstance(part, str) and part for part in key):
            continue
        if key in result:
            duplicates.add(key)
        result[key] = row
    for key in duplicates:
        result.pop(key, None)
    return result

build_sets = unique_set_map(build)
pi_sets = unique_set_map(pi)
for key in build_sets.keys() & pi_sets.keys():
    changed += newer_timestamp(build_sets[key], pi_sets[key])

build_channels = build.get("Channels") if isinstance(build, dict) else None
pi_channels = pi.get("Channels") if isinstance(pi, dict) else None
if isinstance(build_channels, dict) and isinstance(pi_channels, dict):
    for device in build_channels.keys() & pi_channels.keys():
        if isinstance(build_channels[device], dict) and isinstance(pi_channels[device], dict):
            changed += newer_timestamp(build_channels[device], pi_channels[device])

if changed:
    build_path.write_text(json.dumps(build, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(changed)
PY
}

ValidateBuildReadable()
{
  python3 - "$BuildReadableDir" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
for path in sorted(root.rglob("*.json")):
    with path.open("r", encoding="utf-8") as source:
        json.load(source)
times_path = root / "Times.json"
if times_path.is_file():
    with times_path.open("r", encoding="utf-8") as source:
        times = json.load(source)
    if not isinstance(times, dict) or not isinstance(times.get("Sets"), list) or not isinstance(times.get("Times"), list):
        raise ValueError("Times.json must contain Sets[] and Times[]")
profiles_path = root / "HeatProfiles.json"
if profiles_path.is_file():
    with profiles_path.open("r", encoding="utf-8") as source:
        profiles = json.load(source)
    if not isinstance(profiles, dict) or not isinstance(profiles.get("Profiles"), list):
        raise ValueError("HeatProfiles.json must contain Profiles[]")
PY
  if [ -f "$BuildReadableDir/HeatProfiles.json" ]; then
    python3 "$ScriptDir/tools/ValidateHeatProfiles.py" "$BuildReadableDir/HeatProfiles.json"
  fi
}

CompareData()
{
  python3 - "$BuildReadableDir" "$RasPiReadableDir" <<'PY'
import json
import pathlib
import sys

left, right = map(pathlib.Path, sys.argv[1:3])

def files(root):
    return {p.relative_to(root).as_posix(): p for p in root.rglob("*") if p.is_file()}

left_files = files(left)
right_files = files(right)
differences = []
for name in sorted(left_files.keys() | right_files.keys()):
    if name not in left_files or name not in right_files:
        differences.append(name)
        continue
    left_path, right_path = left_files[name], right_files[name]
    try:
        if left_path.suffix.lower() == ".json":
            with left_path.open("r", encoding="utf-8") as source:
                left_data = json.load(source)
            with right_path.open("r", encoding="utf-8") as source:
                right_data = json.load(source)
            equal = left_data == right_data
        else:
            equal = left_path.read_bytes() == right_path.read_bytes()
    except (UnicodeError, json.JSONDecodeError) as error:
        raise SystemExit(f"Invalid comparison input {name}: {error}")
    if not equal:
        differences.append(name)

if differences:
    print("Build and Raspberry Pi data differ:")
    for name in differences:
        print(f"  {name}")
    raise SystemExit(1)
print("Build and Raspberry Pi data are semantically identical.")
PY
}

ApplyBuildEdits()
{
  local BackupParent

  BackupParent="$ScriptDir/generated/data-before-sync"
  mkdir -p -- "$BackupParent"
  DataBackupDir="$(mktemp -d "$BackupParent/$(date '+%Y%m%d-%H%M%S').XXXXXX")"
  cp -a -- "$ProjectDataDir/." "$DataBackupDir/"
  if ! cp -a -- "$BuildReadableDir/Times.json" "$BuildReadableDir/HeatProfiles.json" "$ProjectDataDir/"; then
    rsync -a --delete "$DataBackupDir/" "$ProjectDataDir/" || \
      Fail "Could not apply BCO data or restore the previous data/ backup at $DataBackupDir"
    Fail "Could not apply BCO data; the previous data/ files were restored"
  fi
  mapfile -t Backups < <(find "$BackupParent" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort -r)
  for (( Index = 3; Index < ${#Backups[@]}; ++Index )); do
    rm -rf -- "$BackupParent/${Backups[$Index]}"
  done
  printf 'BCO-edited build data applied to %s\n' "$ProjectDataDir"
  printf 'Previous build data backup: %s\n' "$DataBackupDir"
}

CheckAndMergeData()
{
  echo "Lade zuerst alle Dashboard-Daten vom Raspberry Pi ..."
  DownloadRasPiData

  PrepareBuildReadable || Fail "Could not pretty-print build data"
  local AutoTimestampCount
  AutoTimestampCount="$(MergePiTimestamps)" || Fail "Could not merge newer Pi timestamps"
  if (( AutoTimestampCount > 0 )); then
    HasAutoTimestampUpdates=true
    echo "$AutoTimestampCount newer Pi timestamp(s) adopted automatically; corresponding records were otherwise identical."
  fi
  ValidateBuildReadable || Fail "Build data is invalid"

  if CompareData; then
    HasCentralDataDifferences=false
    if [[ "$HasAutoTimestampUpdates" == true ]]; then
      ApplyBuildEdits || Fail "Could not apply automatically adopted Pi timestamps to the build data"
      echo "Only newer Pi timestamps differed; they were applied to the local build without opening BCO."
    else
      echo "Times.json und HeatProfiles.json sind semantisch identisch."
    fi
    return 0
  fi

  HasCentralDataDifferences=true
  echo "BCO vergleicht ausschließlich Times.json und HeatProfiles.json."
  echo "Layouts.json und HeatProfiles.sync.json bleiben Pi-Laufzeitdaten und werden nicht in den Build übernommen."
  StartCompare

  if [ ! -t 0 ]; then
    Fail "BCO comparison requires an interactive terminal; no project or Pi files were changed"
  fi
  read -r -p "BCO-Abgleich abschließen, BCO schließen und Enter drücken, um die Build-Daten zu übernehmen. " _

  ValidateBuildReadable || Fail "BCO-edited build data is invalid; existing data/ files were not changed"
  ApplyBuildEdits || Fail "Could not apply BCO-edited build data"
}

CopyToRasPi()
{
  python3 "$PrjsDir/tools/UpdateVersionFiles.py" "$ScriptDir" || \
    Fail "Could not generate Dashboard version files"
  if [[ "$SourcesOnly" == true ]]; then
    DownloadRasPiData
    echo "Quellupdate: zentrale Times.json und HeatProfiles.json bleiben auf PC und Pi unverändert."
  else
    CheckAndMergeData
  fi

  if [ ! -t 0 ]; then
    Fail "An interactive confirmation is required before transferring data to the Raspberry Pi"
  fi
  if [[ "$SourcesOnly" == true ]]; then
    read -r -p "Dashboard-Quellstand und Version.json zum Pi übertragen? Zentrale Zeiten/Profile und Gerätezuordnung bleiben unverändert. [j/N] " Answer
  elif [[ "$HasCentralDataDifferences" == true ]]; then
    read -r -p "Dashboard-Quellstand sowie abgestimmte Times.json, HeatProfiles.json und Version.json zum Pi übertragen? Bei geänderten Server-Quellen muss danach auf dem Pi make gebaut und der Dienst neu gestartet werden; Pi-eigene data-Dateien bleiben erhalten. [j/N] " Answer
  else
    read -r -p "Dashboard-Quellstand sowie Times.json, HeatProfiles.json und Version.json zum Pi übertragen? Bei geänderten Server-Quellen muss danach auf dem Pi make gebaut und der Dienst neu gestartet werden. [j/N] " Answer
  fi
  case "$Answer" in
    j|J|ja|Ja|JA|y|Y|yes|Yes|YES)
      ;;
    *)
      echo "Übertragung abgebrochen. Pi-Dateien wurden nicht verändert."
      return 0
      ;;
  esac

  echo "Synchronisiere Dashboard nach $RemoteHost:$RemoteDir/ ..."
  ssh "$RemoteHost" "mkdir -p '$RemoteDataDir' '$RemoteCommonLibDir' '$RemotePrjsDir/tools'" || return $?

  (
    cd "$ScriptDir"
    rsync -v -a \
      Dashboard.js \
      FeedbackAlarm.js \
      DashboardServer.cpp \
      FeedbackMonitor.cpp \
      FeedbackMonitor.h \
      FeedbackMonitors.json \
      DashboardServer.service \
      Devices.json \
      DeviceRegistry.cpp \
      DeviceEditor.cpp \
      EndpointAssignments.cpp \
      EndpointAssignments.h \
      DeviceRegistry.h \
      AssignmentControl.cpp \
      AssignmentControl.h \
      HeatProfileStore.cpp \
      HeatProfileStore.h \
      Index.css \
      Install.sh \
      Makefile \
      StatusClient.js \
      TimeStore.cpp \
      TimeStore.h \
      Translations.json \
      Versions.txt \
      index.html \
      "$RemoteHost:$RemoteDir/"
  ) || return $?

  rsync -v -a "$PrjsDir/tools/UpdateVersionFiles.py" \
    "$RemoteHost:$RemotePrjsDir/tools/" || return $?
  if [[ "$SourcesOnly" == true ]]; then
    rsync -v -a "$ProjectDataDir/Version.json" "$RemoteHost:$RemoteDataDir/" || return $?
  else
  rsync -v -a \
    "$ProjectDataDir/Times.json" "$ProjectDataDir/HeatProfiles.json" "$ProjectDataDir/Version.json" \
    "$RemoteHost:$RemoteDataDir/" || return $?
  fi

  if [ ! -f "$RasPiSnapshotDir/DeviceAssignments.json" ]; then
    rsync -v -a "$ProjectDataDir/DeviceAssignments.json" \
      "$RemoteHost:$RemoteDataDir/" || return $?
  else
    echo "Pi-Gerätezuordnung bleibt erhalten; Änderungen erfolgen im Dashboard."
  fi

  echo "Synchronisiere benötigte GnBasicLib-Quellen ..."
  rsync -v -a \
    --include='*/' \
    --include='*.h' \
    --include='*.cpp' \
    --exclude='*' \
    "$CommonLibDir/" "$RemoteHost:$RemoteCommonLibDir/" || return $?

  echo "Synchronisation abgeschlossen."
}

if CopyToRasPi; then
  exit 0
else
  Status=$?
  echo "Fehler bei der Synchronisation zum Raspberry Pi." >&2
  exit "$Status"
fi
