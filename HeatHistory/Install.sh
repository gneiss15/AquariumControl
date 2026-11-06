#!/usr/bin/env bash
# Install.sh, Version: 1.11
set -Eeuo pipefail
NewSeries=false
RollingRetention=false
for Option in "$@"; do
  case "$Option" in
    --new-series) NewSeries=true ;;
    --allow-log-expiry) RollingRetention=true ;;
    *) echo "Usage: $0 [--new-series] [--allow-log-expiry]" >&2; exit 1 ;;
  esac
done
ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
ServiceUser="${SUDO_USER:-$USER}"
if [[ "$ServiceUser" == root ]]; then
  echo 'Als normaler Benutzer aufrufen; sudo wird bei Bedarf verwendet.' >&2
  exit 1
fi
UserHome="$(getent passwd "$ServiceUser" | cut -d: -f6)"
RuntimeDir="$UserHome/.local/share/HeatHistory"
if [[ "$EUID" != 0 ]]; then
  exec sudo "$ProjectDir/Install.sh" "$@"
fi
install -d -o "$ServiceUser" -g "$(id -gn "$ServiceUser")" "$RuntimeDir"
# Existing configuration and historical data are authoritative.
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
if [[ "$NewSeries" == true ]]; then
  python3 - "$RuntimeDir" "$ServiceUser" "$ProjectDir/config.example.json" "$ProjectDir/generated/energy-calibration-start.json" <<'PY_SERIES'
import datetime,json,os,pathlib,pwd,sys
root=pathlib.Path(sys.argv[1]);user=pwd.getpwnam(sys.argv[2]);config=json.loads((root/'config.json').read_text())
data=pathlib.Path(config.get('DataDir','data'))
if not data.is_absolute():data=root/data
if data.resolve()!=root/'data' or data.is_symlink():raise SystemExit('New series requires the regular runtime data directory; existing data left untouched.')
stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
archive=root/'archive'/stamp;archive.mkdir(parents=True,mode=0o700)
(archive/'config.before.json').write_text(json.dumps(config,indent=2)+'\n')
if data.exists():data.rename(archive/'data')
data.mkdir();os.chown(data,user.pw_uid,user.pw_gid)
for key in ['FloorPowerWatts','RoomTemperatureSource','PowerFactors','EnergyMaxIntervalSeconds']:
    config.setdefault(key,json.loads(pathlib.Path(sys.argv[3]).read_text())[key])
snapshot=pathlib.Path(sys.argv[4])
if snapshot.is_file():config['PowerCalibrationAtStart']=json.loads(snapshot.read_text())
config['SeriesStartedAt']=datetime.datetime.now(datetime.timezone.utc).isoformat()
(root/'config.json').write_text(json.dumps(config,indent=2)+'\n')
os.chown(root/'config.json',user.pw_uid,user.pw_gid)
for path in [archive,archive/'config.before.json',root/'archive']:os.chown(path,user.pw_uid,user.pw_gid)
print('Archived previous data:',archive)
PY_SERIES
fi
if [[ "$RollingRetention" == true ]]; then
  python3 - "$RuntimeDir/config.json" <<'PY_RETENTION'
import json,pathlib,sys
path=pathlib.Path(sys.argv[1]);config=json.loads(path.read_text())
config.update(StorageDeleteExpired=True,StorageRetentionDays=30,StorageMaxBytes=512*1024*1024,ViewRetentionDays=30)
path.write_text(json.dumps(config,indent=2)+'\n')
print('Explicit log expiry enabled: closed own logs after 30 days or at 512 MiB.')
PY_RETENTION
fi
for Source in DashboardEvents.py CsvFiles.py CsvStreaming.py BootSession.py StorageRetention.py HeatHistory.py BrowserServer.py Charts.html HeatHistory.js Messages.js EnergyHistory.py PiHistory.py UdpDescriptions.py Messages.py Versions.txt; do
  install -o "$ServiceUser" -g "$(id -gn "$ServiceUser")" -m 644 "$ProjectDir/$Source" "$RuntimeDir/$Source"
done
install -m 644 "$StageDir/HeatHistory.service" /etc/systemd/system/HeatHistory.service
systemctl daemon-reload
systemctl enable HeatHistory.service
systemctl restart HeatHistory.service
systemctl --no-pager --full status HeatHistory.service
printf '\nDaten und Konfiguration: %s\nBrowser: http://localhost:8091/\n' "$RuntimeDir"
