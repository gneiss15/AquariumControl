#!/usr/bin/env bash
# BuildAllPrjs.sh, Version: 1.01

set -euo pipefail

PrjsDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

pushd "$PrjsDir" >/dev/null
./BuildAllEspPrjs.sh
popd >/dev/null
