#!/usr/bin/env python3
# PrepareWebSrc.py, Version: 1.06

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[3] / "tools"))
from SyncGeneratedFiles import sync_tree


HEAD_MARKER = "<!-- EspUpdateCommonHead -->"
BODY_MARKER = "<!-- EspUpdateCommonBody -->"


def main() -> None:
  parser = argparse.ArgumentParser()
  parser.add_argument("source_dir")
  parser.add_argument("target_dir")
  parser.add_argument("--platform", choices=("esp8266", "esp32", "esp32-rescue"), default="esp8266")
  parser.add_argument("--shelly-calibration-warning", action="store_true")
  args = parser.parse_args()

  common_dir = pathlib.Path(__file__).resolve().parent
  source_dir = pathlib.Path(args.source_dir).resolve()
  target_dir = pathlib.Path(args.target_dir).resolve()
  web_dir = common_dir / "WebSrc"

  if not source_dir.is_dir():
    raise RuntimeError(f"Web source directory not found: {source_dir}")

  final_target = target_dir
  final_target.parent.mkdir(parents=True, exist_ok=True)
  target_dir = pathlib.Path(tempfile.mkdtemp(prefix=".web-stage-", dir=final_target.parent))
  shutil.copytree(source_dir, target_dir, dirs_exist_ok=True)

  html_files = list(target_dir.glob("*.html"))
  if len(html_files) != 1:
    raise RuntimeError(f"Expected exactly one HTML file in {target_dir}")

  html_file = html_files[0]
  html = html_file.read_text(encoding="utf-8")
  if html.count(HEAD_MARKER) != 1:
    raise RuntimeError(f"Expected exactly one {HEAD_MARKER} marker in {html_file}")
  if html.count(BODY_MARKER) != 1:
    raise RuntimeError(f"Expected exactly one {BODY_MARKER} marker in {html_file}")

  script_file = "EspUpdateRescue.js" if args.platform == "esp32-rescue" else "EspUpdate.js"
  head = f'<link rel="stylesheet" href="EspUpdate.css">\n<script type="application/javascript" src="{script_file}"></script>'
  if args.platform == "esp32-rescue":
    body_file = "EspUpdateRescue.html"
  elif args.platform == "esp32":
    body_file = "EspUpdateEsp32.html"
  else:
    body_file = "EspUpdate.html"
  body = (web_dir / body_file).read_text(encoding="utf-8")
  html = html.replace(HEAD_MARKER, head)
  html = html.replace(BODY_MARKER, body)
  if args.shelly_calibration_warning:
    needle = '<h2>Kalibrierung</h2>'
    if html.count(needle) != 1:
      raise RuntimeError("Expected one Shelly calibration heading")
    warning = needle + '\n    <p id="ShellyFactoryCalibrationWarning"><strong>Warnung: Eine Kalibrierung überschreibt die Shelly-Werks-Kalibrierung.</strong></p>'
    html = html.replace(needle, warning)
  html_file.write_text(html, encoding="utf-8")

  shutil.copy2(web_dir / "EspUpdate.css", target_dir / "EspUpdate.css")
  shutil.copy2(web_dir / script_file, target_dir / script_file)
  sync_tree(target_dir, final_target)
  shutil.rmtree(target_dir)


if __name__ == "__main__":
  main()
