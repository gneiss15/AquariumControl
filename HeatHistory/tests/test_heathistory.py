#!/usr/bin/env python3
# test_heathistory.py, Version: 1.16

import contextlib
import csv
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from HeatHistory import HeatHistoryError, HeatHistoryMonitor, temperature_value, canonical_profile


def profile(main_on: float = 27.8) -> dict:
  return {
    "Id": "HeatProfileAq1",
    "TempId": "FeederAq1",
    "MainOn": main_on,
    "FloorOn": 27.9,
    "MainOff": 28.0,
    "FloorOff": 28.0,
    "NoTempOn": True,
  }


class HeatHistoryTests(unittest.TestCase):
  def test_old_or_missing_udp_version_rejected(self):
    for packet in ({"Value": 28}, {"Temperature": 28}, {"Version": 1, "Value": 28},
                   {"Version": 1, "Status": {"Temperature": 28}},
                   {"Version": True, "Status": {"Temperature": 28}}):
      with self.assertRaises(HeatHistoryError):
        temperature_value(packet, "test")

  def test_measurement_profile_signature(self):
    original = profile()
    self.assertEqual(canonical_profile(original), canonical_profile({**original, "Measurement": "Temperature"}))
    self.assertNotEqual(canonical_profile(original), canonical_profile({**original, "Measurement": "BatteryPercent"}))
    for field in ("", "Kind", "Status.Temperature", None, True):
      with self.assertRaises(HeatHistoryError):
        canonical_profile({**original, "Measurement": field})

  def test_temperature_keeps_sensor_resolution(self):
    for value in [28.0625,27.9375,-0.0625]:
      self.assertEqual(temperature_value({'Version':2,'Status':{'Temperature':value}},'test'),value)

  def test_v2_named_measurements(self):
    packet = {"Version": 2, "Value": 99, "Status": {"Temperature": 28, "BatteryPercent": 65}}
    self.assertEqual(temperature_value(packet, "test"), 28)
    self.assertEqual(temperature_value(packet, "test", "BatteryPercent"), 65)
    with self.assertRaises(HeatHistoryError):
      temperature_value({"Version": 1, "Value": 65}, "test", "BatteryPercent")
    for status in ({}, {"BatteryPercent": None}, {"BatteryPercent": True}, {"BatteryPercent": "65"}):
      with self.assertRaises(HeatHistoryError):
        temperature_value({**packet, "Status": status}, "test", "BatteryPercent")
    with self.assertRaises(HeatHistoryError):
      temperature_value({**packet, "Version": 3}, "test")

  def setUp(self) -> None:
    self.temp_dir = tempfile.TemporaryDirectory()
    self.root = Path(self.temp_dir.name)
    self.config_file = self.root / "config.json"
    self.config_file.write_text(json.dumps({
      "DataDir": str(self.root / "data"),
      "DashboardUrl": "http://dashboard.test",
      "StatusMaxAgeSeconds": 5,
    }), encoding="utf-8")
    self.dashboard_profile = profile()
    self.main_state = False
    self.floor_state = True
    self.temp_value = 27.5
    self.fetch_calls: list[str] = []
    self.registry_mode = "MAC"
    self.registry_pending = False
    self.mapping = {"GN-Pi": "02:11:22:33:44:55@1", "HeaterMainAq1": "02:22:33:44:55:66@4", "FeederAq1": "02:33:44:55:66:77@3"}
    self.requested_udp: set[str] = set()
    self.fetcher = self.fake_fetch
    self.monitor = HeatHistoryMonitor(self.config_file)

  def tearDown(self) -> None:
    self.monitor.Close()
    self.temp_dir.cleanup()

  def fake_fetch(self, url: str, _timeout: float) -> dict:
    self.fetch_calls.append(url)
    if url == "http://dashboard.test/api/heatprofiles":
      return {"Profiles": [dict(self.dashboard_profile)]}
    if url == "http://dashboard.test/api/times":
      return {
        "Channels": {
          "GN-Pi": {"Rows": [{"Id": "FloorAq1", "Role": "HeaterFloor", "Profile": "HeatProfileAq1"}]},
          "HeaterMainAq1": {"Rows": [{"Id": "MainAq1", "Role": "HeaterMain", "Profile": "HeatProfileAq1"}]},
        },
      }
    if url == "http://dashboard.test/api/device-registry":
      return {"Mode": self.registry_mode, "Pending": self.registry_pending, "Devices": [{"Function": function, "DeviceId": identity} for function, identity in self.mapping.items()]}
    raise AssertionError(f"unexpected URL: {url}")

  def fake_udp_statuses(self, device_ids: set[str], _timeout: float) -> dict[str, dict]:
    self.requested_udp = device_ids
    self.assertEqual(device_ids, set(self.mapping.values()))
    result = {
      "GN-Pi": {"Status": {"Channels": [{"Name": "FloorAq1", "State": self.floor_state}]}},
      "HeaterMainAq1": {"Status": {"Channels": [{"Name": "MainAq1", "State": self.main_state}]}},
      "FeederAq1": {"Version": 2, "Status": {"Temperature": self.temp_value}},
    }
    return {self.mapping[key]: value for key, value in result.items()}

  def DeliverEvents(self):
    profiles=self.fetcher('http://dashboard.test/api/heatprofiles',1)
    times=self.fetcher('http://dashboard.test/api/times',1)
    registry=self.fetcher('http://dashboard.test/api/device-registry',1)
    self.monitor.OnDashboardEvent({'Snapshot':True,
      'Profiles':{p['Id']:p for p in profiles['Profiles']},
      'Channels':times['Channels'],'Registry':registry})
    for identity,packet in self.fake_udp_statuses(set(self.mapping.values()),1).items():
      self.monitor.OnUdpPacket(dict(packet,Version=2,Type='Status',ID=identity))

  def log_files(self) -> list[Path]:
    return sorted((self.root / "data" / "HeatProfileAq1").glob("*_HeatProfileAq1.csv"))

  def test_swapped_floor_and_main_hardware_are_followed(self) -> None:
    profiles, times = self.monitor.DecodeDashboardData(
      self.fake_fetch("http://dashboard.test/api/heatprofiles",1),
      self.fake_fetch("http://dashboard.test/api/times",1),
      self.fake_fetch("http://dashboard.test/api/device-registry",1))
    floor = times["Channels"]["GN-Pi"]["Rows"][0]
    main = times["Channels"]["HeaterMainAq1"]["Rows"][0]
    times["Channels"]["GN-Pi"]["Rows"] = [main]
    times["Channels"]["HeaterMainAq1"]["Rows"] = [floor]
    states = {
      "GN-Pi": {"Status": {"Channels": [{"Name": main["Id"], "State": False}]}},
      "HeaterMainAq1": {"Status": {"Channels": [{"Name": floor["Id"], "State": True}]}},
      "FeederAq1": {"Version": 2, "Status": {"Temperature": 27.5}},
    }
    self.monitor.OnDashboardEvent({'Snapshot':True,'Profiles':{p['Id']:p for p in profiles},
      'Channels':times['Channels'],'Registry':self.fake_fetch('http://dashboard.test/api/device-registry',1)})
    for function,packet in states.items():
      self.monitor.OnUdpPacket(dict(packet,Version=2,Type='Status',ID=self.mapping[function]))
    with self.log_files()[0].open() as stream:
      row=list(csv.DictReader(stream,delimiter=';'))[-1]
    self.assertEqual((row['Floor'],row['Main']),('1','0'))

  def test_mac_assignments_and_function_swap(self) -> None:
    self.registry_mode = "MAC"
    self.DeliverEvents()
    self.assertEqual(self.requested_udp, set(self.mapping.values()))
    first = self.mapping["FeederAq1"]
    self.mapping["FeederAq1"] = "02:44:55:66:77:88@3"
    self.DeliverEvents()
    self.assertNotIn(first, self.requested_udp)
    self.assertIn(self.mapping["FeederAq1"], self.requested_udp)
    self.assertEqual(len(self.log_files()), 2)

  def test_external_source_mapping(self):
    fetch = self.fetcher
    def external(url, timeout):
      result = fetch(url, timeout)
      if url.endswith('/api/device-registry'):
        result['ExternalSources'] = {'LaptopBattery': 'F4:06:69:2A:E9:CD@3'}
      return result
    self.fetcher = external
    self.DeliverEvents()
    self.assertEqual(self.monitor.wire_id('LaptopBattery'), 'F4:06:69:2A:E9:CD@3')

  def test_unresolved_other_profile_does_not_block_aquarium(self):
    fetch = self.fetcher
    def additional(url, timeout):
      result = fetch(url, timeout)
      if url.endswith('/api/heatprofiles'):
        result['Profiles'].append({**profile(), 'Id': 'LaptopCharge', 'TempId': 'MissingLaptop'})
      return result
    self.fetcher = additional
    errors = io.StringIO()
    with contextlib.redirect_stderr(errors):
      self.DeliverEvents()
    self.assertEqual(len(self.log_files()), 1)
    self.assertNotIn('LaptopCharge', errors.getvalue())
    self.assertNotIn('incomplete temperature/main/floor assignment', errors.getvalue())

  def test_pending_assignment_pauses_logging(self) -> None:
    self.registry_mode = "MAC"
    self.registry_pending = True
    errors = io.StringIO()
    with contextlib.redirect_stderr(errors):
      self.DeliverEvents()
    self.assertIn("assignment distribution pending", errors.getvalue())
    self.assertEqual(self.log_files(), [])

  def test_initial_log_header_baseline_and_change_detection(self) -> None:
    self.DeliverEvents()
    self.assertEqual(self.fetch_calls, [
      "http://dashboard.test/api/heatprofiles", "http://dashboard.test/api/times", "http://dashboard.test/api/device-registry"
    ])
    files = self.log_files()
    self.assertEqual(len(files), 1)
    with files[0].open(encoding="utf-8", newline="") as source:
      rows = list(csv.reader(source, delimiter=";"))
    self.assertEqual(rows[0], ["Time", "Temp", "Floor", "Main"])
    self.assertEqual(rows[1][1:], ["27.5", "1", "0"])
    self.DeliverEvents()
    with files[0].open(encoding="utf-8", newline="") as source:
      self.assertEqual(len(list(csv.reader(source, delimiter=";"))), 2)
    self.main_state = True
    self.DeliverEvents()
    self.assertTrue(all(url.startswith("http://dashboard.test/api/") for url in self.fetch_calls))
    with files[0].open(encoding="utf-8", newline="") as source:
      rows = list(csv.reader(source, delimiter=";"))
    self.assertEqual(len(rows), 3)
    self.assertEqual(rows[-1][1:], ["27.5", "1", "1"])

  def test_semantic_profile_change_starts_new_log_with_baseline(self) -> None:
    self.DeliverEvents()
    self.dashboard_profile = profile(main_on=27.7)
    self.DeliverEvents()
    files = self.log_files()
    self.assertEqual(len(files), 2)
    with files[-1].open(encoding="utf-8", newline="") as source:
      rows = list(csv.reader(source, delimiter=";"))
    self.assertEqual(len(rows), 2)
    self.assertEqual(rows[0], ["Time", "Temp", "Floor", "Main"])
    self.assertEqual(rows[1][1:], ["27.5", "1", "0"])
    snapshots = sorted((self.root / "data" / "HeatProfileAq1").glob("*_HeatProfileAq1.json"))
    self.assertEqual(len(snapshots), 2)

  def test_missing_udp_device_status_pauses_only_affected_profile(self) -> None:
    self.fake_udp_statuses = lambda _ids, _timeout: {self.mapping["GN-Pi"]: {"Status": {"Channels": []}}}
    errors = io.StringIO()
    with contextlib.redirect_stderr(errors):
      self.DeliverEvents()
    self.assertIn("no recent UDP status received from 02:33:44:55:66:77@3", errors.getvalue())
    self.assertEqual(self.log_files(), [])

  def test_udp_listener_cache_accepts_status_packets_only(self) -> None:
    packet = {
      "Version": 2,
      "Type": "Status",
      "ID": "FeederAq1",
      "Status": {"Temperature": 27.5, "Measurements": [], "Channels": []},
    }
    self.assertTrue(self.monitor.OnUdpPacket(packet))
    self.assertIn("FeederAq1", self.monitor.udp_status_cache)
    self.assertTrue(self.monitor.OnUdpPacket({**packet, "Version": 2, "Status": {"Temperature": 28}}))
    self.assertFalse(self.monitor.OnUdpPacket({**packet, "Version": 1}))
    self.assertFalse(self.monitor.OnUdpPacket({**packet, "Version": 3}))
    self.assertFalse(self.monitor.OnUdpPacket({**packet, "Type": "Request"}))

  def test_temperature_rotation_follows_system_boot_not_utc_day(self) -> None:
    self.DeliverEvents()
    original=self.log_files()[0]
    state_path=original.parent/'state.json'
    state=json.loads(state_path.read_text());state['Day']='previous-day'
    state_path.write_text(json.dumps(state))
    self.DeliverEvents()
    self.assertEqual(self.log_files(),[original])
    with patch('HeatHistory.BOOT_ID','next-system-boot'):
      self.DeliverEvents()
    self.assertEqual(len(self.log_files()),2)

  def test_udp_temperature_is_written_in_celsius(self) -> None:
    self.DeliverEvents()
    with self.log_files()[0].open(encoding="utf-8", newline="") as source:
      rows = list(csv.reader(source, delimiter=";"))
    self.assertEqual(rows[1][1], "27.5")

  def test_invalid_dashboard_url_is_rejected(self) -> None:
    config = json.loads(self.config_file.read_text(encoding="utf-8"))
    config["DashboardUrl"] = "gn-pi:8090"
    self.config_file.write_text(json.dumps(config), encoding="utf-8")
    with self.assertRaisesRegex(HeatHistoryError, "DashboardUrl must use http"):
      HeatHistoryMonitor(self.config_file)



if __name__ == "__main__":
  unittest.main()
