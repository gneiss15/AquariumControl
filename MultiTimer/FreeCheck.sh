#!/usr/bin/env bash
# FreeCheck.sh, Version: 1.01

ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
LogFile="$ScriptDir/Log/FreeCheck.log"
LogDateFmt="+%F %R:%S"

if [ ! -e $LogFile ]; then
  echo -e "                     " $(echo "gesamt benutzt frei"|cut -d ' ' -f 1- --output-delimiter='\t') >>$LogFile
fi
echo -e $(date "$LogDateFmt") ":" $(echo $(free -ht|grep Gesamt)|cut -d ' ' -f 2- --output-delimiter='\t') >>$LogFile
