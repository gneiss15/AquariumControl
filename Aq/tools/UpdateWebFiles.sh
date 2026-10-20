#!/usr/bin/env bash
# UpdateWebFiles.sh, Version: 1.18

set -Eeuo pipefail

ToolsDir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=_TrashPaths.sh
source "$ToolsDir/_TrashPaths.sh"
ProjectDir="$(pwd -P)"
WebSrcDir="${PRJS_WEB_SRC_DIR:-$ProjectDir/WebSrc}"
ConfigDir="${PRJS_CONFIG_DIR-$ProjectDir/Config}"
DataDir="${PRJS_DATA_DIR:-$ProjectDir/data}"
MergeHtml="$ToolsDir/MergeHtml.py"
FormatReadableJson="$ToolsDir/FormatReadableJson.py"
MINIFY="${MINIFY:-$ToolsDir/minify}"
TempDir=""

PauseOnError()
{
  if [ -t 0 ]; then
    read -r -p "Press enter to continue (exit)" _ || true
  fi
}

Fail()
{
  local Message="$1"

  trap - ERR
  echo -e "$Message" >&2
  PauseOnError
  exit 1
}

UnexpectedError()
{
  local ExitCode=$?
  local LineNr="$1"

  trap - ERR
  echo "Unexpected error in UpdateWebFiles.sh at line $LineNr" >&2
  PauseOnError
  exit "$ExitCode"
}

Cleanup()
{
  if [ -n "$TempDir" ]; then
    rm -rf -- "$TempDir"
  fi
}

trap 'UnexpectedError "$LINENO"' ERR
trap Cleanup EXIT

[ -d "$WebSrcDir" ] || Fail "Directory not found: $WebSrcDir"
if [ -n "$ConfigDir" ]; then
  [ -d "$ConfigDir" ] || Fail "Directory not found: $ConfigDir"
  [ -f "$ConfigDir/Config.json" ] || Fail "Configuration file not found: $ConfigDir/Config.json"
  [ -f "$ConfigDir/ConfigEntries.json" ] || Fail "Configuration entries file not found: $ConfigDir/ConfigEntries.json"
fi
[ -f "$MergeHtml" ] || Fail "MergeHtml.py not found: $MergeHtml"
[ -f "$FormatReadableJson" ] || Fail "FormatReadableJson.py not found: $FormatReadableJson"
command -v python3 >/dev/null 2>&1 || Fail "python3 not installed or not in PATH"
command -v sha256sum >/dev/null 2>&1 || Fail "sha256sum not installed or not in PATH"
[ -x "$MINIFY" ] || Fail "minify not found or not executable: $MINIFY"

mapfile -d '' HtmlFiles < <(find "$WebSrcDir" -maxdepth 1 -type f -name '*.html' -print0)
if [ "${#HtmlFiles[@]}" -ne 1 ]; then
  Fail "Number of HTML files in $WebSrcDir is not 1"
fi

mkdir -p -- "$DataDir"
TempDir="$(mktemp -d)"
HtmlBase="$(basename -- "${HtmlFiles[0]}")"
MergedHtml="$TempDir/Merged.$HtmlBase"

# WebSrc is the readable source. Build minified CSS and JavaScript copies in a
# temporary directory before MergeHtml.py embeds them into the HTML document.
install -m 0644 "${HtmlFiles[0]}" "$TempDir/$HtmlBase"

shopt -s nullglob
for SourceFile in "$WebSrcDir"/*.css "$WebSrcDir"/*.js; do
  TargetFile="$TempDir/$(basename -- "$SourceFile")"
  "$MINIFY" "$SourceFile" -o "$TargetFile"
  TrashPaths "$DataDir/$(basename -- "$SourceFile")"
done

(
  cd "$TempDir"
  python3 "$MergeHtml" "$HtmlBase" "$MergedHtml"
)
HtmlTarget="$DataDir/$HtmlBase"
"$MINIFY" "$MergedHtml" -o "$HtmlTarget"

Doctype='<!doctype html>'
DoctypeLength="${#Doctype}"
if [ "$(head -c "$DoctypeLength" "$HtmlTarget")" != "$Doctype" ]; then
  Fail "Minified HTML does not start with $Doctype: $HtmlTarget"
fi
HtmlHash="$(sha256sum "$HtmlTarget" | awk '{ print $1 }')"
HashedHtml="$TempDir/Hashed.$HtmlBase"
{
  printf '%s<!-- SHA256: %s -->' "$Doctype" "$HtmlHash"
  tail -c "+$(( DoctypeLength + 1 ))" "$HtmlTarget"
} > "$HashedHtml"
mv -- "$HashedHtml" "$HtmlTarget"

CopyJsonFile()
{
  local SourceFile="$1"
  local TargetFile="$DataDir/$(basename -- "$SourceFile")"

  case "$(basename -- "$SourceFile")" in
    Config.json|Channels.json|SwitchTimes.json|FeedTimes.json|EspSwitchTimes.json)
      # Keep operator-editable configuration readable and consistently aligned on LittleFS.
      python3 "$FormatReadableJson" "$SourceFile" "$TargetFile"
      ;;
    *)
      "$MINIFY" "$SourceFile" -o "$TargetFile"
      ;;
  esac
}

for JsonFile in "$WebSrcDir"/*.json; do
  CopyJsonFile "$JsonFile"
done
if [ -n "$ConfigDir" ]; then
  for JsonFile in "$ConfigDir"/*.json; do
    CopyJsonFile "$JsonFile"
  done
fi

for SourceFile in "$WebSrcDir"/*; do
  [ -f "$SourceFile" ] || continue
  case "$SourceFile" in
    *.html|*.js|*.css|*.json)
      continue
      ;;
  esac
  install -m 0644 "$SourceFile" "$DataDir/$(basename -- "$SourceFile")"
done

DataBytes="$(find "$DataDir" -maxdepth 1 -type f -printf '%s\n' | awk '{ Sum += $1 } END { print Sum + 0 }')"
printf 'Generated web files in: %s (%s bytes total)\n' "$DataDir" "$DataBytes"
