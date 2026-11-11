#!/usr/bin/env python3
# BatteryUdpSender.py, Version: 1.10

from Messages import WriteMessage, NewStatus, BroadcastAddress, UdpPort, SendIntervalSeconds
from ChargeLearning import LoadState
import re
import socket
import sys
import time

SendInterval = SendIntervalSeconds
BatteryCapacityFile = "/sys/class/power_supply/BAT1/capacity"
AcOnlineFile = "/sys/class/power_supply/ACAD/online"
BatteryStateFile = "/sys/class/power_supply/BAT1/status"
LaptopMacFile = "/sys/class/net/wlp3s0/address"


def ReadCapacity():
  try:
    with open(BatteryCapacityFile, encoding="ascii") as BatteryFile:
      Capacity = int(BatteryFile.read().strip())
    return Capacity if 0 <= Capacity <= 100 else None
  except (OSError, ValueError):
    return None


def ReadAcOnline():
  try:
    with open(AcOnlineFile, encoding="ascii") as AcFile:
      AcOnline = AcFile.read().strip()
    return {"1": True, "0": False}.get(AcOnline)
  except OSError:
    return None


def ReadBatteryState():
  try:
    with open(BatteryStateFile, encoding="ascii") as StateFile:
      State = StateFile.read().strip()
    return {
      "Charging": "Charging",
      "Discharging": "Discharging",
      "Full": "Idle",
      "Not charging": "Idle",
    }.get(State, "Unknown")
  except OSError:
    return "Unknown"


def Main():
  try:
    with open(LaptopMacFile, encoding="ascii") as MacFile:
      LaptopMac = MacFile.read().strip().upper()
  except OSError as Error:
    print(f"Cannot read laptop MAC: {Error}", file=sys.stderr)
    return 1

  if not re.fullmatch(r"(?:[0-9A-F]{2}:){5}[0-9A-F]{2}", LaptopMac):
    print("Invalid laptop MAC address", file=sys.stderr)
    return 1

  DeviceId = f"{LaptopMac}@3"
  with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as UdpSocket:
    UdpSocket.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    while True:
      try:
        Learning = LoadState()
      except (OSError, ValueError) as Error:
        print(f"Charge learning disabled: {Error}", file=sys.stderr, flush=True)
        Learning = {"ChargeLearnAllowed": False, "ChargeBootCnt": None}
      Payload = NewStatus(ID=DeviceId, Status={
          "Kind": "LaptopPower",
          "BatteryPct": ReadCapacity(),
          "AcOnline": ReadAcOnline(),
          "BatteryState": ReadBatteryState(),
          "ChargeLearnAllowed": Learning["ChargeLearnAllowed"],
          "BootCnt": Learning["ChargeBootCnt"],
        })
      try:
        UdpSocket.sendto(WriteMessage(Payload).encode("utf-8"),
                         (BroadcastAddress, UdpPort))
      except OSError as Error:
        print(f"UDP send failed: {Error}", file=sys.stderr, flush=True)
      time.sleep(SendInterval)


if __name__ == "__main__":
  sys.exit(Main())
