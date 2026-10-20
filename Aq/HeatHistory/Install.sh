#!/usr/bin/env bash
# Install.sh, Version: 1.00
set -Eeuo pipefail
ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
ServiceUser="${SUDO_USER:-$USER}"
if [[ "$ServiceUser" == root ]]; then
  echo 'Als normaler PC-Benutzer aufrufen; sudo wird bei Bedarf verwendet.' >&2
  exit 1
fi
UserHome="$(getent passwd "$ServiceUser" | cut -d: -f6)"
RuntimeDir="$UserHome/.local/share/HeatHistory"
if [[ "$EUID" != 0 ]]; then
  exec sudo "$ProjectDir/Install.sh"
fi
install -d -o "$ServiceUser" -g "$(id -gn "$ServiceUser")" "$RuntimeDir"
# Existing PC configuration and historical data are authoritative.
if [[ ! -f "$RuntimeDir/config.json" ]]; then
  install -o "$ServiceUser" -g "$(id -gn "$ServiceUser")" -m 644 "$ProjectDir/config.example.json" "$RuntimeDir/config.json"
fi
StageDir="$(mktemp -d)"
trap 'rm -rf -- "$StageDir"' EXIT
python3 - "$RuntimeDir" "$ServiceUser" "$StageDir/HeatHistory.service" <<'PYTHON'
import sys
from pathlib import Path
root, user, target = sys.argv[1:]
quoted = root.replace('\\', '\\\\').replace('"', '\\"').replace('%', '%%')
Path(target).write_text(f'''[Unit]
Description=Aquarium HeatHistory logger and chart server
Wants=network-online.target
After=network-online.target
[Service]
Type=simple
User={user}
WorkingDirectory={root.replace('%', '%%')}
ExecStart=/usr/bin/python3 -u "{quoted}/HeatHistory.py" --config "{quoted}/config.json" --host 0.0.0.0
Restart=on-failure
RestartSec=10
TimeoutStopSec=15
[Install]
WantedBy=multi-user.target
''')
PYTHON
systemd-analyze verify "$StageDir/HeatHistory.service"
# Stop both possible owners before replacing the running program.
if [[ -f "$UserHome/.config/systemd/user/HeatHistory.service" ]]; then
  runuser -u "$ServiceUser" -- env XDG_RUNTIME_DIR="/run/user/$(id -u "$ServiceUser")" systemctl --user disable --now HeatHistory.service
fi
systemctl stop HeatHistory.service 2>/dev/null || true
for Source in HeatHistory.py CreateCharts.py Versions.txt; do
  install -o "$ServiceUser" -g "$(id -gn "$ServiceUser")" -m 644 "$ProjectDir/$Source" "$RuntimeDir/$Source"
done
install -m 644 "$StageDir/HeatHistory.service" /etc/systemd/system/HeatHistory.service
systemctl daemon-reload
systemctl enable HeatHistory.service
systemctl restart HeatHistory.service
systemctl --no-pager --full status HeatHistory.service
printf '\nDaten und Konfiguration: %s\nBrowser: http://localhost:8091/\n' "$RuntimeDir"
