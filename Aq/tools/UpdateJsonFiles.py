# UpdateJsonFiles.py, Version: 1.06
from __future__ import annotations

import json
import pathlib
import sys

import sys as _generated_sys
from pathlib import Path as _GeneratedPath
_generated_sys.path.insert(0, str(next(parent / "tools" for parent in _GeneratedPath(__file__).resolve().parents if (parent / "tools/SyncGeneratedFiles.py").is_file())))
from SyncGeneratedFiles import write_text_if_changed, write_bytes_if_changed


def load_json(path: pathlib.Path) -> object:
  return json.loads(path.read_text(encoding="utf-8"))


def collect_overrides(secrets: object, project_name: str) -> dict[str, object]:
  if not isinstance(secrets, dict):
    raise ValueError("Secrets root must be a JSON object")

  overrides: dict[str, object] = {}
  for key, value in secrets.items():
    if not isinstance(value, dict):
      overrides[key] = value

  common = secrets.get("Common", {})
  if isinstance(common, dict):
    overrides.update(common)

  project = secrets.get(project_name, {})
  if isinstance(project, dict):
    overrides.update(project)

  projects = secrets.get("Projects", {})
  if isinstance(projects, dict):
    project_values = projects.get(project_name, {})
    if isinstance(project_values, dict):
      overrides.update(project_values)

  return overrides


def dumps_aligned_object(value: dict[str, object]) -> str:
  if not value:
    return "{}"

  key_texts = [json.dumps(key, ensure_ascii=False) for key in value]
  key_width = max(len(text) for text in key_texts) + 1  # include ':'
  lines = ["{"]
  items = list(value.items())
  for index, ((_, item_value), key_text) in enumerate(zip(items, key_texts)):
    value_text = json.dumps(item_value, ensure_ascii=False, separators=(",", ":"))
    comma = "," if index + 1 < len(items) else ""
    lines.append(f"  {(key_text + ':'):<{key_width}} {value_text}{comma}")
  lines.append("}")
  return "\n".join(lines)


def main() -> int:
  if len(sys.argv) not in {8, 9}:
    raise SystemExit(
      "Usage: UpdateJsonFiles.py ENTRIES_SOURCE ENTRIES_TARGET "
      "CONFIG_SOURCE CONFIG_TARGET SECRETS_FILE PROJECT_NAME STYLE "
      "[DEVICE_CONFIG]"
    )

  entries_source = pathlib.Path(sys.argv[1])
  entries_target = pathlib.Path(sys.argv[2])
  config_source = pathlib.Path(sys.argv[3])
  config_target = pathlib.Path(sys.argv[4])
  secrets_path = pathlib.Path(sys.argv[5])
  project_name = sys.argv[6]
  style = sys.argv[7]
  device_config_path = pathlib.Path(sys.argv[8]) if len(sys.argv) == 9 and sys.argv[8] else None
  if style not in {"compact", "pretty", "aligned"}:
    raise ValueError(f"Unknown JSON style: {style}")

  entries = load_json(entries_source)
  config = load_json(config_source)
  if not isinstance(entries, dict):
    raise ValueError("Generated ConfigEntries JSON must be an object")
  if not isinstance(config, dict):
    raise ValueError("Generated Config JSON must be an object")

  if secrets_path.is_file():
    overrides = collect_overrides(load_json(secrets_path), project_name)
    for key, value in overrides.items():
      if key in config:
        config[key] = value

  # Device state is the final authority. In particular, device-specific
  # credentials must not be overwritten by project-wide secret defaults.
  if device_config_path is not None:
    device_config = load_json(device_config_path)
    if not isinstance(device_config, dict):
      raise ValueError("Device Config JSON must be an object")
    for key, value in device_config.items():
      if key not in config:
        raise ValueError(f"Unknown device Config key: {key}")
      config[key] = value

  if style == "pretty":
    entries_text = json.dumps(entries, ensure_ascii=False, indent=2)
    config_text = json.dumps(config, ensure_ascii=False, indent=2)
  elif style == "aligned":
    entries_text = dumps_aligned_object(entries)
    config_text = dumps_aligned_object(config)
  else:
    entries_text = json.dumps(entries, ensure_ascii=False, separators=(",", ":"))
    config_text = json.dumps(config, ensure_ascii=False, separators=(",", ":"))

  write_text_if_changed(entries_target, entries_text + "\n", encoding="utf-8")
  write_text_if_changed(config_target, config_text + "\n", encoding="utf-8")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
