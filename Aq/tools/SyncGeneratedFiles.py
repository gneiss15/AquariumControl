# SyncGeneratedFiles.py, Version: 1.01
"""Publish generated files without touching unchanged contents or timestamps."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess


def remove_generated_path(path: Path) -> None:
  if path.exists() or path.is_symlink():
    if path.is_dir() and not path.is_symlink():
      shutil.rmtree(path)
    else:
      path.unlink()


def write_bytes_if_changed(path: Path, content: bytes) -> bool:
  path = Path(path)
  if path.is_symlink():
    raise ValueError(f"Generated output must not be a symlink: {path}")
  if path.is_file() and path.read_bytes() == content:
    return False
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_bytes(content)
  return True


def write_text_if_changed(path: Path, content: str, encoding: str = "utf-8", **kwargs) -> bool:
  newline = kwargs.pop("newline", None)
  if kwargs:
    raise TypeError(f"Unsupported text options: {list(kwargs)}")
  if newline not in (None, "", "\n"):
    content = content.replace("\n", newline)
  return write_bytes_if_changed(Path(path), content.encode(encoding))


def copy_file_if_changed(source: Path, target: Path) -> bool:
  source, target = Path(source), Path(target)
  if source.is_symlink():
    link = os.readlink(source)
    if target.is_symlink() and os.readlink(target) == link:
      return False
    remove_generated_path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.symlink_to(link, target_is_directory=source.is_dir())
    return True
  if not source.is_file():
    raise ValueError(f"Source file not found: {source}")
  if target.is_symlink() or target.is_dir():
    remove_generated_path(target)
  changed = write_bytes_if_changed(target, source.read_bytes())
  if changed:
    shutil.copymode(source, target)
  return changed


def generate_output_if_changed(command, padding=0):
  """Stage a generator's final output before publishing its finished bytes."""
  import tempfile
  import subprocess
  output = Path(command[-1])
  output.parent.mkdir(parents=True, exist_ok=True)
  stage = Path(tempfile.mkdtemp(prefix=".generator-", dir=output.parent))
  temporary = stage / output.name
  try:
    subprocess.run([*command[:-1], str(temporary)], check=True)
    data = temporary.read_bytes()
    if padding:
      data = data.ljust(padding, b"\xff")
    return write_bytes_if_changed(output, data)
  finally:
    shutil.rmtree(stage)


def sync_tree(source: Path, target: Path, keep: tuple[str, ...] = ()) -> int:
  source, target = Path(source).resolve(), Path(target).absolute()
  if not source.is_dir():
    raise ValueError(f"Source directory not found: {source}")
  if target.is_symlink():
    raise ValueError(f"Generated directory must not be a symlink: {target}")
  resolved_target = target.resolve()
  if source == resolved_target or source in resolved_target.parents or resolved_target in source.parents:
    raise ValueError("Source and target directories must not overlap")
  target.mkdir(parents=True, exist_ok=True)
  expected = set()
  changed = 0
  for entry in sorted(source.rglob("*")):
    relative = entry.relative_to(source)
    expected.add(relative)
    destination = target / relative
    if entry.is_symlink() or entry.is_file():
      changed += copy_file_if_changed(entry, destination)
    elif entry.is_dir():
      if destination.is_symlink() or destination.is_file():
        remove_generated_path(destination)
      destination.mkdir(parents=True, exist_ok=True)
  for entry in sorted(target.rglob("*"), key=lambda p: len(p.parts), reverse=True):
    relative = entry.relative_to(target)
    if relative in expected or relative.as_posix() in keep:
      continue
    if entry.is_dir() and not entry.is_symlink() and any(entry.iterdir()):
      continue
    remove_generated_path(entry)
    changed += 1
  return changed


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("mode", choices=("file", "tree"))
  parser.add_argument("source", type=Path)
  parser.add_argument("target", type=Path)
  parser.add_argument("--keep", action="append", default=[])
  args = parser.parse_args()
  if args.mode == "file":
    copy_file_if_changed(args.source, args.target)
  else:
    sync_tree(args.source, args.target, tuple(args.keep))


if __name__ == "__main__":
  main()
