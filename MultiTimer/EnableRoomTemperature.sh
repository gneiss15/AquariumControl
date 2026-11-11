#!/usr/bin/env bash
# EnableRoomTemperature.sh, Version: 1.00
set -euo pipefail
ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
if [[ $EUID -ne 0 ]]; then
  exec sudo /bin/bash "$0" "$@"
fi
BootConfig=/boot/firmware/config.txt
if [[ ! -f "$BootConfig" || ! -r /proc/device-tree/model ]] || ! command -v dtoverlay >/dev/null; then
  echo "Run this script on the Raspberry Pi with its active /boot/firmware/config.txt." >&2
  exit 1
fi
python3 - "$ScriptDir" "$BootConfig" <<'PY_CHECK'
import json,pathlib,re,sys
project=pathlib.Path(sys.argv[1]);boot=pathlib.Path(sys.argv[2])
config=json.loads((project/'Config.json').read_text())
channels=json.loads((project/'data/Channels.json').read_text())
for channel in channels['Channels']:
    pins=[channel.get('InputPin'),channel.get('OutputPin')]
    output=channel.get('Output',{})
    if str(output.get('Driver','')).lower()=='gpio':pins.append(output.get('Pin'))
    if 4 in pins:raise SystemExit('GPIO4 is already assigned to a MultiTimer channel; no changes made.')
for line in boot.read_text().splitlines():
    line=line.strip()
    if re.match(r'dtoverlay\s*=\s*w1-gpio(?:,|$)',line) and line!='dtoverlay=w1-gpio,gpiopin=4':
        raise SystemExit('A different 1-Wire overlay is configured; inspect it before enabling GPIO4.')
room=config.get('RoomTemperature',{})
if not isinstance(room,dict):raise SystemExit('Existing RoomTemperature configuration must be an object.')
PY_CHECK
Backup="$ScriptDir/generated/room-temperature/$(date -u +%Y%m%dT%H%M%S%NZ)"
install -d -m 0700 "$Backup"
cp -p -- "$BootConfig" "$Backup/boot-config.before"
cp -p -- "$ScriptDir/Config.json" "$Backup/Config.json.before"
# Apply the overlay live: no Raspberry Pi reboot or relay reset is needed.
if ! compgen -G '/sys/bus/w1/devices/w1_bus_master*' >/dev/null; then
  dtoverlay w1-gpio gpiopin=4
fi
modprobe w1_therm
if ! grep -Fxq '# MultiTimer room temperature GPIO4' "$BootConfig"; then
  printf '\n[all]\n# MultiTimer room temperature GPIO4\ndtoverlay=w1-gpio,gpiopin=4\n' >> "$BootConfig"
fi
python3 - "$ScriptDir/Config.json" <<'PY_CONFIG'
import json,os,pathlib,sys,tempfile
path=pathlib.Path(sys.argv[1]);status=path.stat();data=json.loads(path.read_text())
room=data.setdefault('RoomTemperature',{});room['Enabled']=True;room.setdefault('SensorId','')
fd,name=tempfile.mkstemp(prefix='.room-temperature-',dir=path.parent)
try:
    with os.fdopen(fd,'w') as stream:
        json.dump(data,stream,ensure_ascii=False,indent=2);stream.write('\n');stream.flush();os.fsync(stream.fileno())
        os.fchmod(stream.fileno(),status.st_mode & 0o777);os.fchown(stream.fileno(),status.st_uid,status.st_gid)
    os.replace(name,path)
    fd=os.open(path.parent,os.O_DIRECTORY);os.fsync(fd);os.close(fd)
finally:
    if os.path.exists(name):os.unlink(name)
PY_CONFIG
systemctl restart MultiTimer.service
printf 'GPIO4 enabled live and for future boots. MultiTimer restarted.\nBackup: %s\n' "$Backup"
printf 'Sensor discovery may take a few seconds. Dashboard reports the result under System.\n'
