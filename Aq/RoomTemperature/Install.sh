#!/usr/bin/env bash
# Install.sh, Version: 1.01
set -euo pipefail
ProjectDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
ServiceUser="${ROOM_TEMPERATURE_USER:-$(stat -c %U "$ProjectDir")}"
if [[ $EUID -ne 0 ]]; then
  exec sudo ROOM_TEMPERATURE_USER="$ServiceUser" /bin/bash "$ProjectDir/Install.sh"
fi
id "$ServiceUser" >/dev/null
[[ "$ServiceUser" != root ]] || { echo 'Set ROOM_TEMPERATURE_USER to a non-root account.' >&2; exit 1; }
ServiceTemp="$(mktemp)"
trap 'rm -f "$ServiceTemp"' EXIT
python3 - "$ProjectDir" "$ServiceUser" "$ServiceTemp" <<'PY'
from pathlib import Path
import sys
root,user,target=sys.argv[1:]
def escape(value):
    return value.replace('\\','\\\\').replace('"','\\"').replace('%','%%')
source=(Path(root)/'RoomTemperature.service').read_text()
source=source.replace('WorkingDirectory=@PROJECT_DIR@','WorkingDirectory='+root.replace('%','%%'))
source=source.replace('@PROJECT_DIR@',escape(root)).replace('@SERVICE_USER@',user)
Path(target).write_text(source)
PY
install -m 644 "$ServiceTemp" /etc/systemd/system/RoomTemperature.service
systemctl daemon-reload
systemctl enable RoomTemperature.service
systemctl restart RoomTemperature.service
systemctl --no-pager --full status RoomTemperature.service
