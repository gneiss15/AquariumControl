#!/usr/bin/env python3
# ValidateJsonFiles.py, Version: 1.00

import json
import sys
from pathlib import Path


def collect_json_files(arguments):
  files = []
  seen = set()
  for argument in arguments:
    path = Path(argument)
    if path.is_dir():
      candidates = sorted(path.glob("*.json"))
    elif path.is_file():
      candidates = [path]
    else:
      print(f"JSON validation target not found: {path}", file=sys.stderr)
      return None

    for candidate in candidates:
      resolved = candidate.resolve()
      if resolved not in seen:
        seen.add(resolved)
        files.append(candidate)
  return files


def main():
  if len(sys.argv) < 2:
    print(f"Usage: {Path(sys.argv[0]).name} <json-file-or-directory> [...]", file=sys.stderr)
    return 2

  files = collect_json_files(sys.argv[1:])
  if files is None:
    return 1

  errors = 0
  for path in files:
    try:
      with path.open("r", encoding="utf-8") as source:
        json.load(source)
    except json.JSONDecodeError as error:
      print(
        f"JSON syntax error: {path}: line {error.lineno}, column {error.colno}: {error.msg}",
        file=sys.stderr,
      )
      errors += 1
    except (OSError, UnicodeError) as error:
      print(f"Could not read JSON file: {path}: {error}", file=sys.stderr)
      errors += 1

  if errors:
    print(f"JSON validation failed: {errors} file(s) invalid.", file=sys.stderr)
    return 1

  print(f"JSON validation OK: {len(files)} file(s).")
  return 0


if __name__ == "__main__":
  sys.exit(main())
