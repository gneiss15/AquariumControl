# UpdateVersionFiles.py, Version: 1.14
from __future__ import annotations

import argparse
import json
import os
import pathlib
import re


REVISION_PATTERN = re.compile(r"[0-9A-Za-z][0-9A-Za-z._+-]*")
VERSION_KEYS = ("Firmware", "LittleFS", "Rescue", "Web", "Server", "Data", "App")
HEADER_NAMES = {
  "Firmware": "FirmwareRevision",
  "LittleFS": "LittleFsRevision",
  "Rescue": "RescueTargetRevision",
  "Web": "WebRevision",
  "Server": "ServerRevision",
  "Data": "DataRevision",
  "App": "AppRevision",
}


def write_if_changed(path: pathlib.Path, content: str) -> None:
  if path.exists() and path.read_text(encoding="utf-8") == content:
    return
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(content, encoding="utf-8")


def read_versions(path: pathlib.Path) -> dict[str, str]:
  versions: dict[str, str] = {}
  for line_number, raw_line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
    line = raw_line.strip()
    if not line or line.startswith("#"):
      continue
    if "=" not in line:
      raise ValueError(f"{path}:{line_number}: expected NAME=REVISION")
    key, revision = (part.strip() for part in line.split("=", 1))
    if key not in VERSION_KEYS:
      raise ValueError(f"{path}:{line_number}: unknown version key: {key}")
    if key in versions:
      raise ValueError(f"{path}:{line_number}: duplicate version key: {key}")
    if not REVISION_PATTERN.fullmatch(revision):
      raise ValueError(
        f"{path}:{line_number}: invalid revision for {key}; allowed characters are "
        "letters, digits, '.', '_', '+', and '-'"
      )
    versions[key] = revision

  if not versions:
    raise ValueError(f"{path}: at least one version key is required")
  return versions


def update_project(project_dir: pathlib.Path) -> None:
  versions_file = project_dir / "Versions.txt"
  if not versions_file.is_file():
    raise FileNotFoundError(f"Version file not found: {versions_file}")

  versions = read_versions(versions_file)
  header_versions = versions
  if "Server" in versions and "Data" in versions:
    header_versions = {key: revision for key, revision in versions.items() if key != "Data"}

  generated_header = "// Version.inc.h, generated from Versions.txt - do not edit.\n#pragma once\n\nnamespace ProjectVersion\n {\n"
  if "Firmware" not in header_versions:
    generated_header += 'constexpr char FirmwareRevision[] = "";\n'
  if "Rescue" not in header_versions:
    generated_header += 'constexpr char RescueTargetRevision[] = "";\n'
  for key, revision in header_versions.items():
    generated_header += f'constexpr char {HEADER_NAMES[key]}[] = "{revision}";\n'
  generated_header += " }\n"
  source_dir = project_dir / "Sw" if (project_dir / "Sw").is_dir() else project_dir
  write_if_changed(source_dir / "Version.inc.h", generated_header)
  project_revision = versions.get("Firmware", versions.get("App", versions.get("Server")))
  if project_revision is not None and (project_dir / "CMakeLists.txt").is_file():
    write_if_changed(
      project_dir / "generated" / "ProjectVersion.cmake",
      f'set(PROJECT_VER "{project_revision}")\n',
    )
  if "LittleFS" in versions:
    data_dir = pathlib.Path(os.environ.get("PRJS_DATA_DIR", str(project_dir / "generated" / "data")))
    write_if_changed(data_dir / "Versions.txt", f"LittleFS={versions['LittleFS']}\n")
  if "Web" in versions:
    web_src_dir = pathlib.Path(os.environ.get("PRJS_WEB_SRC_DIR", str(project_dir / "WebSrc")))
    write_if_changed(web_src_dir / "Versions.txt", f"Web={versions['Web']}\n")
  if "Server" in versions and "Data" in versions:
    data_version = json.dumps({"DataVersion": versions["Data"]}, indent=2) + "\n"
    write_if_changed(project_dir / "data" / "Version.json", data_version)
  for key, revision in versions.items():
    print(f"{key} revision: {revision}")


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("project_dir", type=pathlib.Path)
  arguments = parser.parse_args()
  update_project(arguments.project_dir.resolve())
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
