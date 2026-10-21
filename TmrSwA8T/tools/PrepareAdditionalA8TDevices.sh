#!/usr/bin/env bash
# PrepareAdditionalA8TDevices.sh, Version: 1.01
set -Eeuo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
SourceDevice="LightTimerAq3"
SourceDir="$ProjectDir/DevState/$SourceDevice"

[ -f "$SourceDir/Config.json" ] || {
  echo "Source device config not found: $SourceDir/Config.json" >&2
  echo "Run this script in the normal project tree where DevState/$SourceDevice exists." >&2
  exit 1
}

CreateDevice()
 {
  local DeviceName="$1"
  local ChannelId="$2"
  local TargetDir="$ProjectDir/DevState/$DeviceName"

  if [ -e "$TargetDir" ]; then
    echo "Target already exists, not overwriting: $TargetDir" >&2
    return 1
  fi

  mkdir -p -- "$TargetDir"
  cp -- "$SourceDir/Config.json" "$TargetDir/Config.json"

  python3 - "$TargetDir/Config.json" "$DeviceName" <<'PY'
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
device = sys.argv[2]
data = json.loads(path.read_text())
for key in ("Hostname", "ApSsid", "ApPw", "FtpPw"):
    if key in data:
        data[key] = device
for key in ("MeterVoltageCal", "MeterCurrentCal", "MeterPowerCal"):
    data.pop(key, None)
path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")
PY

  cat > "$TargetDir/Channels.json" <<EOF_CHANNELS
{
  "Profiles": [],
  "Channels": [
    { "Id": "$ChannelId", "Role": "Timer" }
  ]
}
EOF_CHANNELS

  printf '[]\n' > "$TargetDir/SwitchTimes.json"
  echo "Created: DevState/$DeviceName"
 }

CreateDevice "HeaterMainAq1" "HeaterMainAq1"
CreateDevice "HeaterMainAq2" "HeaterMainAq2"
CreateDevice "HeaterMainAq3" "HeaterMainAq3"

echo
echo "The three heater devices intentionally start with Role=Timer and no switch times."
echo "Configure HeaterMain/profile only during the later heating-control test."
