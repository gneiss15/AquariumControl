#!/usr/bin/env python3
# ChargeLearning.py, Version: 1.06
"""Persistent laptop learning permission, shared by sender and command-line tool."""
import argparse
from contextlib import contextmanager
import fcntl
import os
from pathlib import Path
import uuid
import sys
import tempfile
from Messages import ReadJson, WriteJson

MaximumCounter = 0xFFFFFFFF


def StatePath():
  return Path.home() / ".local/state/LaptopBattery/ChargeLearning.json"


@contextmanager
def Locked(PathName):
  PathName.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
  Descriptor = os.open(str(PathName) + ".lock", os.O_CREAT | os.O_RDWR, 0o600)
  with os.fdopen(Descriptor, "a") as Lock:
    fcntl.flock(Lock, fcntl.LOCK_EX)
    yield


def WriteState(PathName, State):
  Temporary = None
  try:
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=PathName.parent,
                                     prefix=PathName.name + ".", delete=False) as Output:
      Temporary = Path(Output.name)
      Output.write(WriteJson("TJsonBatteryLearningState", State, indent=2))
      Output.write("\n")
      Output.flush()
      os.fsync(Output.fileno())
    os.replace(Temporary, PathName)
    Directory = os.open(PathName.parent, os.O_RDONLY | os.O_DIRECTORY)
    try:
      os.fsync(Directory)
    finally:
      os.close(Directory)
  finally:
    if Temporary is not None and Temporary.exists():
      Temporary.unlink()


def ReadBootId():
  BootId = Path("/proc/sys/kernel/random/boot_id").read_text(encoding="ascii").strip()
  return str(uuid.UUID(BootId))


def ReadState(PathName):
  Changed = False
  try:
    with PathName.open(encoding="utf-8") as Input:
      State = ReadJson("TJsonBatteryLearningState", Input.read())
  except FileNotFoundError:
    State = {"ChargeLearnAllowed": True, "ChargeBootCnt": 0, "LastBootId": ""}
    Changed = True
  Counter = State["ChargeBootCnt"]
  if type(Counter) is not int or not 0 <= Counter <= MaximumCounter or type(State["ChargeLearnAllowed"]) is not bool or not isinstance(State["LastBootId"],str):
    raise ValueError("Invalid persistent learning state")
  BootId = ReadBootId()
  if State["LastBootId"] != BootId:
    State["ChargeBootCnt"] = (Counter + 1) & MaximumCounter
    State["LastBootId"] = BootId
    Changed = True
  if Changed:
    WriteState(PathName, State)
  return State


def LoadState(PathName=None):
  PathName = Path(PathName) if PathName is not None else StatePath()
  with Locked(PathName):
    return ReadState(PathName)


def SetLearning(Enabled, PathName=None):
  """None toggles the current permission; explicit ON/OFF commands are idempotent."""
  if Enabled is not None and type(Enabled) is not bool:
    raise ValueError("Enabled must be boolean or None")
  PathName = Path(PathName) if PathName is not None else StatePath()
  with Locked(PathName):
    State = ReadState(PathName)
    NewEnabled = not State["ChargeLearnAllowed"] if Enabled is None else Enabled
    if State["ChargeLearnAllowed"] != NewEnabled:
      State["ChargeLearnAllowed"] = NewEnabled
      WriteState(PathName, State)
    return State


def Main():
  Parser = argparse.ArgumentParser(description=__doc__)
  Parser.add_argument("Command", nargs="?", default="status",
                      choices=("on", "off", "status", "toggle", "ein", "aus"))
  Parser.add_argument("--state-file", type=Path, help="Override state path for both testing and manual inspection")
  Args = Parser.parse_args()
  try:
    if Args.Command == "status":
      State = LoadState(Args.state_file)
    else:
      Enabled = None if Args.Command == "toggle" else Args.Command in ("on", "ein")
      State = SetLearning(Enabled, Args.state_file)
    Mode = "ON" if State["ChargeLearnAllowed"] else "OFF"
    print(f"Charge learning: {Mode}; boot counter: {State['ChargeBootCnt']}")
    return 0
  except (OSError, ValueError) as Error:
    print(f"Charge learning failed: {Error}", file=sys.stderr)
    return 1


if __name__ == "__main__":
  sys.exit(Main())
