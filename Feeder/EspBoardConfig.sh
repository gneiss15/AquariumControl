#!/usr/bin/env bash
# EspBoardConfig.sh, Version: 1.00

# Select the ESP8266 target once for both Main and Rescue builds.
BoardId="wifi_kit_8"
FQBN="esp8266:esp8266:wifi_kit_8:eesz=4M2M"
BinSubDir="${BoardId}_Arduino2"
LittleFsSize=$(( 0x1FA000 ))
LittleFsBlockSize=8192
LittleFsPageSize=256
