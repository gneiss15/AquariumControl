#!/usr/bin/env bash
# EspBoardConfig.sh, Version: 1.02

# Select the Nous A8T target once for Main and Rescue.
BoardId="esp32"
FQBN="esp32:esp32:esp32:FlashSize=4M,FlashMode=dio,FlashFreq=40,PartitionScheme=default"
BinSubDir="${BoardId}_Arduino2"
PartitionDefinition="$ProjectDir/partitions.csv"
MainPartitionSize=$(( 0x2D0000 ))
RescuePartitionSize=$(( 0xD0000 ))
LittleFsSize=$(( 0x50000 ))
LittleFsBlockSize=4096
LittleFsPageSize=256
