#!/usr/bin/env bash
# Install.sh, Version: 1.05

set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
  exec sudo RASPI_PRJS_DIR="${RASPI_PRJS_DIR:-}" /bin/bash "$0" "$@"
fi

RemotePrjsDir="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
ProjectDir="$RemotePrjsDir/Dashboard"
ServiceName=DashboardServer.service
ServiceSource="$ProjectDir/$ServiceName"
ServiceTarget="/etc/systemd/system/$ServiceName"
CronFile=${CRON_FILE:-/etc/crontab}
TempFile="$(mktemp)"
ServiceTemp="$(mktemp)"
trap 'rm -f "$TempFile" "$ServiceTemp"' EXIT

if [ ! -x "$ProjectDir/DashboardServer" ]; then
  echo "Executable not found: $ProjectDir/DashboardServer" >&2
  exit 1
fi

if [ ! -f "$ServiceSource" ]; then
  echo "Service file not found: $ServiceSource" >&2
  exit 1
fi

touch "$CronFile"

awk -v ProjectDir="$ProjectDir" '
  $0 == "# BEGIN AquariumDashboard" {
    Skip = 1
    next
  }
  $0 == "# END AquariumDashboard" {
    Skip = 0
    next
  }
  $0 == "# BEGIN Dashboard" {
    Skip = 1
    next
  }
  $0 == "# END Dashboard" {
    Skip = 0
    next
  }
  Skip {
    next
  }
  index( $0, ProjectDir "/DashboardServer" ) {
    next
  }
  index( $0, "/home/pi/Prj/Dashboard/DashboardServer" ) {
    next
  }
  index( $0, "pgrep" ) && index( $0, "DashboardServer" ) {
    next
  }
  {
    print
  }
' "$CronFile" >"$TempFile"

install -m 0644 "$TempFile" "$CronFile"
systemctl reload cron 2>/dev/null || true

systemctl disable --now aquarium-dashboard.service 2>/dev/null || true
rm -f /etc/systemd/system/aquarium-dashboard.service

systemctl disable --now "$ServiceName" 2>/dev/null || true
pkill -x DashboardServer 2>/dev/null || true

python3 - "$ServiceSource" "$ServiceTemp" "$RemotePrjsDir" <<'PY_SERVICE'
from pathlib import Path
import sys

source, target, root = map(Path, sys.argv[1:])
text = source.read_text()
if text.count('@RASPI_PRJS_DIR@') != 3:
    raise SystemExit('DashboardServer.service template has unexpected path markers')
target.write_text(text.replace('@RASPI_PRJS_DIR@', str(root)))
PY_SERVICE
install -m 0644 "$ServiceTemp" "$ServiceTarget"

systemctl daemon-reload
systemctl reset-failed "$ServiceName" 2>/dev/null || true
systemctl enable --now "$ServiceName"

echo
echo "Installed service: $ServiceName"
echo "Removed old Dashboard start and watch entries from $CronFile."
echo
systemctl --no-pager --full status "$ServiceName"
