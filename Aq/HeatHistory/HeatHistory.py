#!/usr/bin/env python3
# HeatHistory.py, Version: 1.15

from __future__ import annotations

import argparse
import csv
import datetime as dt
import hashlib
import json
import math
import os
import re
import select
import signal
import socket
import sys
import tempfile
import time
import threading
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Callable

from CreateCharts import chart_server


CSV_HEADER = ("Time", "Temp", "Floor", "Main")
PROFILE_FIELDS = ("Id", "TempId", "FloorOn", "FloorOff", "MainOn", "MainOff", "NoTempOn")
PROFILE_ID_PATTERN = re.compile(r"^HeatProfile(Aq[0-9]+)$")
UDP_STATUS_PORT = 44444
UDP_STATUS_VERSION = 2


class HeatHistoryError(Exception):
  pass


def utc_now() -> dt.datetime:
  return dt.datetime.now(dt.timezone.utc)


def timestamp_text(value: dt.datetime | None = None) -> str:
  current = (value or utc_now()).astimezone(dt.timezone.utc)
  return current.isoformat(timespec="milliseconds").replace("+00:00", "Z")


def timestamp_prefix(value: dt.datetime | None = None) -> str:
  current = (value or utc_now()).astimezone(dt.timezone.utc)
  return current.strftime("%Y%m%dT%H%M%S%fZ")


def canonical_profile(profile: Any) -> bytes:
  if not isinstance(profile, dict):
    raise HeatHistoryError("profile entry is not a JSON object")
  missing = [field for field in PROFILE_FIELDS if field not in profile]
  if missing:
    raise HeatHistoryError(f"profile is missing fields: {', '.join(missing)}")
  if not isinstance(profile["Id"], str) or not profile["Id"]:
    raise HeatHistoryError("profile Id must be a non-empty string")
  if not isinstance(profile["TempId"], str) or not profile["TempId"]:
    raise HeatHistoryError(f"{profile['Id']}: TempId must be a non-empty string")
  if not isinstance(profile["NoTempOn"], bool):
    raise HeatHistoryError(f"{profile['Id']}: NoTempOn must be boolean")
  measurement = profile.get("Measurement", "Temperature")
  if not isinstance(measurement, str) or not re.fullmatch(r"[A-Za-z0-9_]{1,64}", measurement) or measurement == "Kind":
    raise HeatHistoryError(f"{profile['Id']}: invalid Measurement")
  selected: dict[str, Any] = {}
  for field in PROFILE_FIELDS:
    value = profile[field]
    if field in ("FloorOn", "FloorOff", "MainOn", "MainOff"):
      if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise HeatHistoryError(f"{profile['Id']}: {field} must be a finite number")
      selected[field] = float(value)
    else:
      selected[field] = value
  if selected["FloorOn"] > selected["FloorOff"]:
    raise HeatHistoryError(f"{profile['Id']}: FloorOn must be <= FloorOff")
  if selected["MainOn"] > selected["MainOff"]:
    raise HeatHistoryError(f"{profile['Id']}: MainOn must be <= MainOff")
  if measurement != "Temperature":
    selected["Measurement"] = measurement
  return json.dumps(selected, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def profile_digest(profile: dict[str, Any]) -> str:
  return hashlib.sha256(canonical_profile(profile)).hexdigest()


def profile_list(document: Any, source: str) -> list[dict[str, Any]]:
  if not isinstance(document, dict) or not isinstance(document.get("Profiles"), list):
    raise HeatHistoryError(f"{source}: expected a Profiles array")
  result: list[dict[str, Any]] = []
  seen: set[str] = set()
  for profile in document["Profiles"]:
    canonical_profile(profile)
    profile_id = profile["Id"]
    if profile_id in seen:
      raise HeatHistoryError(f"{source}: duplicate profile Id {profile_id}")
    seen.add(profile_id)
    result.append(profile)
  return result


def find_channel(document: Any, device_name: str, profile_id: str, role: str) -> dict[str, Any]:
  channels = document.get("Channels") if isinstance(document, dict) else None
  device_channels = channels.get(device_name) if isinstance(channels, dict) else None
  if isinstance(device_channels, dict):
    device_channels = device_channels.get("Rows")
  if not isinstance(device_channels, list):
    raise HeatHistoryError(f"Dashboard Times.json: no channel rows for {device_name}")
  matches = [channel for channel in device_channels
             if isinstance(channel, dict) and channel.get("Role") == role
             and channel.get("Profile") == profile_id]
  if len(matches) != 1:
    raise HeatHistoryError(
      f"Dashboard Times.json: expected one {role} channel for {profile_id} on {device_name}, found {len(matches)}"
    )
  return matches[0]


def channel_state(status: Any, channel_name: str, source: str) -> bool:
  if not isinstance(status, dict) or not isinstance(status.get("Channels"), list):
    raise HeatHistoryError(f"{source}: expected a Channels status array")
  matches = [channel for channel in status["Channels"]
             if isinstance(channel, dict) and channel.get("Name") == channel_name]
  if len(matches) != 1:
    raise HeatHistoryError(f"{source}: expected one status channel {channel_name}, found {len(matches)}")
  value = matches[0].get("State")
  if not isinstance(value, bool):
    raise HeatHistoryError(f"{source}: channel {channel_name} has no valid boolean State")
  return value


def temperature_value(packet: Any, source: str, measurement: str = "Temperature") -> float:
  if not isinstance(packet, dict):
    raise HeatHistoryError(f"{source}: invalid UDP packet")
  version = packet.get("Version")
  if isinstance(version, bool) or not isinstance(version, int) or version != UDP_STATUS_VERSION:
    raise HeatHistoryError(f"{source}: unsupported UDP version")
  values = packet.get("Status")
  field = measurement
  if not isinstance(values, dict) or field not in values:
    raise HeatHistoryError(f"{source}: UDP packet has no {field} field")
  value = values[field]
  if value is None or isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
    raise HeatHistoryError(f"{source}: UDP measurement is invalid")
  return round(float(value), 2)


def fetch_json(url: str, timeout: float) -> Any:
  request = urllib.request.Request(url, headers={"Accept": "application/json", "Cache-Control": "no-cache"})
  started = time.monotonic()
  try:
    with urllib.request.urlopen(request, timeout=timeout) as response:
      if response.status != 200:
        raise HeatHistoryError(f"{url}: HTTP {response.status}")
      return json.loads(response.read().decode("utf-8"))
  except (urllib.error.URLError, TimeoutError, UnicodeDecodeError, json.JSONDecodeError) as error:
    elapsed = time.monotonic() - started
    raise HeatHistoryError(f"{url}: {error} after {elapsed:.2f}s") from error


def atomic_json_write(path: Path, value: Any) -> None:
  path.parent.mkdir(parents=True, exist_ok=True)
  descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
  try:
    with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
      json.dump(value, output, ensure_ascii=False, indent=2, sort_keys=True)
      output.write("\n")
      output.flush()
      os.fsync(output.fileno())
    os.replace(temporary_name, path)
  finally:
    try:
      os.unlink(temporary_name)
    except FileNotFoundError:
      pass


def unique_log_prefix(directory: Path, base: str) -> str:
  candidate = base
  suffix = 1
  while any(directory.glob(f"{candidate}_*")):
    suffix += 1
    candidate = f"{base}_{suffix}"
  return candidate


class HeatHistoryMonitor:
  def __init__(self, config_path: Path, fetcher: Callable[[str, float], Any] = fetch_json,
               udp_status_provider: Callable[[set[str], float], dict[str, Any]] | None = None):
    self.config_path = config_path.resolve()
    self.project_dir = self.config_path.parent
    self.fetcher = fetcher
    try:
      self.config = json.loads(self.config_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
      raise HeatHistoryError(f"cannot read config {self.config_path}: {error}") from error
    if not isinstance(self.config, dict):
      raise HeatHistoryError("config must be a JSON object")
    self.data_dir = self.resolve_path(self.config.get("DataDir", "data"))
    self.dashboard_url = self.config.get("DashboardUrl", "http://192.168.99.51:8090").rstrip("/")
    if not isinstance(self.dashboard_url, str) or not self.dashboard_url.startswith(("http://", "https://")):
      raise HeatHistoryError("DashboardUrl must use http:// or https://")
    self.poll_interval = float(self.config.get("PollIntervalSeconds", 10))
    self.timeout = float(self.config.get("RequestTimeoutSeconds", 3))
    self.status_max_age = float(self.config.get("StatusMaxAgeSeconds", 15))
    self.status_wait = float(self.config.get("StatusWaitSeconds", 8))
    if not 1 <= self.poll_interval <= 3600:
      raise HeatHistoryError("PollIntervalSeconds must be between 1 and 3600")
    if not 0.2 <= self.timeout <= 60:
      raise HeatHistoryError("RequestTimeoutSeconds must be between 0.2 and 60")
    if not 5 <= self.status_max_age <= 300:
      raise HeatHistoryError("StatusMaxAgeSeconds must be between 5 and 300")
    if not 1 <= self.status_wait <= 60:
      raise HeatHistoryError("StatusWaitSeconds must be between 1 and 60")
    self.udp_status_provider = udp_status_provider
    self.udp_socket: socket.socket | None = None
    self.udp_status_cache: dict[str, tuple[float, dict[str, Any]]] = {}
    self.reported_errors: dict[str, str] = {}
    self.device_ids: dict[str, str] = {}

  def resolve_path(self, value: Any) -> Path:
    if not isinstance(value, str) or not value:
      raise HeatHistoryError("configured paths must be non-empty strings")
    path = Path(value).expanduser()
    return path if path.is_absolute() else (self.project_dir / path).resolve()

  def read_dashboard_data(self) -> tuple[list[dict[str, Any]], Any]:
    profile_document = self.fetcher(f"{self.dashboard_url}/api/heatprofiles", self.timeout)
    times_document = self.fetcher(f"{self.dashboard_url}/api/times", self.timeout)
    registry = self.fetcher(f"{self.dashboard_url}/api/device-registry", self.timeout)
    if not isinstance(registry, dict) or registry.get("Mode") != "MAC" or not isinstance(registry.get("Devices"), list):
      raise HeatHistoryError("invalid Dashboard device registry")
    mapping: dict[str, str] = {}
    for row in registry["Devices"]:
      if not isinstance(row, dict) or not isinstance(row.get("Function"), str) or not isinstance(row.get("DeviceId"), str) \
          or not re.fullmatch(r"(?:[0-9A-F]{2}:){5}[0-9A-F]{2}@[1-4]", row["DeviceId"]) or row["Function"] in mapping:
        raise HeatHistoryError("invalid/duplicate Dashboard device assignment")
      mapping[row["Function"]] = row["DeviceId"]
    for endpoint in registry.get("Endpoints", []):
      if endpoint.get("Capabilities") == 2:
        mapping[endpoint["Function"]] = endpoint["DeviceId"]
    external = registry.get("ExternalSources", {})
    if not isinstance(external, dict):
      raise HeatHistoryError("invalid Dashboard external sources")
    for function, identity in external.items():
      if not isinstance(function, str) or not function or function in mapping or not isinstance(identity, str) \
          or not re.fullmatch(r"(?:[0-9A-F]{2}:){5}[0-9A-F]{2}@3", identity):
        raise HeatHistoryError("invalid/conflicting Dashboard external source")
      mapping[function] = identity
    self.device_ids = mapping
    if registry.get("Pending"):
      raise HeatHistoryError("device assignment distribution pending")
    # Battery and other non-aquarium profiles do not describe water heating.
    profiles = profile_list(profile_document, "Dashboard /api/heatprofiles")
    return [profile for profile in profiles if PROFILE_ID_PATTERN.fullmatch(profile["Id"])
            and profile.get("Measurement", "Temperature") == "Temperature"], times_document

  def wire_id(self, function: str) -> str:
    if function not in self.device_ids:
      raise HeatHistoryError(f"no device assigned to {function}")
    return self.device_ids[function]

  def open_udp_socket(self) -> None:
    if self.udp_socket is not None:
      return
    try:
      self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
      self.udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
      self.udp_socket.bind(("", UDP_STATUS_PORT))
      self.udp_socket.setblocking(False)
    except OSError as error:
      if self.udp_socket is not None:
        self.udp_socket.close()
        self.udp_socket = None
      raise HeatHistoryError(f"cannot listen for device UDP status on port {UDP_STATUS_PORT}: {error}") from error

  def receive_udp_packets(self, timeout: float) -> None:
    self.open_udp_socket()
    assert self.udp_socket is not None
    readable, _, _ = select.select([self.udp_socket], [], [], max(0.0, timeout))
    if not readable:
      return
    while True:
      try:
        payload, _address = self.udp_socket.recvfrom(65535)
      except BlockingIOError:
        return
      try:
        packet = json.loads(payload.decode("utf-8"))
      except (UnicodeDecodeError, json.JSONDecodeError):
        continue
      self.cache_udp_status_packet(packet)

  def cache_udp_status_packet(self, packet: Any) -> bool:
    version = packet.get("Version") if isinstance(packet, dict) else None
    if isinstance(version, bool) or not isinstance(version, int) or version != UDP_STATUS_VERSION \
        or packet.get("Type") != "Status" or not isinstance(packet.get("ID"), str) \
        or not packet["ID"] or not isinstance(packet.get("Status"), dict):
      return False
    self.udp_status_cache[packet["ID"]] = (time.monotonic(), packet)
    return True

  def collect_udp_statuses(self, device_ids: set[str]) -> dict[str, Any]:
    if self.udp_status_provider is not None:
      return self.udp_status_provider(device_ids, self.status_wait)
    deadline = time.monotonic() + self.status_wait
    while True:
      now = time.monotonic()
      current = {
        device_id: packet
        for device_id, (received_at, packet) in self.udp_status_cache.items()
        if now - received_at <= self.status_max_age
      }
      if device_ids.issubset(current) or now >= deadline:
        return {device_id: current[device_id] for device_id in device_ids if device_id in current}
      self.receive_udp_packets(deadline - now)

  def report(self, profile_id: str, error: str | None) -> None:
    previous = self.reported_errors.get(profile_id)
    if error is not None and previous != error:
      print(f"{timestamp_text()} ERROR {profile_id}: {error}", file=sys.stderr, flush=True)
      self.reported_errors[profile_id] = error
    elif error is None and previous is not None:
      print(f"{timestamp_text()} OK {profile_id}: profile and status data are valid again", flush=True)
      del self.reported_errors[profile_id]

  def get_state(self, profile_id: str) -> dict[str, Any]:
    state_path = self.data_dir / profile_id / "state.json"
    try:
      value = json.loads(state_path.read_text(encoding="utf-8"))
    except FileNotFoundError:
      return {}
    except (OSError, json.JSONDecodeError) as error:
      raise HeatHistoryError(f"cannot read runtime state {state_path}: {error}") from error
    if not isinstance(value, dict):
      raise HeatHistoryError(f"invalid runtime state {state_path}")
    return value

  def sample_profile(self, profile: dict[str, Any], times_document: Any,
                     udp_statuses: dict[str, Any]) -> dict[str, Any]:
    profile_id = profile["Id"]
    match = PROFILE_ID_PATTERN.fullmatch(profile_id)
    if not match:
      raise HeatHistoryError(f"{profile_id}: expected profile Id format HeatProfileAq<number>")
    # Locate each role by the central channel assignment, regardless of its hardware host.
    hosts = {}
    channel_groups = times_document.get("Channels", {})
    for role in ("HeaterFloor", "HeaterMain"):
      matches = []
      for device_name, state in channel_groups.items():
        rows = state.get("Rows", []) if isinstance(state, dict) else state
        matches.extend((device_name, row) for row in rows
                       if isinstance(row, dict) and row.get("Role") == role and row.get("Profile") == profile_id)
      if len(matches) != 1:
        raise HeatHistoryError(f"expected one {role} channel for {profile_id}, found {len(matches)}")
      hosts[role] = matches[0]
    required = list(dict.fromkeys([profile["TempId"], *(host[0] for host in hosts.values())]))
    for device_name in required:
      if device_name not in udp_statuses:
        raise HeatHistoryError(f"no recent UDP status received from {device_name}")
    temp = temperature_value(udp_statuses[profile["TempId"]], profile["TempId"], profile.get("Measurement", "Temperature"))
    floor_host, floor_channel = hosts["HeaterFloor"]
    main_host, main_channel = hosts["HeaterMain"]
    floor = channel_state(udp_statuses[floor_host].get("Status"), floor_channel.get("Id"), floor_host)
    main = channel_state(udp_statuses[main_host].get("Status"), main_channel.get("Id"), main_host)
    return {"Time": timestamp_text(), "Temp": temp, "Floor": floor, "Main": main}

  def create_log(self, profile: dict[str, Any], sample: dict[str, Any]) -> dict[str, Any]:
    profile_id = profile["Id"]
    directory = self.data_dir / profile_id
    directory.mkdir(parents=True, exist_ok=True)
    prefix = unique_log_prefix(directory, timestamp_prefix())
    csv_name = f"{prefix}_{profile_id}.csv"
    profile_name = f"{prefix}_{profile_id}.json"
    with (directory / csv_name).open("x", encoding="utf-8", newline="") as output:
      writer = csv.writer(output, delimiter=";", lineterminator="\n")
      writer.writerow(CSV_HEADER)
      writer.writerow((sample["Time"], sample["Temp"], int(sample["Floor"]), int(sample["Main"])))
      output.flush()
      os.fsync(output.fileno())
    atomic_json_write(directory / profile_name, {"Profile": profile})
    state = {
      "ProfileDigest": profile_digest(profile),
      "CsvFile": csv_name,
      "ProfileFile": profile_name,
      "LastSample": {key: sample[key] for key in ("Temp", "Floor", "Main")},
    }
    atomic_json_write(directory / "state.json", state)
    print(f"{timestamp_text()} START {profile_id}: {csv_name}", flush=True)
    return state

  def append_if_changed(self, profile_id: str, state: dict[str, Any], sample: dict[str, Any]) -> dict[str, Any]:
    current = {key: sample[key] for key in ("Temp", "Floor", "Main")}
    if state.get("LastSample") == current:
      return state
    csv_name = state.get("CsvFile")
    if not isinstance(csv_name, str) or Path(csv_name).name != csv_name:
      raise HeatHistoryError(f"{profile_id}: invalid active CSV name in runtime state")
    csv_path = self.data_dir / profile_id / csv_name
    with csv_path.open("a", encoding="utf-8", newline="") as output:
      csv.writer(output, delimiter=";", lineterminator="\n").writerow(
        (sample["Time"], sample["Temp"], int(sample["Floor"]), int(sample["Main"]))
      )
      output.flush()
      os.fsync(output.fileno())
    state["LastSample"] = current
    atomic_json_write(self.data_dir / profile_id / "state.json", state)
    return state

  def poll_once(self) -> None:
    try:
      profiles, times_document = self.read_dashboard_data()
      self.report("Dashboard", None)
    except HeatHistoryError as error:
      self.report("Dashboard", str(error))
      return
    if not profiles:
      return

    device_ids = {profile["TempId"] for profile in profiles}
    profile_ids = {profile["Id"] for profile in profiles}
    for device_name, state in times_document.get("Channels", {}).items():
      rows = state.get("Rows", []) if isinstance(state, dict) else state
      if any(isinstance(row, dict) and row.get("Profile") in profile_ids and
             row.get("Role") in ("HeaterFloor", "HeaterMain") for row in rows):
        device_ids.add(device_name)
    try:
      wire_ids = {function: self.wire_id(function) for function in device_ids if function in self.device_ids}
      physical_statuses = self.collect_udp_statuses(set(wire_ids.values()))
      udp_statuses = {function: physical_statuses[wire] for function, wire in wire_ids.items() if wire in physical_statuses}
    except HeatHistoryError as error:
      for profile in profiles:
        self.report(profile["Id"], str(error))
      return

    for profile in profiles:
      profile_id = profile["Id"]
      try:
        self.wire_id(profile["TempId"])
        sample = self.sample_profile(profile, times_document, udp_statuses)
        digest = profile_digest(profile)
        state = self.get_state(profile_id)
        if state.get("ProfileDigest") != digest:
          state = self.create_log(profile, sample)
        else:
          self.append_if_changed(profile_id, state, sample)
        self.report(profile_id, None)
      except (HeatHistoryError, OSError) as error:
        self.report(profile_id, str(error))


def main() -> int:
  parser = argparse.ArgumentParser(description="Passively record verified aquarium heating status")
  parser.add_argument("--version", action="version", version=Path(__file__).with_name("Versions.txt").read_text(encoding="utf-8").strip().split("=", 1)[1])
  parser.add_argument("--config", type=Path, default=Path(__file__).with_name("config.json"))
  parser.add_argument("--once", action="store_true", help="perform one poll and exit")
  parser.add_argument("--no-server", action="store_true", help="record only, without the chart server")
  parser.add_argument("--host", default="127.0.0.1", help="chart server bind address")
  parser.add_argument("--port", type=int, default=8091, help="chart server port")
  arguments = parser.parse_args()
  if not 1 <= arguments.port <= 65535:
    parser.error("--port must be between 1 and 65535")
  server = None
  worker = None
  def stop_service(signum: int, frame: Any) -> None:
    raise KeyboardInterrupt
  previous_handler = signal.signal(signal.SIGTERM, stop_service)
  try:
    monitor = HeatHistoryMonitor(arguments.config)
    if not arguments.once and not arguments.no_server:
      server = chart_server(monitor.data_dir, arguments.host, arguments.port)
      worker = threading.Thread(target=server.serve_forever, name="HeatHistoryCharts", daemon=True)
      worker.start()
      print(f"HeatHistory charts: http://{arguments.host}:{server.server_port}/", flush=True)
    while True:
      started = time.monotonic()
      monitor.poll_once()
      if arguments.once:
        return 0
      remaining = monitor.poll_interval - (time.monotonic() - started)
      if remaining > 0:
        time.sleep(remaining)
  except KeyboardInterrupt:
    return 0
  except (HeatHistoryError, OSError) as error:
    print(f"{timestamp_text()} ERROR: {error}", file=sys.stderr)
    return 2
  finally:
    signal.signal(signal.SIGTERM, previous_handler)
    if server is not None:
      server.shutdown()
      server.server_close()
    if worker is not None:
      worker.join()
    if "monitor" in locals() and monitor.udp_socket is not None:
      monitor.udp_socket.close()


if __name__ == "__main__":
  raise SystemExit(main())
