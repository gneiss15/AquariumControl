#!/usr/bin/env bash
# CopyToRasPi.sh, Version: 1.15

set -euo pipefail

ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(cd -- "$ScriptDir/.." && pwd -P)"
RemoteHost="${RASPI_HOST:-pi@192.168.99.51}"
RemotePrjsDir="${RASPI_PRJS_DIR:-/home/pi/Prjs}"
RemoteDir="$RemotePrjsDir/MultiTimer"
CommonLibDir="$PrjsDir/Libs/GnBasicLib"
RemoteCommonLibDir="$RemotePrjsDir/Libs/GnBasicLib"

PauseOnError()
{
  if [ -t 0 ]; then
    read -r -p "Press enter to continue (exit)" _ || true
  fi
}

CopyToRasPi()
{
  if [ ! -t 0 ]; then
    echo "An interactive confirmation is required before transferring MultiTimer." >&2
    return 1
  fi
  read -r -p "MultiTimer-Quellstand und statische Anwendungsdateien zum Pi übertragen? Laufende Pi-Daten (Config, Channels, Schaltzeiten und Syncstatus) bleiben erhalten. [j/N] " Answer
  case "$Answer" in
    j|J|ja|Ja|JA|y|Y|yes|Yes|YES)
      ;;
    *)
      echo "Übertragung abgebrochen. Weder Build noch Raspberry Pi wurden verändert."
      return 0
      ;;
  esac

  echo "Erzeuge MultiTimer-Daten aus WebSrc ..."
  "$ScriptDir/PreBuild.sh" || return $?

  echo "Synchronisiere Aq/MultiTimer nach $RemoteHost:$RemoteDir/ ..."
  ssh "$RemoteHost" "mkdir -p '$RemoteDir' '$RemoteCommonLibDir' '$RemotePrjsDir/tools'" || return $?

  (
    cd "$ScriptDir"
    rsync -v -a \
      Makefile \
      MultiTimer.service \
      Versions.txt \
      Install.sh \
      StartService.sh \
      RebootPi.sh \
      FreeCheck.sh \
      "$RemoteHost:$RemoteDir/"
    rsync -v -a "$PrjsDir/tools/UpdateVersionFiles.py" "$PrjsDir/tools/_TrashPaths.sh" \
      "$RemoteHost:$RemotePrjsDir/tools/"
    rsync -v -a \
      --exclude='*Esp*.*' \
      --exclude='HardwareDummy.*' \
      --exclude='/data/Config.json' \
      --exclude='/data/Channels.json' \
      --exclude='/data/SwitchTimes.json' \
      --exclude='/data/SwitchTimes.sync.json' \
      --exclude='/data/Identity.json' \
      --exclude='/data/Identity.json.new' \
      --exclude='/data/Configuration.transaction.json' \
      --exclude='/data/*.new' \
      include \
      src \
      data \
      "$RemoteHost:$RemoteDir/"
  ) || return $?

  echo "Synchronisiere benötigte GnBasicLib-Quellen ..."
  rsync -v -a \
    --include='*/' \
    --include='*.h' \
    --include='*.cpp' \
    --exclude='*' \
    "$CommonLibDir/" "$RemoteHost:$RemoteCommonLibDir/" || return $?

  echo "Synchronisation abgeschlossen."
}

if CopyToRasPi; then
  exit 0
else
  Status=$?
  echo "Fehler bei der Synchronisation zum Raspberry Pi." >&2
  PauseOnError
  exit "$Status"
fi
