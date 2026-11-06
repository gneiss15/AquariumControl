#!/usr/bin/env python3
# HeatHistory.py, Version: 1.30

from __future__ import annotations
from StorageRetention import StorageRetention

import argparse
from BootSession import BOOT_ID
import csv
import datetime as dt
import hashlib
import json
from Messages import ReadJson, WriteJson, WriteMessage, ValidateType, ValidateMembers, TYPES, UdpPort
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
from pathlib import Path
from typing import Any

from CsvFiles import CsvFiles
from DashboardEvents import DashboardEvents
from BrowserServer import chart_server
from EnergyHistory import EnergyHistory, role_matches
from PiHistory import PiHistory
from UdpDescriptions import UdpDescriptions


CSV_HEADER = ("Time", "Temp", "Floor", "Main")
PROFILE_FIELDS = tuple(Field["Name"] for Field in TYPES["TJsonProfile"]["Fields"])
PROFILE_ID_PATTERN = re.compile(r"^HeatProfile(Aq[0-9]+)$")
UDP_STATUS_PORT = UdpPort


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
  if not ValidateType("TJsonProfile", profile):
    raise HeatHistoryError("profile does not match MessageDefinition.inc.h")
  selected = {}
  for field in TYPES["TJsonProfile"]["Fields"]:
    key = field["Name"]
    if key in profile:
      selected[key] = float(profile[key]) if field["Type"] == "double" else profile[key]
  measurement = profile.get("Meas", "Temperature")
  if measurement != "Temperature": selected["Meas"] = measurement
  return WriteJson("TJsonProfile", selected, sort_keys=True, separators=(",", ":")).encode("utf-8")


def profile_digest(profile: dict[str, Any]) -> str:
  return hashlib.sha256(canonical_profile(profile)).hexdigest()


def profile_list(document: Any, source: str) -> list[dict[str, Any]]:
  if not isinstance(document, dict) or not isinstance(document.get("Profs"), list):
    raise HeatHistoryError(f"{source}: expected a Profiles array")
  result: list[dict[str, Any]] = []
  seen: set[str] = set()
  for profile in document["Profs"]:
    canonical_profile(profile)
    profile_id = profile["ProfName"]
    if profile_id in seen:
      raise HeatHistoryError(f"{source}: duplicate profile Id {profile_id}")
    seen.add(profile_id)
    result.append(profile)
  return result


def find_channel(document: Any, device_name: str, profile_id: str, role: str) -> dict[str, Any]:
  channels = document.get("Chs") if isinstance(document, dict) else None
  device_channels = channels.get(device_name) if isinstance(channels, dict) else None
  if isinstance(device_channels, dict):
    device_channels = device_channels.get("Rows")
  if not isinstance(device_channels, list):
    raise HeatHistoryError(f"Dashboard Times.json: no channel rows for {device_name}")
  matches = [channel for channel in device_channels
             if isinstance(channel, dict) and role_matches(channel.get("Role"),role)
             and channel.get("Prof") == profile_id]
  if len(matches) != 1:
    raise HeatHistoryError(
      f"Dashboard Times.json: expected one {role} channel for {profile_id} on {device_name}, found {len(matches)}"
    )
  return matches[0]


def channel_state(status: Any, channel_name: str, source: str) -> bool:
  if not isinstance(status, dict) or not isinstance(status.get("Chs"), list):
    raise HeatHistoryError(f"{source}: expected a Channels status array")
  matches = [channel for channel in status["Chs"]
             if isinstance(channel, dict) and channel.get("Id") == channel_name]
  if len(matches) != 1:
    raise HeatHistoryError(f"{source}: expected one status channel {channel_name}, found {len(matches)}")
  value = matches[0].get("State")
  if not isinstance(value, bool):
    raise HeatHistoryError(f"{source}: channel {channel_name} has no valid boolean State")
  return value


def temperature_value(packet: Any, source: str, measurement: str = "Temperature") -> float:
  values = packet.get("Status")
  field = {"Temperature":"Temp","BatteryPercent":"BatteryPct"}.get(measurement,measurement)
  if not isinstance(values, dict) or field not in values:
    raise HeatHistoryError(f"{source}: UDP packet has no {field} field")
  value = values[field]
  if value is None or isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
    raise HeatHistoryError(f"{source}: UDP measurement is invalid")
  return float(value)


def atomic_json_write(path: Path, value: Any, contract: str) -> None:
  path.parent.mkdir(parents=True, exist_ok=True)
  descriptor, temporary_name = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
  try:
    with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as output:
      output.write(WriteJson(contract, value, indent=2, sort_keys=True))
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
  def __init__(self, config_path: Path):
    self.config_path = config_path.resolve()
    self.project_dir = self.config_path.parent
    try:
      self.config = ReadJson("TJsonHeatHistoryConfig", self.config_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
      raise HeatHistoryError(f"cannot read config {self.config_path}: {error}") from error
    if not isinstance(self.config, dict):
      raise HeatHistoryError("config must be a JSON object")
    self.data_dir = self.resolve_path(self.config.get("DataDir", "data"))
    self.data_dir.mkdir(parents=True,exist_ok=True)
    self.dashboard_url = self.config.get("DashboardUrl", "http://192.168.99.51:8090").rstrip("/")
    if not isinstance(self.dashboard_url, str) or not self.dashboard_url.startswith(("http://", "https://")):
      raise HeatHistoryError("DashboardUrl must use http:// or https://")
    self.status_max_age = float(self.config.get("StatusMaxAgeSec", 15))
    if not 5 <= self.status_max_age <= 300:
      raise HeatHistoryError("StatusMaxAgeSeconds must be between 5 and 300")
    view_days = self.config.get("ViewRetentionDays",30)
    if isinstance(view_days,bool) or not isinstance(view_days,int) or not 1 <= view_days <= 366:
      raise HeatHistoryError("ViewRetentionDays must be an integer in 1..366")
    view_start = self.config.get("ActiveFrom")
    if view_start is not None:
      from BrowserServer import instant
      try:
        if not isinstance(view_start,str): raise ValueError("not a timestamp")
        instant(view_start)
      except ValueError as error:
        raise HeatHistoryError("ActiveFrom must be an ISO timestamp with timezone") from error
    self.udp_socket: socket.socket | None = None
    self.udp_query_socket: socket.socket | None = None
    self.udp_descriptions = UdpDescriptions()
    self.udp_status_cache: dict[str, tuple[float, dict[str, Any]]] = {}
    self.reported_errors: dict[str, str] = {}
    self.device_ids: dict[str, str] = {}
    self.profiles={}
    self.profile_documents={}
    self.channel_documents={}
    self.registry_document=None
    self.states={}
    self.routes={}
    self.csv=CsvFiles()
    self.configured=False
    self.events=None
    try:
      self.retention = StorageRetention(self.data_dir,self.config)
      self.pi = PiHistory(self.data_dir)
      self.energy = EnergyHistory(self.data_dir,self.config)
    except ValueError as error:
      raise HeatHistoryError(str(error)) from error

  def resolve_path(self, value: Any) -> Path:
    if not isinstance(value, str) or not value:
      raise HeatHistoryError("configured paths must be non-empty strings")
    path = Path(value).expanduser()
    return path if path.is_absolute() else (self.project_dir / path).resolve()


  def DecodeDashboardData(self,profile_document,times_document,registry):
    if not ValidateType("TJsonRegistryView",registry):
      raise HeatHistoryError("invalid Dashboard device registry")
    if not ValidateType("TJsonRecordingChannels",times_document):
      raise HeatHistoryError("invalid Dashboard channel groups")
    mapping: dict[str, str] = {}
    for row in registry["Devices"]:
      if row["Func"] in mapping:
        raise HeatHistoryError("duplicate Dashboard device assignment")
      mapping[row["Func"]] = row["DevId"]
    for endpoint in registry.get("Endpoints", []):
      if endpoint["Caps"] == 2:
        mapping[endpoint["Func"]] = endpoint["OutAddress"].split('/')[0]
    for function, identity in registry.get("ExternalSrcs", {}).items():
      if not function or function in mapping:
        raise HeatHistoryError("conflicting Dashboard external source")
      mapping[function] = identity
    self.device_ids = mapping
    if registry.get("Pending"):
      raise HeatHistoryError("device assignment distribution pending")
    # Battery and other non-aquarium profiles do not describe water heating.
    profiles = profile_list(profile_document, "Dashboard /api/heatprofiles")
    return [profile for profile in profiles if PROFILE_ID_PATTERN.fullmatch(profile["ProfName"])
            and profile.get("Meas", "Temperature") == "Temperature"], times_document

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
      self.udp_query_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
      self.udp_query_socket.bind(("", 0))
      self.udp_query_socket.setblocking(False)
    except OSError as error:
      if self.udp_socket is not None:
        self.udp_socket.close()
        self.udp_socket = None
      raise HeatHistoryError(f"cannot listen for device UDP status on port {UDP_STATUS_PORT}: {error}") from error

  def receive_udp_packets(self, timeout: float) -> None:
    self.open_udp_socket()
    assert self.udp_socket is not None
    readable, _, _ = select.select([self.udp_socket, self.udp_query_socket], [], [], max(0.0, timeout))
    for receiver in readable:
      while True:
        try:
          payload, address = receiver.recvfrom(65535)
        except BlockingIOError:
          break
        self.OnUdpPacket(payload, address=address)

  def OnUdpPacket(self, packet: Any, received_at: float | None = None, wall_time: float | None = None,
                  address: tuple | None = None) -> bool:
    now = time.monotonic() if received_at is None else received_at
    wall = time.time() if wall_time is None else wall_time
    sample, request = self.udp_descriptions.OnUdpPacket(
      packet, now, wall, self.device_ids.values() if address is not None else None)
    if request is not None and address is not None:
      self.udp_query_socket.sendto(WriteMessage(request).encode(),
                                   (address[0], 44500 + int(request['ID'][-1])))
    if sample is None:
      return False
    self.OnStatusPacket(*sample)
    return True

  def OnStatusPacket(self, packet: dict, received_at: float, wall_time: float) -> None:
    """Record an already validated and expanded status with its original times."""
    now = received_at
    self.udp_status_cache[packet["ID"]] = (now, packet)
    try:
      self.energy.ingest(packet,now,wall_time)
      self.pi.ingest(packet)
      if self.configured:
        for profile_id in self.routes.get(packet['ID'],()):
          self.RecordProfile(profile_id,now)
    except (OSError,ValueError) as error:
      self.energy.pause()
      self.report("Energy",str(error))


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
      value = ReadJson("TJsonHistoryState", state_path.read_text(encoding="utf-8"))
    except FileNotFoundError:
      return {}
    except (OSError, ValueError) as error:
      raise HeatHistoryError(f"cannot read runtime state {state_path}: {error}") from error
    if not isinstance(value, dict):
      raise HeatHistoryError(f"invalid runtime state {state_path}")
    return value


  def create_log(self, profile: dict[str, Any], sample: dict[str, Any]) -> dict[str, Any]:
    profile_id = profile["ProfName"]
    directory = self.data_dir / profile_id
    directory.mkdir(parents=True, exist_ok=True)
    prefix = unique_log_prefix(directory, timestamp_prefix())
    csv_name = f"{prefix}_{profile_id}.csv"
    profile_name = f"{prefix}_{profile_id}.json"
    self.csv.create(directory/csv_name,CSV_HEADER)
    self.csv.write(directory/csv_name,(sample['Time'],sample['Temp'],int(sample['Floor']),int(sample['Main'])),durable=True)
    atomic_json_write(directory / profile_name, {"Prof": profile}, "TJsonHistoryMetadata")
    state = {
      "HostBootId": BOOT_ID,
      "ProfDigest": profile_digest(profile),
      "CsvFile": csv_name,
      "ProfFile": profile_name,
      "LastSample": {key: sample[key] for key in ("Temp", "Floor", "Main")},
    }
    if profile_id in self.energy.assignments:
      state['AssignmentSignature']=self.energy.assignments[profile_id]['Signature']
    atomic_json_write(directory / "state.json", state, "TJsonHistoryState")
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
    self.csv.write(csv_path,(sample["Time"],sample["Temp"],int(sample["Floor"]),int(sample["Main"])),durable=True)
    state["LastSample"] = current
    atomic_json_write(self.data_dir / profile_id / "state.json", state, "TJsonHistoryState")
    return state

  def OnProfileChange(self,changes):
    for profile_id,profile in changes.items():
      if profile is None:
        self.profile_documents.pop(profile_id,None)
      else:
        self.profile_documents[profile_id]=profile

  def OnChannelChange(self,changes):
    for device,channels in changes.items():
      if channels is None:
        self.channel_documents.pop(device,None)
      else:
        self.channel_documents[device]=channels

  def OnRegistryChange(self,registry):
    self.registry_document=registry

  def OnDashboardEvent(self,event):
    """Apply one atomic snapshot/patch, then rebind only changed recordings."""
    if 'ConnectionError' in event:
      self.report('Dashboard',event['ConnectionError'])
      return # Keep recording with the last complete, valid configuration.
    if event.get('Snapshot'):
      self.profile_documents={}
      self.channel_documents={}
      self.registry_document=None
    self.OnProfileChange(event.get('Profs',{}))
    self.OnChannelChange(event.get('Chs',{}))
    if 'Registry' in event:
      self.OnRegistryChange(event['Registry'])
    try:
      self.ApplyConfiguration()
    except (HeatHistoryError,OSError,ValueError) as error:
      self.configured=False
      self.energy.pause()
      self.report('Dashboard',str(error))

  def ApplyConfiguration(self):
    """Validate the complete cached configuration, preserving unchanged writers."""
    profiles,times=self.DecodeDashboardData({'Profs':list(self.profile_documents.values())},
      {'Chs':self.channel_documents},self.registry_document)
    old={key:value['Signature'] for key,value in self.energy.assignments.items()}
    self.energy.configure(profiles,times,self.device_ids)
    self.pi.configure(self.energy.assignments)
    for profile_id in list(self.states):
      assignment=self.energy.assignments.get(profile_id)
      if assignment is None or old.get(profile_id)!=assignment['Signature']:
        state=self.states.pop(profile_id)
        if state.get('CsvFile'):self.csv.close(self.data_dir/profile_id/state['CsvFile'])
        for key in list(self.pi.files):
          if key[0]==profile_id:self.pi.csv.close(self.pi.files.pop(key)[0])
    self.profiles={profile['ProfName']:profile for profile in profiles}
    for profile_id in self.profiles:
      if profile_id not in self.energy.assignments:
        self.report(profile_id,'incomplete temperature/main/floor assignment')
    self.routes={}
    for profile_id,assignment in self.energy.assignments.items():
      if profile_id not in self.states:
        self.states[profile_id]=self.get_state(profile_id)
      state=self.states[profile_id]
      signature=assignment['Signature']
      # Persist assignment identity with resume state; never append under a changed binding.
      if state.get('AssignmentSignature',signature)!=signature or (profile_id in old and old[profile_id]!=signature):
        state.clear()
      for identity in (assignment['Temp'],assignment['Main'][0],assignment['Floor'][0]):
        self.routes.setdefault(identity,set()).add(profile_id)
    self.configured=True
    self.report('Dashboard',None)

  def CurrentStatus(self,identity,now):
    received,packet=self.udp_status_cache.get(identity,(0,None))
    if packet is None or now-received>self.status_max_age:
      raise HeatHistoryError('no recent UDP status received from '+identity)
    return packet

  def RecordProfile(self,profile_id,now):
    """Route a received packet to its temperature/switch recording."""
    assignment=self.energy.assignments[profile_id]
    profile=self.profiles[profile_id]
    try:
      sample={'Time':timestamp_text(),
        'Temp':temperature_value(self.CurrentStatus(assignment['Temp'],now),assignment['Temp'],profile.get('Meas','Temperature')),
        'Floor':channel_state(self.CurrentStatus(assignment['Floor'][0],now)['Status'],assignment['Floor'][1],assignment['Floor'][0]),
        'Main':channel_state(self.CurrentStatus(assignment['Main'][0],now)['Status'],assignment['Main'][1],assignment['Main'][0])}
      state=self.states[profile_id]
      if state.get('ProfDigest')!=profile_digest(profile) or state.get('HostBootId')!=BOOT_ID:
        if state.get('CsvFile'):self.csv.close(self.data_dir/profile_id/state['CsvFile'])
        state=self.create_log(profile,sample)
        self.states[profile_id]=state
      state['AssignmentSignature']=assignment['Signature']
      self.append_if_changed(profile_id,state,sample)
      self.report(profile_id,None)
    except (HeatHistoryError,OSError) as error:
      self.report(profile_id,str(error))

  def MaintainStorage(self):
    active=list(self.csv.files)+list(self.energy.csv.files)+list(self.pi.csv.files)
    active.extend(entry['Path'] for entry in self.energy.sessions.values())
    active.extend(entry[0] for entry in self.pi.files.values())
    active.extend(self.data_dir/profile_id/state['CsvFile'] for profile_id,state in self.states.items() if state.get('CsvFile'))
    self.retention.maintain(active)

  def Start(self):
    self.MaintainStorage()
    self.open_udp_socket()
    self.events=DashboardEvents(self.dashboard_url)
    self.events.Start()

  def Run(self):
    """Wait for UDP or Dashboard changes; no profile/status polling."""
    self.Start()
    while True:
      readable,_,_=select.select([self.udp_socket,self.udp_query_socket,self.events.reader],[],[])
      if self.events.reader in readable:
        for event in self.events.Read():
          self.OnDashboardEvent(event)
      if self.udp_socket in readable or self.udp_query_socket in readable:
        self.receive_udp_packets(0)

  def Close(self):
    if self.events:
      self.events.Close()
    self.csv.close_all()
    self.energy.csv.close_all()
    self.pi.csv.close_all()
    if self.udp_socket:
      self.udp_socket.close()
    if self.udp_query_socket:
      self.udp_query_socket.close()


def main() -> int:
  parser = argparse.ArgumentParser(description="Passively record verified aquarium heating status")
  parser.add_argument("--version", action="version", version=Path(__file__).with_name("Versions.txt").read_text(encoding="utf-8").strip().split("=", 1)[1])
  parser.add_argument("--config", type=Path, default=Path(__file__).with_name("config.json"))
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
    if not arguments.no_server:
      server = chart_server(monitor.data_dir, arguments.host, arguments.port,
        since=monitor.config.get("ViewFromUTC"),retention_days=monitor.config.get("ViewRetentionDays",30))
      worker = threading.Thread(target=server.serve_forever, name="HeatHistoryCharts", daemon=True)
      worker.start()
      print(f"HeatHistory charts: http://{arguments.host}:{server.server_port}/", flush=True)
    monitor.Run()
    return 0
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
    if "monitor" in locals():
      monitor.Close()


if __name__ == "__main__":
  raise SystemExit(main())
