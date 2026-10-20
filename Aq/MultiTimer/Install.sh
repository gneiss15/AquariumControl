#!/usr/bin/env bash
# Install.sh, Version: 1.07

set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
  exec sudo RASPI_PRJS_DIR="${RASPI_PRJS_DIR:-}" /bin/bash "$0" "$@"
fi

RemotePrjsDir="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
ProjectDir="$RemotePrjsDir/MultiTimer"
ServiceName=MultiTimer.service
ServiceSource="$ProjectDir/$ServiceName"
ServiceTarget="/etc/systemd/system/$ServiceName"
CronFile=${CRON_FILE:-/etc/crontab}
TempFile="$(mktemp)"
ServiceTemp="$(mktemp)"
trap 'rm -f "$TempFile" "$ServiceTemp"' EXIT

if [ ! -x "$ProjectDir/MultiTimer" ]; then
  echo "Executable not found: $ProjectDir/MultiTimer" >&2
  exit 1
fi

if [ ! -f "$ServiceSource" ]; then
  echo "Service file not found: $ServiceSource" >&2
  exit 1
fi

chmod +x \
  "$ProjectDir/StartService.sh" \
  "$ProjectDir/RebootPi.sh" \
  "$ProjectDir/FreeCheck.sh"

touch "$CronFile"

awk -v ProjectDir="$ProjectDir" '
  $0 == "# BEGIN MultiTimer" {
    Skip = 1
    next
  }
  $0 == "# END MultiTimer" {
    Skip = 0
    next
  }
  $0 == "# BEGIN MultiTimer maintenance" {
    Skip = 1
    next
  }
  $0 == "# END MultiTimer maintenance" {
    Skip = 0
    next
  }
  Skip {
    next
  }
  index( $0, ProjectDir "/" ) {
    next
  }
  index( $0, "/home/pi/Prj/MultiTimer/" ) {
    next
  }
  index( $0, "pgrep" ) && index( $0, "MultiTimer" ) {
    next
  }
  $0 == "# Start of and Periodic Check of SwitchTimer" {
    next
  }
  $0 == "# Reboot at Midnight" {
    next
  }
  $0 == "# Check Free every hour" {
    next
  }
  {
    print
  }
' "$CronFile" >"$TempFile"

cat >>"$TempFile" <<EOF_CRON

# BEGIN MultiTimer maintenance
# Reboot at midnight
1    0  *  *  * root     $ProjectDir/RebootPi.sh

# Check free memory every hour
12   *  *  *  * root     $ProjectDir/FreeCheck.sh
# END MultiTimer maintenance
EOF_CRON

MigrationBackup="$ProjectDir/generated/name-migration/$(date -u +%Y%m%dT%H%M%S%NZ)"
install -d -m 0700 "$MigrationBackup"
cp -p -- "$CronFile" "$MigrationBackup/crontab.before"
install -m 0644 "$TempFile" "$CronFile"
systemctl reload cron 2>/dev/null || true

systemctl disable --now "$ServiceName" 2>/dev/null || true
pkill -x MultiTimer 2>/dev/null || true

python3 - "$ServiceSource" "$ServiceTemp" "$RemotePrjsDir" <<'PY_SERVICE'
from pathlib import Path
import sys

source, target, root = map(Path, sys.argv[1:])
text = source.read_text()
if text.count('@RASPI_PRJS_DIR@') != 2:
    raise SystemExit('MultiTimer.service template has unexpected path markers')
target.write_text(text.replace('@RASPI_PRJS_DIR@', str(root)))
PY_SERVICE
install -m 0644 "$ServiceTemp" "$ServiceTarget"
for Marker in /run/MultiTimer.started; do
  if [ -e "$Marker" ]; then mv -- "$Marker" "$MigrationBackup/$(basename "$Marker").before"; fi
done

systemctl daemon-reload
systemctl reset-failed "$ServiceName" 2>/dev/null || true
systemctl enable --now "$ServiceName"

echo
echo "Installed service: $ServiceName"
echo "Removed old MultiTimer start and watch entries from $CronFile."
echo "Retained MultiTimer maintenance jobs in $CronFile."
echo
systemctl --no-pager --full status "$ServiceName"
