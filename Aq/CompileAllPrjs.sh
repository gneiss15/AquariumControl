#!/usr/bin/env bash
# CompileAllPrjs.sh, Version: 1.01

set -euo pipefail

PrjsDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

pushd "$PrjsDir" >/dev/null
./CompileAllEspPrjs.sh
popd >/dev/null
