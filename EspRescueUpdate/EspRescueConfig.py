# EspRescueConfig.py, Version: 1.00
from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile


WifiNames = ("WlanSsid", "WlanPw", "WlanSsid2", "WlanPw2")


def CppString(Value: object) -> str:
  if Value is None:
    Value = ""
  if not isinstance(Value, str):
    raise ValueError("WiFi configuration values must be strings")
  return json.dumps(Value, ensure_ascii=False)


def LoadJson(Path: pathlib.Path) -> object:
  return json.loads(Path.read_text(encoding="utf-8"))


def CollectOverrides(Secrets: object, ProjectName: str) -> dict[str, object]:
  if not isinstance(Secrets, dict):
    raise ValueError("Secrets root must be a JSON object")

  Overrides: dict[str, object] = {}
  for Key, Value in Secrets.items():
    if not isinstance(Value, dict):
      Overrides[Key] = Value

  Common = Secrets.get("Common", {})
  if isinstance(Common, dict):
    Overrides.update(Common)

  Project = Secrets.get(ProjectName, {})
  if isinstance(Project, dict):
    Overrides.update(Project)

  Projects = Secrets.get("Projects", {})
  if isinstance(Projects, dict):
    ProjectValues = Projects.get(ProjectName, {})
    if isinstance(ProjectValues, dict):
      Overrides.update(ProjectValues)

  return Overrides


def LoadConfigDefaults(ConfigDefinition: pathlib.Path) -> dict[str, object]:
  IncludePath = str(ConfigDefinition).replace("\\", "\\\\").replace('"', '\\"')
  Header = f'''\
#define CD( htmlType, typ, name, defValue, ... ) :#name:defValue,
#define CD_Div
#define WIFI_OFF 0
#define WIFI_STA 1
#define WIFI_AP 2
#define WIFI_AP_STA 3
#include "{IncludePath}"
'''

  with tempfile.TemporaryDirectory() as TempDir:
    HeaderPath = pathlib.Path(TempDir) / "RescueConfig.h"
    HeaderPath.write_text(Header, encoding="utf-8")
    Result = subprocess.run(
      ["gcc", "-E", str(HeaderPath)],
      check=True,
      capture_output=True,
      text=True,
    )

  ConfigLines = []
  for Line in Result.stdout.splitlines():
    if Line.startswith(":"):
      ConfigLines.append(" " + Line[1:])

  if not ConfigLines:
    raise ValueError("No CD entries found in config definition")

  ConfigLines[-1] = ConfigLines[-1].rstrip(",")
  Config = json.loads("{\n" + "\n".join(ConfigLines) + "\n}\n")
  if not isinstance(Config, dict):
    raise ValueError("Generated config defaults must be a JSON object")
  return Config


def Main() -> int:
  if len(sys.argv) != 5:
    raise ValueError(
      "Usage: EspRescueConfig.py <Config.inc.h> <Secrets.local.json> "
      "<ProjectName> <Output.h>"
    )

  ConfigDefinition = pathlib.Path(sys.argv[1]).resolve()
  SecretsPath = pathlib.Path(sys.argv[2])
  ProjectName = sys.argv[3]
  OutputPath = pathlib.Path(sys.argv[4])

  Config = LoadConfigDefaults(ConfigDefinition)

  if SecretsPath.is_file():
    Overrides = CollectOverrides(LoadJson(SecretsPath), ProjectName)
    for Name in WifiNames:
      if Name in Overrides:
        Config[Name] = Overrides[Name]

  Lines = [
    "#pragma once",
    f"#define ESP_RESCUE_PROJECT_NAME {CppString(ProjectName)}",
  ]
  MacroNames = {
    "WlanSsid": "ESP_RESCUE_WLAN_SSID",
    "WlanPw": "ESP_RESCUE_WLAN_PW",
    "WlanSsid2": "ESP_RESCUE_WLAN_SSID2",
    "WlanPw2": "ESP_RESCUE_WLAN_PW2",
  }
  for Name in WifiNames:
    Lines.append(f"#define {MacroNames[Name]} {CppString(Config.get(Name, ''))}")
  Lines.append("")

  OutputPath.write_text("\n".join(Lines), encoding="utf-8")
  return 0


if __name__ == "__main__":
  raise SystemExit(Main())
