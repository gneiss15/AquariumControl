# EspCompile.py, Version: 1.01
from __future__ import annotations

import argparse
import csv
import json
import pathlib
import sys


def arduino_data_dir() -> None:
  config = json.load(sys.stdin)
  if not isinstance(config, dict):
    raise ValueError("Arduino CLI configuration must be a JSON object")
  normalized = config.get("config", config)
  print(normalized["directories"]["data"])


def parse_size(value: str) -> int:
  normalized = value.strip()
  suffix = normalized[-1:].upper()
  factor = 1
  if suffix == "K":
    factor = 1024
    normalized = normalized[:-1]
  elif suffix == "M":
    factor = 1024 * 1024
    normalized = normalized[:-1]
  return int(normalized, 0) * factor


def littlefs_partition(path: pathlib.Path) -> None:
  for line in path.read_text(encoding="utf-8").splitlines():
    normalized = line.strip()
    if not normalized or normalized.startswith("#"):
      continue
    row = next(csv.reader([normalized]))
    if len(row) < 5:
      continue
    _, partition_type, subtype, offset, size = [field.strip() for field in row[:5]]
    if partition_type == "data" and subtype in {"spiffs", "littlefs"}:
      print(offset, parse_size(size))
      return
  raise ValueError("No SPIFFS/LittleFS data partition found")


def main() -> int:
  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(dest="command", required=True)
  subparsers.add_parser("arduino-data-dir")
  partition_parser = subparsers.add_parser("littlefs-partition")
  partition_parser.add_argument("partitions_csv", type=pathlib.Path)
  arguments = parser.parse_args()

  if arguments.command == "arduino-data-dir":
    arduino_data_dir()
  elif arguments.command == "littlefs-partition":
    littlefs_partition(arguments.partitions_csv)
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
