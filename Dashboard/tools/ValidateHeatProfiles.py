#!/usr/bin/env python3
# ValidateHeatProfiles.py, Version: 1.09

import json
import sys
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Libs/GnAqPrjsLib"))
from Messages import ValidateType


def Fail(Message):
  print(f"Error: {Message}", file=sys.stderr)
  raise SystemExit(1)


def ValidateProfile(Profile, Index):
  if not ValidateType("TJsonProfile", Profile):
    Fail(f"Profiles[{Index}] does not match MessageDefinition.inc.h")
  return Profile["ProfName"]


def main():
  if len(sys.argv) != 2:
    Fail("usage: ValidateHeatProfiles.py HeatProfiles.json")

  FileName = Path(sys.argv[1])
  try:
    Data = json.loads(FileName.read_text(encoding="utf-8"))
  except (OSError, json.JSONDecodeError) as Error:
    Fail(str(Error))

  if not ValidateType("TJsonProfiles", Data):
    Fail("HeatProfiles.json does not match MessageDefinition.inc.h")
  Profiles = Data["Profs"]
  Seen = set()
  for Index, Profile in enumerate(Profiles):
    ProfileId = ValidateProfile(Profile, Index)
    if ProfileId in Seen:
      Fail(f"duplicate profile Id: {ProfileId}")
    Seen.add(ProfileId)

  print(f"Heat profiles valid: {len(Profiles)}")


if __name__ == "__main__":
  main()
