#!/usr/bin/env bash
# GenerateJs.sh, Version: 1.00

set -Eeuo pipefail

ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PrjsDir="$(cd -- "$ProjectDir/../.." && pwd -P)"
Target="${1:-$ProjectDir/WebSrc/Messages.js}"
python3 "$PrjsDir/Libs/GnAqPrjsLib/tools/GenerateJs.py" --out "$Target"
