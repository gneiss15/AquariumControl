#!/usr/bin/env python3
# EspRescueSigning.py, Version: 1.02
# Compatibility wrapper; signing-header generation is shared by all ESP8266 builds.

from __future__ import annotations

import runpy
from pathlib import Path


runpy.run_path(
  str(Path(__file__).resolve().parents[1] / "Libs" / "Common" / "EspUpdate" / "EspUpdateSigning.py"),
  run_name="__main__",
)
