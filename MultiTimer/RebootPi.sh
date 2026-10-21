#!/usr/bin/env bash
# RebootPi.sh, Version: 1.02

###############################################################################
# Enshure current user is root
###############################################################################
if [[ $EUID -ne 0 ]]; then
  #echo -e "This script must be run as root\nRestart with sudo.." 1>&2
  exec sudo /bin/bash "$0" "$@"
fi
ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
LogFile="$ScriptDir/Log/MultiTimer.log"
LogDateFmt="+%F %R:%S"
##############################
# Start (main)
##############################
echo $(date "$LogDateFmt") Rebooting >> $LogFile
##############################
# Reboot
##############################
reboot

