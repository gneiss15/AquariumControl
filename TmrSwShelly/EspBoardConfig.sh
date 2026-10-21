#!/usr/bin/env bash
# EspBoardConfig.sh, Version: 1.01

# Shared production layout; the CSV remains authoritative.
BoardId="esp32c3"
IdfTarget="esp32c3"
BinSubDir="${BoardId}_Arduino2"
PartitionDefinition="$ProjectDir/partitions.csv"
MainPartitionSize=$((0x2A0000))
RescuePartitionSize=$((0x180000))
LittleFsSize=$((0xE0000))
LittleFsBlockSize=4096
LittleFsPageSize=256
