#!/usr/bin/env bash
# ChargeLearning.sh, Version: 1.00
set -euo pipefail
ScriptDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
exec python3 "$ScriptDir/ChargeLearning.py" "$@"
