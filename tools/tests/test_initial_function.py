# test_initial_function.py, Version: 1.00
"""Placeholder device keys get AP-safe names; actual functions stay unchanged."""
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("update_json_files", ROOT / "tools/UpdateJsonFiles.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class InitialFunctionTests(unittest.TestCase):
  def test_a8t_placeholder_is_expanded(self):
    self.assertEqual(MODULE.resolve_initial_function("Nous1", "TmrSwA8T"), "NousA8TSw1")

  def test_shelly_placeholder_is_expanded(self):
    self.assertEqual(MODULE.resolve_initial_function("Shelly2", "TmrSwShelly"), "ShellySw2")

  def test_real_functions_are_preserved(self):
    for project, function in (
      ("TmrSwA8T", "HeaterMainAq2"),
      ("TmrSwA8T", "LaptopSw"),
      ("TmrSwShelly", "LightTimerAq3"),
    ):
      with self.subTest(project=project, function=function):
        self.assertEqual(MODULE.resolve_initial_function(function, project), function)

  def test_other_project_names_are_preserved(self):
    self.assertEqual(MODULE.resolve_initial_function("Nous1", "OtherProject"), "Nous1")

  def test_generated_config_uses_resolved_function_for_all_four_fields(self):
    with tempfile.TemporaryDirectory(prefix="aq-initial-function-") as temporary:
      root = pathlib.Path(temporary)
      entries_source = root / "entries.json"
      entries_target = root / "ConfigEntries.json"
      config_source = root / "config-source.json"
      config_target = root / "Config.json"
      secrets = root / "Secrets.local.json"
      entries_source.write_text("{}\n", encoding="utf-8")
      config_source.write_text(json.dumps({
        "Hostname": "old",
        "ApSsid": "old",
        "ApPw": "old",
        "FtpPw": "old",
      }), encoding="utf-8")

      for project, function, expected in (
        ("TmrSwA8T", "Nous1", "NousA8TSw1"),
        ("TmrSwShelly", "Shelly2", "ShellySw2"),
        ("TmrSwA8T", "HeaterMainAq2", "HeaterMainAq2"),
      ):
        with self.subTest(project=project, function=function):
          args = [str(entries_source), str(entries_target), str(config_source),
                  str(config_target), str(secrets), project, "compact"]
          with patch.dict(MODULE.os.environ, {"PRJS_INITIAL_FUNCTION": function}), \
               patch.object(sys, "argv", ["UpdateJsonFiles.py", *args]):
            self.assertEqual(MODULE.main(), 0)
          generated = json.loads(config_target.read_text(encoding="utf-8"))
          for key in ("Hostname", "ApSsid", "ApPw", "FtpPw"):
            self.assertEqual(generated[key], expected)


if __name__ == "__main__":
  unittest.main()
