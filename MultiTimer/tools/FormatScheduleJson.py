# FormatScheduleJson.py, Version: 1.02
from __future__ import annotations

import argparse
import json
import pathlib
import tempfile


def parse_arguments() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
    description="Validate and format a MultiTimer schedule JSON file."
  )
  parser.add_argument("source_file", type=pathlib.Path)
  parser.add_argument("target_file", type=pathlib.Path)
  return parser.parse_args()


def load_schedule(source_file: pathlib.Path) -> dict[str, object]:
  try:
    value = json.loads(source_file.read_text(encoding="utf-8"))
  except (OSError, UnicodeError, json.JSONDecodeError) as error:
    raise ValueError(f"Could not read valid JSON from {source_file}: {error}") from error

  if not isinstance(value, dict):
    raise ValueError("Schedule root must be an object")
  if not isinstance(value.get("SwitchTimes"), list):
    raise ValueError("Schedule entry SwitchTimes must be an array")
  if not isinstance(value.get("Intervals"), list):
    raise ValueError("Schedule entry Intervals must be an array")

  return value


def format_rows(rows: list[object], indent: int) -> str:
  if not rows:
    return "[]"
  if not all(isinstance(row, list) for row in rows):
    raise ValueError("Schedule rows must be arrays")

  column_count = max((len(row) for row in rows), default=0)
  widths = [0] * column_count
  cells: list[list[str]] = []
  for row in rows:
    row_cells = [
      json.dumps(value, ensure_ascii=False, separators=(",", ":"))
      for value in row
    ]
    cells.append(row_cells)
    for column, text in enumerate(row_cells):
      widths[column] = max(widths[column], len(text))

  prefix = " " * (indent + 2)
  lines = ["["]
  for row_index, row_cells in enumerate(cells):
    parts: list[str] = []
    for column, text in enumerate(row_cells):
      if column + 1 < len(row_cells):
        parts.append(text + "," + " " * (widths[column] - len(text) + 1))
      else:
        parts.append(text)
    comma = "," if row_index + 1 < len(cells) else ""
    lines.append(f"{prefix}[ {''.join(parts)} ]{comma}")
  lines.append(" " * indent + "]")
  return "\n".join(lines)


def write_schedule(target_file: pathlib.Path, value: dict[str, object]) -> None:
  target_file.parent.mkdir(parents=True, exist_ok=True)
  switch_times = value["SwitchTimes"]
  intervals = value["Intervals"]
  assert isinstance(switch_times, list)
  assert isinstance(intervals, list)
  text = (
    "{\n"
    f"  \"SwitchTimes\":{format_rows(switch_times, 2)},\n"
    f"  \"Intervals\":{format_rows(intervals, 2)}\n"
    "}\n"
  )

  with tempfile.NamedTemporaryFile(
    mode="w",
    encoding="utf-8",
    dir=target_file.parent,
    prefix=f".{target_file.name}.",
    delete=False,
  ) as temporary_file:
    temporary_file.write(text)
    temporary_path = pathlib.Path(temporary_file.name)

  temporary_path.replace(target_file)


def main() -> int:
  arguments = parse_arguments()
  schedule = load_schedule(arguments.source_file)
  write_schedule(arguments.target_file, schedule)
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
