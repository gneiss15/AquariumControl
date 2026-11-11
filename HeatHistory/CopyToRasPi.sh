#!/usr/bin/env bash
# CopyToRasPi.sh, Version: 1.00

set -euo pipefail

ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd -- "$ScriptDir"
RemoteHost="${RASPI_HOST:-pi@192.168.99.51}"
RemotePrjsDir="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
RemoteDir="$RemotePrjsDir/HeatHistory"

Fail()
{
  echo "Error: $*" >&2
  exit 1
}

CopyToRasPi()
{
  if [[ ! -t 0 ]]; then
    Fail "An interactive confirmation is required before transferring HeatHistory."
  fi

  read -r -p "HeatHistory-Quellstand nach $RemoteHost:$RemoteDir übertragen? Laufzeitdaten, Konfiguration und der Pi-Dienst bleiben unverändert. [j/N] " Answer
  case "$Answer" in
    j|J|ja|Ja|JA|y|Y|yes|Yes|YES)
      ;;
    *)
      echo "Übertragung abgebrochen. Raspberry Pi und HeatHistory-Laufzeitdaten wurden nicht verändert."
      return 0
      ;;
  esac

  command -v rsync >/dev/null 2>&1 || Fail "rsync not installed or not in PATH"
  command -v ssh >/dev/null 2>&1 || Fail "ssh not installed or not in PATH"

  echo "Synchronisiere HeatHistory nach $RemoteHost:$RemoteDir/ ..."
  ssh "$RemoteHost" "mkdir -p '$RemoteDir'" || return $?
  rsync -v -a \
    BootSession.py \
    BrowserServer.py \
    Charts.html \
    CopyToRasPi.sh \
    CreateCharts.py \
    CsvFiles.py \
    CsvStreaming.py \
    DashboardEvents.py \
    EnergyHistory.py \
    ExportActive.py \
    HeatHistory.py \
    HeatHistory.js \
    Install.sh \
    Messages.js \
    Messages.py \
    MoveToRasPi.sh \
    PiHistory.py \
    PrjRegeln.md \
    README.DE.md \
    README.md \
    StorageRetention.py \
    UdpDescriptions.py \
    Versions.txt \
    config.example.json \
    "$RemoteHost:$RemoteDir/" || return $?

  echo "Synchronisation abgeschlossen. Für die Laufzeitinstallation auf dem Pi dort separat ./Install.sh ausführen."
}

CopyToRasPi
