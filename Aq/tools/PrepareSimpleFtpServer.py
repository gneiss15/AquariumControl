# PrepareSimpleFtpServer.py, Version: 1.02
from __future__ import annotations

import argparse
import pathlib
import re
import sys

FTP_AUTH_TIME_OUT = 60
ESP32_STORAGE = "STORAGE_LITTLEFS"


def patch_timeout(config_path: pathlib.Path) -> bool:
  text = config_path.read_text(encoding="utf-8")
  pattern = re.compile(
    r'(?m)^(?P<indent>[ \t]*)#ifndef[ \t]+FTP_AUTH_TIME_OUT[ \t]*\r?\n'
    r'(?P<define_indent>[ \t]*)#define[ \t]+FTP_AUTH_TIME_OUT[ \t]+(?P<value>[0-9]+)[ \t]*\r?\n'
    r'(?P<endif_indent>[ \t]*)#endif(?P<tail>[ \t]*(?://[^\r\n]*)?)$'
  )
  matches = list(pattern.finditer(text))
  if len(matches) != 1:
    raise ValueError(
      "expected exactly one #ifndef FTP_AUTH_TIME_OUT / #define / #endif block "
      f"in {config_path}, found {len(matches)}"
    )

  match = matches[0]
  current = int(match.group("value"))
  if current == FTP_AUTH_TIME_OUT:
    return False

  newline = "\r\n" if "\r\n" in match.group(0) else "\n"
  replacement = (
    f"{match.group('indent')}#ifndef FTP_AUTH_TIME_OUT{newline}"
    f"{match.group('define_indent')}#define FTP_AUTH_TIME_OUT {FTP_AUTH_TIME_OUT}{newline}"
    f"{match.group('endif_indent')}#endif{match.group('tail')}"
  )
  patched = text[:match.start()] + replacement + text[match.end():]

  if len(re.findall(r'(?m)^\s*#define\s+FTP_AUTH_TIME_OUT\s+60\s*$', patched)) != 1:
    raise ValueError("patched FTP_AUTH_TIME_OUT definition is not unique")

  config_path.write_text(patched, encoding="utf-8", newline="")
  return True


def patch_esp32_storage(config_path: pathlib.Path) -> bool:
  text = config_path.read_text(encoding="utf-8")
  pattern = re.compile(
    r'(?m)^(?P<indent>[ \t]*)#define[ \t]+DEFAULT_STORAGE_TYPE_ESP32'
    r'[ \t]+(?P<value>STORAGE_[A-Za-z0-9_]+)(?P<tail>[ \t]*(?://[^\r\n]*)?)$'
  )
  matches = list(pattern.finditer(text))
  if len(matches) != 1:
    raise ValueError(
      "expected exactly one DEFAULT_STORAGE_TYPE_ESP32 definition "
      f"in {config_path}, found {len(matches)}"
    )

  match = matches[0]
  if match.group("value") == ESP32_STORAGE:
    return False

  replacement = (
    f"{match.group('indent')}#define DEFAULT_STORAGE_TYPE_ESP32 "
    f"{ESP32_STORAGE}{match.group('tail')}"
  )
  patched = text[:match.start()] + replacement + text[match.end():]

  if len(re.findall(
      rf'(?m)^\s*#define\s+DEFAULT_STORAGE_TYPE_ESP32\s+{ESP32_STORAGE}\b',
      patched)) != 1:
    raise ValueError("patched DEFAULT_STORAGE_TYPE_ESP32 definition is not unique")

  config_path.write_text(patched, encoding="utf-8", newline="")
  return True


def prepare(source_dir: pathlib.Path) -> None:
  if not source_dir.is_dir():
    raise ValueError(f"SimpleFTPServer library directory not found: {source_dir}")

  config_path = source_dir / "FtpServerKey.h"
  if not config_path.is_file():
    raise ValueError(f"SimpleFTPServer configuration file not found: {config_path}")

  timeout_changed = patch_timeout(config_path)
  storage_changed = patch_esp32_storage(config_path)

  if timeout_changed:
    print(f"SimpleFTPServer: FTP_AUTH_TIME_OUT patched to {FTP_AUTH_TIME_OUT} s")
  else:
    print(f"SimpleFTPServer: FTP_AUTH_TIME_OUT already {FTP_AUTH_TIME_OUT} s")

  if storage_changed:
    print(f"SimpleFTPServer: ESP32 storage patched to {ESP32_STORAGE}")
  else:
    print(f"SimpleFTPServer: ESP32 storage already {ESP32_STORAGE}")


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("source")
  arguments = parser.parse_args()

  try:
    prepare(pathlib.Path(arguments.source))
  except (OSError, ValueError) as error:
    print(f"Error: {error}", file=sys.stderr)
    return 1
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
