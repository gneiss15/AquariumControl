# ResolveDeviceAddress.py, Version: 1.02
"""Resolve a function-named DevState directory to the current fixed device address."""
import ipaddress
import json
import pathlib
import sys

def resolve(name: str, table: pathlib.Path) -> str:
  document = json.loads(table.read_text(encoding="utf-8"))
  rows = document.get("Devices")
  if not isinstance(rows, list): raise ValueError("Invalid device assignment table")
  matches = [row for row in rows if isinstance(row, dict) and row.get("Function") == name]
  if len(matches) != 1: raise ValueError(f"Unknown/ambiguous device function: {name}")
  return str(ipaddress.IPv4Address(matches[0]["IP"]))

if __name__ == "__main__":
  if len(sys.argv) != 2: raise SystemExit("Usage: ResolveDeviceAddress.py FUNCTION")
  try: print(resolve(sys.argv[1], pathlib.Path(__file__).resolve().parent.parent / "Dashboard/data/DeviceAssignments.json"))
  except (OSError, ValueError, KeyError) as error: raise SystemExit(str(error))
