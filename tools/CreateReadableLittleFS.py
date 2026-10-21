# CreateReadableLittleFS.py, Version: 1.04
from __future__ import annotations

import argparse
import json
import pathlib
import re
import shutil
import subprocess
import tempfile


def remove_path(path: pathlib.Path) -> None:
  if path.is_symlink() or path.exists():
    subprocess.run(["gio", "trash", "--", str(path)], check=True)


def flush_line(lines: list[str], current: list[str], indent: int) -> None:
  text = "".join(current).strip()
  current.clear()
  if text:
    lines.append("  " * indent + text)



def looks_like_regex_start(text: str, index: int) -> bool:
  prefix = text[:index].rstrip()
  if not prefix:
    return True
  if prefix[-1] in "([{:;,=!?&|+-*%^~<>":
    return True
  match = re.search(r"([A-Za-z_$][A-Za-z0-9_$]*)$", prefix)
  return bool(match and match.group(1) in {
    "await", "case", "delete", "do", "else", "in", "instanceof",
    "new", "of", "return", "throw", "typeof", "void", "yield",
  })

def format_braced_text(text: str) -> str:
  lines: list[str] = []
  current: list[str] = []
  indent = 0
  state = "normal"
  escape = False
  index = 0

  while index < len(text):
    character = text[index]
    next_character = text[index + 1] if index + 1 < len(text) else ""

    if state in ("single", "double", "template"):
      current.append(character)
      if escape:
        escape = False
      elif character == "\\":
        escape = True
      elif (
        (state == "single" and character == "'")
        or (state == "double" and character == '"')
        or (state == "template" and character == "`")
      ):
        state = "normal"
      index += 1
      continue

    if state == "line-comment":
      current.append(character)
      if character in "\r\n":
        flush_line(lines, current, indent)
        state = "normal"
      index += 1
      continue

    if state == "block-comment":
      current.append(character)
      if character == "*" and next_character == "/":
        current.append(next_character)
        index += 2
        state = "normal"
        flush_line(lines, current, indent)
      else:
        index += 1
      continue

    if state == "regex":
      current.append(character)
      if escape:
        escape = False
      elif character == "\\":
        escape = True
      elif character == "[":
        state = "regex-class"
      elif character == "/":
        index += 1
        while index < len(text) and text[index].isalpha():
          current.append(text[index])
          index += 1
        state = "normal"
        continue
      index += 1
      continue

    if state == "regex-class":
      current.append(character)
      if escape:
        escape = False
      elif character == "\\":
        escape = True
      elif character == "]":
        state = "regex"
      index += 1
      continue

    if character == "/" and next_character == "/":
      if current and not current[-1].isspace():
        current.append(" ")
      current.extend((character, next_character))
      state = "line-comment"
      index += 2
      continue
    if character == "/" and next_character == "*":
      flush_line(lines, current, indent)
      current.extend((character, next_character))
      state = "block-comment"
      index += 2
      continue
    if character == "/" and looks_like_regex_start(text, index):
      current.append(character)
      state = "regex"
      index += 1
      continue
    if character == "'":
      current.append(character)
      state = "single"
      index += 1
      continue
    if character == '"':
      current.append(character)
      state = "double"
      index += 1
      continue
    if character == "`":
      current.append(character)
      state = "template"
      index += 1
      continue

    if character == "{":
      if current and not current[-1].isspace():
        current.append(" ")
      current.append(character)
      flush_line(lines, current, indent)
      indent += 1
    elif character == "}":
      flush_line(lines, current, indent)
      indent = max(0, indent - 1)
      current.append(character)
      flush_line(lines, current, indent)
    elif character == ";":
      current.append(character)
      flush_line(lines, current, indent)
    elif character in "\r\n\t ":
      if current and not current[-1].isspace():
        current.append(" ")
    else:
      current.append(character)
    index += 1

  flush_line(lines, current, indent)
  return "\n".join(lines) + ("\n" if lines else "")


def fallback_format_html(text: str) -> str:
  text = re.sub(r">\s*<", ">\n<", text.strip())
  return text + ("\n" if text else "")


def format_html(text: str) -> str:
  try:
    from bs4 import BeautifulSoup
  except ImportError:
    return fallback_format_html(text)

  soup = BeautifulSoup(text, features="lxml")
  for style in soup.find_all("style"):
    if style.string is not None:
      style.string.replace_with("\n" + format_braced_text(str(style.string)))
  for script in soup.find_all("script"):
    if not script.has_attr("src") and script.string is not None:
      script.string.replace_with("\n" + format_braced_text(str(script.string)))
  return soup.prettify(formatter=None)


def write_readable_text(source: pathlib.Path, target: pathlib.Path,
                        strict_json: bool) -> bool:
  suffix = source.suffix.casefold()
  try:
    text = source.read_text(encoding="utf-8")
  except UnicodeDecodeError:
    shutil.copy2(source, target)
    return False

  if suffix == ".json":
    try:
      json.loads(text)
    except json.JSONDecodeError as error:
      if strict_json:
        raise ValueError(f"Invalid JSON file {source}: {error}") from error
      shutil.copy2(source, target)
      print(f"Warning: copied invalid JSON unchanged: {source}")
      return False
    # Use the same JSON layout for build and device comparison snapshots.
    readable = json.dumps(json.loads(text), ensure_ascii=False, indent=2) + "\n"
  elif suffix == ".html":
    readable = format_html(text)
  elif suffix in (".css", ".js"):
    readable = format_braced_text(text)
  else:
    shutil.copy2(source, target)
    return False

  target.write_text(readable, encoding="utf-8")
  shutil.copystat(source, target)
  return True


def create_snapshot(source_dir: pathlib.Path, target_dir: pathlib.Path,
                    strict_json: bool) -> tuple[int, int]:
  source_dir = source_dir.resolve()
  target_dir = target_dir.resolve()

  if not source_dir.is_dir():
    raise ValueError(f"Source directory not found: {source_dir}")
  if source_dir == target_dir or source_dir in target_dir.parents:
    raise ValueError("Target directory must not be inside the source directory")
  if target_dir.is_symlink():
    raise ValueError(f"Target directory must not be a symbolic link: {target_dir}")
  if target_dir.exists() and not target_dir.is_dir():
    raise ValueError(f"Target path is not a directory: {target_dir}")

  target_dir.parent.mkdir(parents=True, exist_ok=True)
  temporary_dir = pathlib.Path(
    tempfile.mkdtemp(prefix=f".{target_dir.name}.", dir=target_dir.parent)
  )
  backup_dir = target_dir.with_name(f".{target_dir.name}.previous")
  file_count = 0
  formatted_text_count = 0

  try:
    for source in sorted(source_dir.rglob("*")):
      relative = source.relative_to(source_dir)
      target = temporary_dir / relative

      if source.is_symlink():
        raise ValueError(f"Symbolic links are not supported: {source}")
      if source.is_dir():
        target.mkdir(parents=True, exist_ok=True)
        continue
      if not source.is_file():
        raise ValueError(f"Unsupported filesystem entry: {source}")

      target.parent.mkdir(parents=True, exist_ok=True)
      if write_readable_text(source, target, strict_json):
        formatted_text_count += 1
      file_count += 1

    remove_path(backup_dir)
    if target_dir.exists():
      target_dir.rename(backup_dir)

    try:
      temporary_dir.rename(target_dir)
    except Exception:
      if backup_dir.exists() and not target_dir.exists():
        backup_dir.rename(target_dir)
      raise

    # The replacement succeeded; the previous generated snapshot is obsolete.
    if backup_dir.exists():
      shutil.rmtree(backup_dir)
  except Exception:
    # Only this invocation created the temporary working directory.
    if temporary_dir.exists():
      shutil.rmtree(temporary_dir)
    raise

  return file_count, formatted_text_count


def parse_arguments() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
    description="Create a readable LittleFS directory snapshot."
  )
  parser.add_argument("source_directory", type=pathlib.Path)
  parser.add_argument("target_directory", type=pathlib.Path)
  parser.add_argument(
    "--strict-json",
    action="store_true",
    help="fail instead of copying malformed JSON unchanged",
  )
  return parser.parse_args()


def main() -> int:
  arguments = parse_arguments()
  file_count, text_count = create_snapshot(
    arguments.source_directory,
    arguments.target_directory,
    arguments.strict_json,
  )
  print(
    f"Readable LittleFS snapshot: {arguments.target_directory} "
    f"({file_count} files, {text_count} text files formatted)"
  )
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
