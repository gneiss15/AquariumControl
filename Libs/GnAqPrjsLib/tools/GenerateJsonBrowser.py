# GenerateJsonBrowser.py, Version: 1.05
"""Generate browser JSON codecs from the preprocessed definition table."""
import json
from pathlib import Path

RUNTIME = (Path(__file__).resolve().parent/'Templates/JsonRuntime.js').read_text()

def Generate(Types,AcceptNull=False,Notice=None):
  Notice = "// " + (Notice or "Generated file, do not modify. Definition: MessageDefinition.inc.h.") + "\n"
  return Notice + "const ACCEPT_NULL = " + json.dumps(AcceptNull) + ";\n" + "const TYPES = " + json.dumps(Types, ensure_ascii=False, separators=(",", ":")) + ";\n" + RUNTIME
