# FormatReadableJson.py, Version: 1.01
from __future__ import annotations

import json
import pathlib
import sys

import sys as _generated_sys
from pathlib import Path as _GeneratedPath
_generated_sys.path.insert(0, str(next(parent / "tools" for parent in _GeneratedPath(__file__).resolve().parents if (parent / "tools/SyncGeneratedFiles.py").is_file())))
from SyncGeneratedFiles import write_text_if_changed, write_bytes_if_changed


def json_text(value: object) -> str:
  return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def format_aligned_object(value: dict[str, object]) -> str:
  if not value:
    return "{}"
  keys = list(value.keys())
  key_texts = [json.dumps(key, ensure_ascii=False) + ":" for key in keys]
  width = max(len(text) for text in key_texts)
  lines = ["{"]
  for index, key in enumerate(keys):
    comma = "," if index + 1 < len(keys) else ""
    lines.append(f"  {key_texts[index]:<{width}} {json_text(value[key])}{comma}")
  lines.append("}")
  return "\n".join(lines)


def format_rows(rows: list[object], indent: int) -> str:
  if not rows:
    return "[]"
  if not all(isinstance(row, list) for row in rows):
    return json.dumps(rows, ensure_ascii=False, indent=2)

  typed_rows = [row for row in rows if isinstance(row, list)]
  column_count = max((len(row) for row in typed_rows), default=0)
  widths = [0] * column_count
  cells: list[list[str]] = []
  for row in typed_rows:
    row_cells = [json_text(cell) for cell in row]
    cells.append(row_cells)
    for column, text in enumerate(row_cells):
      widths[column] = max(widths[column], len(text))

  lines = ["["]
  prefix = " " * (indent + 2)
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


def format_record_array(rows: list[object], indent: int) -> str:
  if not rows:
    return "[]"
  if not all(isinstance(row, dict) for row in rows):
    return json.dumps(rows, ensure_ascii=False, indent=2)

  typed_rows = [row for row in rows if isinstance(row, dict)]
  keys: list[str] = []
  for row in typed_rows:
    for key in row:
      if key not in keys:
        keys.append(key)

  cells: list[dict[str, str]] = []
  widths = {key: 0 for key in keys}
  for row in typed_rows:
    row_cells: dict[str, str] = {}
    for key in keys:
      if key not in row:
        continue
      text = f'{json.dumps(key, ensure_ascii=False)}: {json_text(row[key])}'
      row_cells[key] = text
      widths[key] = max(widths[key], len(text))
    cells.append(row_cells)

  lines = ["["]
  prefix = " " * (indent + 2)
  for row_index, row_cells in enumerate(cells):
    parts: list[str] = []
    for key_index, key in enumerate(keys):
      later = any(next_key in row_cells for next_key in keys[key_index + 1 :])
      if key in row_cells:
        text = row_cells[key]
        if later:
          parts.append(text + "," + " " * (widths[key] - len(text) + 1))
        else:
          parts.append(text)
      elif later:
        parts.append(" " * (widths[key] + 2))
    comma = "," if row_index + 1 < len(cells) else ""
    lines.append(f"{prefix}{{ {''.join(parts).rstrip()} }}{comma}")
  lines.append(" " * indent + "]")
  return "\n".join(lines)


def format_schedule(value: dict[str, object]) -> str:
  switch_times = value.get("SwitchTimes")
  intervals = value.get("Intervals")
  if not isinstance(switch_times, list) or not isinstance(intervals, list):
    return format_aligned_object(value)
  return (
    "{\n"
    f'  "SwitchTimes":{format_rows(switch_times, 2)},\n'
    f'  "Intervals":{format_rows(intervals, 2)}\n'
    "}"
  )


def format_channels(value: dict[str, object]) -> str:
  profiles = value.get("Profiles", [])
  channels = value.get("Channels")
  if not isinstance(profiles, list) or not isinstance(channels, list):
    return json.dumps(value, ensure_ascii=False, indent=2)
  return (
    "{\n"
    f'  "Profiles": {format_record_array(profiles, 2)},\n'
    f'  "Channels": {format_record_array(channels, 2)}\n'
    "}"
  )


def format_value(file_name: str, value: object) -> str:
  if file_name == "Config.json" and isinstance(value, dict):
    return format_aligned_object(value)
  if file_name == "Channels.json" and isinstance(value, dict):
    return format_channels(value)
  if file_name in {"SwitchTimes.json", "EspSwitchTimes.json"}:
    if isinstance(value, dict):
      return format_schedule(value)
    if isinstance(value, list):
      return format_rows(value, 0)
  if file_name == "FeedTimes.json" and isinstance(value, list):
    return format_rows(value, 0)
  return json.dumps(value, ensure_ascii=False, indent=2)


def main() -> int:
  if len(sys.argv) != 3:
    raise SystemExit("Usage: FormatReadableJson.py SOURCE TARGET")
  source = pathlib.Path(sys.argv[1])
  target = pathlib.Path(sys.argv[2])
  value = json.loads(source.read_text(encoding="utf-8"))
  target.parent.mkdir(parents=True, exist_ok=True)
  write_text_if_changed(target, format_value(source.name, value) + "\n", encoding="utf-8")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
