# DownloadLittleFS.py, Version: 1.05
from __future__ import annotations

import ftplib
import json
import pathlib
import shutil
import sys


def read_configuration(path: pathlib.Path, host_override: str = "") -> tuple[str, str]:
  config = json.loads(path.read_text(encoding="utf-8"))
  if not isinstance(config, dict):
    raise ValueError("Config.json must contain a JSON object")

  hostname = host_override.strip() or config.get("Hostname", "")
  password = config.get("FtpPw", "")
  if not isinstance(hostname, str) or not hostname.strip():
    raise ValueError("Hostname is missing in Config.json")
  if not isinstance(password, str) or not password:
    raise ValueError("FtpPw is missing in Config.json")
  if any(character in hostname or character in password for character in "\r\n"):
    raise ValueError("FTP credentials must not contain line breaks")
  return hostname, password


def clear_target(target_dir: pathlib.Path) -> None:
  for entry in target_dir.iterdir():
    if entry.is_dir():
      shutil.rmtree(entry)
    else:
      entry.unlink()


def download(host: str, user: str, password: str,
             target_dir: pathlib.Path, passive: bool) -> list[str]:
  clear_target(target_dir)
  ftp = ftplib.FTP()
  try:
    ftp.connect(host, 21, timeout=15)
    ftp.login(user, password)
    ftp.set_pasv(passive)
    downloaded: list[str] = []

    def download_current_directory(local_dir: pathlib.Path) -> None:
      seen_names: set[str] = set()
      for name, facts in ftp.mlsd():
        if name in (".", "..") or facts.get("type", "").lower() in ("cdir", "pdir"):
          continue
        remote_path = pathlib.PurePosixPath(name.rstrip("/"))
        entry_name = remote_path.name
        if (not entry_name or entry_name in (".", "..") or ".." in remote_path.parts or
            "\\" in name or any(ord(character) < 32 for character in name)):
          raise RuntimeError(f"Invalid FTP entry name: {name!r}")
        if entry_name in seen_names:
          raise RuntimeError(f"Duplicate FTP entry: {entry_name}")
        seen_names.add(entry_name)

        entry_type = facts.get("type", "").lower()
        if local_dir == target_dir and entry_name == "diagnostics" and entry_type == "dir":
          continue
        local_path = local_dir / entry_name
        if entry_type == "dir":
          local_path.mkdir()
          ftp.cwd(entry_name)
          try:
            download_current_directory(local_path)
          finally:
            ftp.cwd("..")
        elif entry_type == "file":
          try:
            with local_path.open("wb") as output:
              ftp.retrbinary(f"RETR {entry_name}", output.write)
          except Exception:
            local_path.unlink(missing_ok=True)
            raise
          downloaded.append(local_path.relative_to(target_dir).as_posix())
        else:
          raise RuntimeError(f"Unknown FTP entry type for {name!r}: {entry_type!r}")

    download_current_directory(target_dir)
    if not downloaded:
      raise RuntimeError("FTP root contains no downloadable files")

    try:
      ftp.quit()
    except ftplib.all_errors:
      ftp.close()
    return downloaded
  except Exception:
    try:
      ftp.close()
    except Exception:
      pass
    raise


def main() -> int:
  if len(sys.argv) not in (3, 4):
    raise SystemExit("Usage: DownloadLittleFS.py CONFIG_FILE TARGET_DIRECTORY [HOSTNAME]")

  config_path = pathlib.Path(sys.argv[1])
  target_dir = pathlib.Path(sys.argv[2])
  host_override = sys.argv[3] if len(sys.argv) == 4 else ""
  target_dir.mkdir(parents=True, exist_ok=True)
  hostname, password = read_configuration(config_path, host_override)
  username = json.loads(config_path.read_text(encoding="utf-8")).get("Hostname", "")
  if not isinstance(username, str) or not username or "\n" in username or "\r" in username:
    raise ValueError("Invalid FTP username in Config.json")
  print(f"Downloading LittleFS root from {hostname}")

  errors: list[str] = []
  for passive in (True, False):
    try:
      files = download(hostname, username, password, target_dir, passive)
      mode = "passive" if passive else "active"
      print(f"FTP transfer mode: {mode}")
      for name in sorted(files, key=str.casefold):
        print(f"  {name}")
      print(f"Downloaded files: {len(files)}")
      return 0
    except Exception as error:
      mode = "passive" if passive else "active"
      errors.append(f"{mode}: {error}")

  raise SystemExit("FTP download failed\n" + "\n".join(errors))


if __name__ == "__main__":
  raise SystemExit(main())
