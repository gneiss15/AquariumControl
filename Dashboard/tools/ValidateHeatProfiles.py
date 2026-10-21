#!/usr/bin/env python3
# ValidateHeatProfiles.py, Version: 1.03

from datetime import datetime
import math
import re
import json
import sys
from pathlib import Path


RequiredKeys = (
  "Id",
  "TempId",
  "FloorOn",
  "FloorOff",
  "MainOn",
  "MainOff",
  "NoTempOn",
)


def Fail(Message):
  print(f"Error: {Message}", file=sys.stderr)
  raise SystemExit(1)


def IsNumber(Value):
  return isinstance(Value, (int, float)) and not isinstance(Value, bool)


def ValidateProfile(Profile, Index):
  if not isinstance(Profile, dict):
    Fail(f"Profiles[{Index}] is not an object")

  Missing = [Key for Key in RequiredKeys if Key not in Profile]
  if Missing:
    Fail(f"Profiles[{Index}] missing: {', '.join(Missing)}")

  Unknown = sorted(set(Profile) - set(RequiredKeys) - {"Measurement"})
  if Unknown:
    Fail(f"Profiles[{Index}] unknown key(s): {', '.join(Unknown)}")

  if "Measurement" in Profile:
    Field = Profile["Measurement"]
    if not isinstance(Field, str) or not re.fullmatch(r"[A-Za-z0-9_]{1,64}", Field) or Field == "Kind":
      Fail(f"Profiles[{Index}].Measurement must be a valid status field name")

  ProfileId = Profile["Id"]
  TempId = Profile["TempId"]
  if not isinstance(ProfileId, str) or not ProfileId:
    Fail(f"Profiles[{Index}].Id must be a non-empty string")
  if not isinstance(TempId, str) or not TempId:
    Fail(f"Profiles[{Index}].TempId must be a non-empty string")

  for Key in ("FloorOn", "FloorOff", "MainOn", "MainOff"):
    if not IsNumber(Profile[Key]) or not math.isfinite(Profile[Key]):
      Fail(f"Profiles[{Index}].{Key} must be numeric")

  if Profile["FloorOn"] >= Profile["FloorOff"]:
    Fail(f"{ProfileId}: FloorOn must be < FloorOff")
  if Profile["MainOn"] >= Profile["MainOff"]:
    Fail(f"{ProfileId}: MainOn must be < MainOff")
  if not isinstance(Profile["NoTempOn"], bool):
    Fail(f"{ProfileId}: NoTempOn must be boolean")

  return ProfileId


def main():
  if len(sys.argv) != 2:
    Fail("usage: ValidateHeatProfiles.py HeatProfiles.json")

  FileName = Path(sys.argv[1])
  try:
    Data = json.loads(FileName.read_text(encoding="utf-8"))
  except (OSError, json.JSONDecodeError) as Error:
    Fail(str(Error))

  if not isinstance(Data, dict) or set(Data) not in ({"Profiles"}, {"Profiles", "TimeStamp"}):
    Fail("root must contain Profiles and optional global TimeStamp")

  if "TimeStamp" in Data:
    TimeStamp = Data["TimeStamp"]
    if not isinstance(TimeStamp, str):
      Fail("TimeStamp must be an ISO-8601 UTC string")
    if not re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z", TimeStamp):
      Fail("TimeStamp must use YYYY-MM-DDTHH:MM:SS.sssZ")
    try:
      datetime.strptime(TimeStamp, "%Y-%m-%dT%H:%M:%S.%fZ")
    except ValueError:
      Fail("TimeStamp must use YYYY-MM-DDTHH:MM:SS.sssZ")

  Profiles = Data["Profiles"]
  if not isinstance(Profiles, list):
    Fail("Profiles must be an array")

  Seen = set()
  for Index, Profile in enumerate(Profiles):
    ProfileId = ValidateProfile(Profile, Index)
    if ProfileId in Seen:
      Fail(f"duplicate profile Id: {ProfileId}")
    Seen.add(ProfileId)

  print(f"Heat profiles valid: {len(Profiles)}")


if __name__ == "__main__":
  main()
