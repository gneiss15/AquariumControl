# DownloadLittleFS.py, Version: 1.03
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
    remote_files: list[tuple[str, str]] = []
    seen_local_names: set[str] = set()

    for name in ftp.nlst():
      normalized = name.rstrip("/")
      local_name = pathlib.PurePosixPath(normalized).name
      if local_name in ("", ".", ".."):
        continue
      if local_name in seen_local_names:
        raise RuntimeError(f"Duplicate FTP basename: {local_name}")
      seen_local_names.add(local_name)
      remote_files.append((name, local_name))

    if not remote_files:
      raise RuntimeError("FTP root contains no downloadable files")

    downloaded: list[str] = []
    for remote_name, local_name in remote_files:
      local_path = target_dir / local_name
      try:
        with local_path.open("wb") as output:
          ftp.retrbinary(f"RETR {remote_name}", output.write)
      except Exception:
        local_path.unlink(missing_ok=True)
        raise
      downloaded.append(local_name)

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
