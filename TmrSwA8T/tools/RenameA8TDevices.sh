#!/usr/bin/env bash
# RenameA8TDevices.sh, Version: 1.00
set -Eeuo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
DevStateDir="$ProjectDir/DevState"

RenameDevice()
 {
  local OldName="$1"
  local NewName="$2"
  local OldDir="$DevStateDir/$OldName"
  local NewDir="$DevStateDir/$NewName"

  if [ -e "$OldDir" ] && [ -e "$NewDir" ]; then
    echo "Both old and new device directories exist; refusing: $OldName / $NewName" >&2
    return 1
  fi

  if [ -e "$OldDir" ]; then
    mv -- "$OldDir" "$NewDir"
    echo "Renamed: DevState/$OldName -> DevState/$NewName"
  elif [ -e "$NewDir" ]; then
    echo "Already renamed: DevState/$NewName"
  else
    echo "Not present, skipped: DevState/$OldName"
    return 0
  fi

  python3 - "$NewDir" "$OldName" "$NewName" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
old = sys.argv[2]
new = sys.argv[3]


def replace(value):
    if isinstance(value, str):
        return new if value == old else value
    if isinstance(value, list):
        return [replace(item) for item in value]
    if isinstance(value, dict):
        return {key: replace(item) for key, item in value.items()}
    return value


for path in sorted(root.glob('*.json')):
    data = json.loads(path.read_text())
    updated = replace(data)
    if updated != data:
        path.write_text(json.dumps(updated, indent=2, ensure_ascii=False) + '\n')
        print(f'  Updated: {path.name}')
PY
 }

RenameDevice "HeizungHauptAq1" "HeaterMainAq1"
RenameDevice "HeizungHauptAq2" "HeaterMainAq2"
RenameDevice "A8TSwitch4" "HeaterMainAq3"
