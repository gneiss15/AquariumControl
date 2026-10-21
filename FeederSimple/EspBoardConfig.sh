#!/usr/bin/env bash
# EspBoardConfig.sh, Version: 1.02

# Adjust these values before compiling for a different ESP8266 board or flash layout.
FQBN="esp8266:esp8266:nodemcuv2:eesz=4M2M"
BoardId="nodemcuv2"
LittleFsOffset="0x200000"
LittleFsSize=$(( 0x1FA000 ))
LittleFsBlockSize=8192
LittleFsPageSize=256