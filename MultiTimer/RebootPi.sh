#!/usr/bin/env bash
# RebootPi.sh, Version: 1.03

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
if [[ "${GN_DEBUG_FILE_LOG:-0}" == 1 ]]; then
  printf '%s Rebooting\n' "$(date "$LogDateFmt")" >> "$LogFile"
fi
##############################
# Reboot
##############################
reboot

