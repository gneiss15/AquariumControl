#!/usr/bin/env bash
# CompileAllEspPrjs.sh, Version: 2.04

PrjsDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
pushd "$PrjsDir" >/dev/null
for i in TmrSwA8T TmrSwShelly Feeder FeederSimple; do $i/EspRescueCompile.sh; $i/EspCompile.sh; done
popd
