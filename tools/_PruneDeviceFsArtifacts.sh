# _PruneDeviceFsArtifacts.sh, Version: 1.02
# Remove retired function images only after a successful filesystem build.
if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
  echo "_PruneDeviceFsArtifacts.sh must be sourced." >&2
  exit 2
fi

DeviceFsArtifactToolsDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"

FsArtifactName()
{
  python3 "$DeviceFsArtifactToolsDir/DeviceFsArtifactName.py" "$1"
}

PruneDeviceFsArtifacts()
{
  local ArtifactDir="$1" StateDir="$2" Artifact Name Directory Hostname
  local -A ActiveNames=()
  [ -d "$StateDir" ] || { echo "Missing DevState directory; no artifacts removed." >&2; return 1; }
  [ -d "$ArtifactDir" ] || return 0
  while IFS= read -r -d '' Directory; do
    Hostname="$(FsArtifactName "$Directory/Config.json")" || return $?
    [ -z "${ActiveNames[$Hostname]+present}" ] || { echo "Duplicate filesystem Hostname: $Hostname" >&2; return 1; }
    ActiveNames[$Hostname]=1
  done < <(find "$StateDir" -mindepth 1 -maxdepth 1 -type d -print0)
  [ "${#ActiveNames[@]}" -gt 0 ] || { echo "No active FS hostnames; no artifacts removed." >&2; return 1; }
  while IFS= read -r -d '' Artifact; do
    Name="${Artifact##*/}"
    case "$Name" in
      *.ino.littlefs_signed) Name="${Name%.ino.littlefs_signed}" ;;
      *.ino.littlefs.signed) Name="${Name%.ino.littlefs.signed}" ;;
      *) continue ;;
    esac
    if [ -z "${ActiveNames[$Name]+present}" ]; then
      rm -rf -- "$Artifact" || return $?
      printf 'Retired filesystem artifact removed: %s\n' "$Artifact"
    fi
  done < <(find "$ArtifactDir" -mindepth 1 -maxdepth 1 -type f -print0)
}
