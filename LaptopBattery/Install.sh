#!/usr/bin/env bash
# Install.sh, Version: 1.03
set -euo pipefail
ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
LaptopUser="$(id -un)"
if [[ "$LaptopUser" == root ]]; then
  echo "Run this script as the laptop user without sudo; required installation commands use sudo." >&2
  exit 1
fi
[[ "$LaptopUser" =~ ^[a-zA-Z_][a-zA-Z0-9_.-]*$ ]] || exit 1

# Verify the source's existing hardware paths on the target before replacing an installation.
python3 - "$ScriptDir" <<'PY'
import pathlib
import sys
sys.path.insert(0, sys.argv[1])
import BatteryUdpSender as Sender
import ChargeLearning
for Name in ("BatteryCapacityFile", "AcOnlineFile", "BatteryStateFile", "LaptopMacFile"):
    Path = pathlib.Path(getattr(Sender, Name))
    if not Path.is_file() or not Path.read_text().strip():
        raise SystemExit("Check laptop hardware path before installation: " + str(Path))
if Sender.ReadCapacity() is None or Sender.ReadAcOnline() is None:
    raise SystemExit("Invalid battery/AC measurement; check source paths before installation")
ChargeLearning.LoadState()
PY

UnitFile="$(mktemp)"
trap 'rm -f -- "$UnitFile"' EXIT
sed "s/@LAPTOP_USER@/$LaptopUser/g" "$ScriptDir/BatteryUdpSender.service" > "$UnitFile"
sudo install -m 755 "$ScriptDir/BatteryUdpSender.py" "$ScriptDir/ChargeLearning.py" \
  "$ScriptDir/ChargeLearning.sh" "$ScriptDir/Messages.py" /usr/local/bin/
sudo install -d /usr/local/share/LaptopBattery
sudo install -m 644 "$ScriptDir/Versions.txt" /usr/local/share/LaptopBattery/
sudo install -m 644 "$UnitFile" /etc/systemd/system/BatteryUdpSender.service
sudo systemctl daemon-reload
sudo systemctl enable BatteryUdpSender.service
sudo systemctl restart BatteryUdpSender.service
systemctl --no-pager --full status BatteryUdpSender.service
