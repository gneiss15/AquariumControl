# AvrFlashCommon.sh, Version: 1.02
# Common implementation sourced by project-specific Flash.sh scripts.
# Uses the board upload recipe selected by the Arduino FQBN (Micronucleus for Digispark).

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "AvrFlashCommon.sh must be sourced by a project Flash.sh." >&2
  exit 2
fi

: "${ProjectDir:?ProjectDir not set}"
: "${PrjsDir:?PrjsDir not set}"
: "${ProjectName:?ProjectName not set}"
: "${FQBN:?FQBN not set}"
: "${BinSubDir:?BinSubDir not set}"

MountDir="$(dirname -- "$(dirname -- "$PrjsDir")")"
CLI="${ARDUINO_CLI_WRAPPER:-${ARDUINO2CLI_START:-$MountDir/Start_Arduino2cli.sh}}"
Firmware="$ProjectDir/generated/bin/$BinSubDir/$ProjectName.ino.hex"

[ "$#" -eq 0 ] || {
  echo "Usage: $(basename -- "$0")" >&2
  exit 2
}
[ -x "$CLI" ] || {
  echo "Error: Arduino CLI wrapper not executable: $CLI" >&2
  exit 1
}
[ -f "$Firmware" ] || {
  echo "Error: firmware not found: $Firmware" >&2
  echo "Run ./AvrCompile.sh first." >&2
  exit 1
}

echo "Flashing: $Firmware"
"$CLI" upload --no-color --fqbn "$FQBN" --input-file "$Firmware"
