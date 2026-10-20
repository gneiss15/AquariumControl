# EspFlashCommon.sh, Version: 1.04
# Common implementation sourced by project-specific Flash.sh scripts.
# Do not execute this file directly.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "EspFlashCommon.sh must be sourced by a project Flash.sh." >&2
  exit 2
fi

: "${ProjectDir:?ProjectDir not set}"
: "${PrjsDir:?PrjsDir not set}"
: "${ProjectName:?ProjectName not set}"
: "${BinSubDir:?BinSubDir not set}"
: "${Chip:?Chip not set}"
: "${FlashOffset:?FlashOffset not set}"
: "${LittleFsOffset:?LittleFsOffset not set}"

MountDir="$(dirname -- "$(dirname -- "$PrjsDir")")"
Esptool="${ARDUINO_ESPTOOL_WRAPPER:-${ARDUINO2ESPTOOL_START:-$MountDir/Start_Arduino2esptool.sh}}"
Port="${ESP_PORT:-/dev/ttyUSB0}"
Baud="${ESP_FLASH_BAUD:-115200}"
BinDir="$ProjectDir/generated/bin/$BinSubDir"
Firmware="$BinDir/$ProjectName.ino.bin"
FlashLittleFs=true
PortSet=false
DeviceName=""

ShowUsage()
{
  echo "Usage: $(basename -- "$0") [--NoFS] [--Device NAME] [port]" >&2
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --NoFS)
      FlashLittleFs=false
      shift
      ;;
    --Device)
      [ "$#" -ge 2 ] || { ShowUsage; exit 2; }
      DeviceName="$2"
      shift 2
      ;;
    --*)
      ShowUsage
      exit 2
      ;;
    *)
      if "$PortSet"; then
        ShowUsage
        exit 2
      fi
      Port="$1"
      PortSet=true
      shift
      ;;
  esac
done

if "$FlashLittleFs" && [ -z "$DeviceName" ]; then
  if [ -d "$ProjectDir/DevState" ]; then
    mapfile -t Devices < <(find "$ProjectDir/DevState" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' | sort)
  else
    Devices=()
  fi
  if [ "${#Devices[@]}" -eq 1 ]; then
    DeviceName="${Devices[0]}"
  else
    echo "Error: specify --Device NAME for LittleFS flash (${#Devices[@]} device states found)." >&2
    exit 2
  fi
fi
LittleFs="$BinDir/$DeviceName.ino.littlefs"

[ -x "$Esptool" ] || { echo "Error: Arduino esptool wrapper not executable: $Esptool" >&2; exit 1; }
[ -f "$Firmware" ] || { echo "Error: firmware not found: $Firmware" >&2; echo "Run ./EspCompile.sh first." >&2; exit 1; }
if "$FlashLittleFs" && [ ! -f "$LittleFs" ]; then
  echo "Error: LittleFS image not found: $LittleFs" >&2
  echo "Run ./EspCompile.sh first or use --NoFS for firmware only." >&2
  exit 1
fi

declare -p EsptoolExtraArgs >/dev/null 2>&1 || EsptoolExtraArgs=()
declare -p WriteFlashArgs >/dev/null 2>&1 || WriteFlashArgs=()

FlashImages=( "$FlashOffset" "$Firmware" )
if "$FlashLittleFs"; then
  FlashImages+=( "$LittleFsOffset" "$LittleFs" )
fi

echo "Flashing firmware: $Firmware"
if "$FlashLittleFs"; then
  echo "Flashing LittleFS for $DeviceName: $LittleFs"
else
  echo "LittleFS: skipped (--NoFS)"
fi

"$Esptool" --chip "$Chip" --port "$Port" --baud "$Baud" \
  "${EsptoolExtraArgs[@]}" write_flash "${WriteFlashArgs[@]}" "${FlashImages[@]}"
