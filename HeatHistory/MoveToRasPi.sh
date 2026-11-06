#!/usr/bin/env bash
# MoveToRasPi.sh, Version: 1.00
set -Eeuo pipefail
# Running this explicit option authorizes permanent expiry of closed own Pi logs.
if [[ $# != 1 || $1 != --allow-log-expiry ]]; then
  echo "Usage: $0 --allow-log-expiry"
  echo 'Behält alle PC-Daten. Auf dem Pi werden eigene abgeschlossene Logs nach 30 Tagen bzw. bei 512 MiB endgültig entfernt.'
  exit 1
fi
ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
RuntimeDir="$HOME/.local/share/HeatHistory"
PiHost="${HEATHISTORY_PI_HOST:-pi@192.168.99.51}"
PiRoot=/home/pi/.local/share/HeatHistory
PiProject=/home/pi/Prjs/HeatHistory
Stamp="$(date -u +%Y%m%dT%H%M%SZ)"
ExportDir="$ProjectDir/generated/PiMigration-$Stamp"
ssh -o BatchMode=yes "$PiHost" "test ! -e '$PiRoot/config.json' && ! systemctl is-active --quiet HeatHistory.service" || {
  echo 'Pi-Ziel ist bereits belegt oder nicht erreichbar; nichts gestoppt.' >&2; exit 1;
}
sudo -v
Stopped=false
rollback() {
  local Result=$?
  if [[ "$Stopped" == true ]]; then
    echo 'Umzug nicht bestätigt; PC-Aufzeichnung wird wieder gestartet.' >&2
    ssh -o BatchMode=yes "$PiHost" 'Pid=$(systemctl show HeatHistory.service -p MainPID --value); if [ "${Pid:-0}" -gt 1 ]; then kill -TERM "$Pid"; fi' || true
    sudo systemctl start HeatHistory.service || true
  fi
  exit "$Result"
}
trap rollback EXIT
sudo systemctl stop HeatHistory.service
Stopped=true
python3 "$ProjectDir/ExportActive.py" "$RuntimeDir" "$ExportDir"
ssh "$PiHost" "mkdir -p '$PiProject' '$PiRoot'"
rsync -a --exclude generated/ --exclude __pycache__/ --exclude tests/ "$ProjectDir/" "$PiHost:$PiProject/"
rsync -a "$ExportDir/" "$PiHost:$PiRoot/"
echo 'Jetzt benötigt die Installation auf dem Pi dessen sudo-Passwort.'
ssh -t "$PiHost" "cd '$PiProject' && ./Install.sh --allow-log-expiry"
python3 - "$PiHost" <<'PY_VERIFY'
import json,subprocess,sys,time,urllib.request
host=sys.argv[1]
for attempt in range(12):
    time.sleep(5)
    try:
        with urllib.request.urlopen('http://192.168.99.51:8091/api/history?range=1h',timeout=20) as response:document=json.load(response)
        now=time.time();fresh={}
        import datetime
        for profile in document['Profiles']:
            rows=profile['Energy']
            if rows:fresh[profile['Id']]=now-datetime.datetime.fromisoformat(rows[-1][0].replace('Z','+00:00')).timestamp()
        if all(0<=fresh.get(key,999)<=30 for key in ('HeatProfileAq1','HeatProfileAq2')):
            print('Pi-Aufzeichnung bestätigt:',fresh)
            break
    except (OSError,ValueError,KeyError):pass
else:
    raise SystemExit('Keine frischen Daten beider Aquarien; PC wird wieder gestartet. Pi-Dienst bitte prüfen.')
PY_VERIFY
sudo systemctl disable HeatHistory.service
Stopped=false
trap - EXIT
printf '\nHeatHistory läuft auf dem Pi: http://192.168.99.51:8091/\nPC-Daten unverändert: %s/data/\nExportnachweis: %s\n' "$RuntimeDir" "$ExportDir"
