# BootSession.py, Version: 1.03
"""Identify a system boot and recover only the latest compatible recording segment."""
import csv
import json
import uuid
from pathlib import Path

def system_boot_id():
  try:
    return Path('/proc/sys/kernel/random/boot_id').read_text().strip()
  except OSError:
    return str(uuid.uuid4())

BOOT_ID = system_boot_id()

def latest_segment(directory, suffix, expected):
  """Return the newest appendable segment matching boot/config metadata.

  Filenames sort chronologically. Only the newest segment is considered: never
  resume an older match across a later configuration or interrupted recording.
  Return None when a new segment must be created.
  """
  paths = sorted(directory.glob('*' + suffix))
  if not paths:
    return None
  path = paths[-1]
  # An interrupted record must never be joined to the next appended row.
  # Preserve the original file and let the caller start a new segment.
  with path.open('rb') as stream:
    stream.seek(0, 2)
    if stream.tell() == 0:
      return None
    stream.seek(-1, 2)
    if stream.read(1) != b'\n':
      return None
  # Never reopen an older matching segment across an intervening configuration.
  try:
    metadata = json.loads(path.with_suffix('.json').read_text())
  except (FileNotFoundError, json.JSONDecodeError, UnicodeError):
    return None
  if not isinstance(metadata, dict):
    return None
  if any(json.dumps(metadata.get(key),sort_keys=True) != json.dumps(value,sort_keys=True) for key, value in expected.items()):
    return None
  return path

def last_row(path, header):
  """Recover totals/sample counters from complete records, never time anchors.

  A record is committed by its terminating newline. Ignore an interrupted tail
  without altering the file; malformed complete records still report an error.
  """
  def complete_lines(stream):
    for line in stream:
      if not line.endswith('\n'):
        return
      yield line

  with path.open(encoding='utf-8', errors='surrogateescape', newline='') as stream:
    reader = csv.DictReader(complete_lines(stream), delimiter=';', strict=True)
    if tuple(reader.fieldnames or ()) != tuple(header):
      raise ValueError(f'Unexpected CSV header: {path}')
    last = None
    for row in reader:
      if None in row or any(value is None for value in row.values()):
        raise ValueError(f'Incomplete CSV row: {path}')
      last = row
    return last
