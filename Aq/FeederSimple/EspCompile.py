# EspCompile.py, Version: 1.00
from __future__ import annotations

import argparse
import json
import pathlib
import struct
import sys


def arduino_data_dir() -> None:
  config = json.load(sys.stdin)
  if not isinstance(config, dict):
    raise ValueError("Arduino CLI configuration must be a JSON object")
  normalized = config.get("config", config)
  print(normalized["directories"]["data"])


def extract_signature(arguments: argparse.Namespace) -> None:
  source_path = pathlib.Path(arguments.source)
  signed_path = pathlib.Path(arguments.signed)
  payload_path = pathlib.Path(arguments.payload)
  signature_path = pathlib.Path(arguments.signature)

  source = source_path.read_bytes()
  signed = signed_path.read_bytes()
  if len(signed) < 5:
    raise ValueError("Signed file is too short")

  signature_size = struct.unpack("<I", signed[-4:])[0]
  signed_data_size = signature_size + 4
  if signature_size == 0 or signed_data_size > len(signed):
    raise ValueError("Invalid signature size")

  payload = signed[:-signed_data_size]
  signature = signed[-signed_data_size:-4]
  if payload != source:
    raise ValueError("Signed payload differs from source file")

  payload_path.write_bytes(payload)
  signature_path.write_bytes(signature)


def main() -> int:
  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(dest="command", required=True)
  subparsers.add_parser("arduino-data-dir")

  signature_parser = subparsers.add_parser("extract-signature")
  signature_parser.add_argument("source")
  signature_parser.add_argument("signed")
  signature_parser.add_argument("payload")
  signature_parser.add_argument("signature")

  arguments = parser.parse_args()
  if arguments.command == "arduino-data-dir":
    arduino_data_dir()
  elif arguments.command == "extract-signature":
    extract_signature(arguments)
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
