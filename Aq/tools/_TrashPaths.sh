# _TrashPaths.sh, Version: 1.00
# Recoverable removal for sourced build helpers.

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "_TrashPaths.sh must be sourced." >&2
  exit 2
fi

TrashPaths()
{
  local Path
  for Path in "$@"; do
    if [ -e "$Path" ] || [ -L "$Path" ]; then
      command -v gio >/dev/null 2>&1 || { echo "gio is required for recoverable cleanup." >&2; return 1; }
      gio trash -- "$Path" || return $?
    fi
  done
}
