#!/usr/bin/env python3
# MonitorPowerMeters.py, Version: 1.01
"""Read-only long-term logger for the four TmrSwA8T power-meter APIs."""

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import csv
from datetime import datetime, timezone
import json
from pathlib import Path
import time
from urllib.error import URLError
from urllib.request import urlopen


DEVICES = {
    "LightTimerAq3": "http://192.168.99.102",
    "HeaterMainAq1": "http://192.168.99.105",
    "HeaterMainAq2": "http://192.168.99.106",
    "HeaterMainAq3": "http://192.168.99.107",
}
FIELDS = (
    "TimestampUtc", "DeviceId", "FirmwareVersion", "DataVersion", "RelayOn",
    "Voltage", "Current", "ActivePower", "Valid", "CfEdges", "CfDelta",
    "Cf1Edges", "Cf1Delta", "VoltageIntervals", "VoltageIntervalsDelta",
    "CurrentIntervals", "CurrentIntervalsDelta", "SelectorLevel", "LoadEnabled",
    "Error",
)


def get_json(base_url, path, timeout):
    with urlopen(base_url + path, timeout=timeout) as response:
        return json.loads(response.read().decode("utf-8"))


def read_device(device_id, base_url, timeout):
    info = get_json(base_url, "/api/v1/device", timeout)
    status = get_json(base_url, "/api/v1/status", timeout)
    channels = status.get("Channels") or [{}]
    meter = status.get("PowerMeter") or {}
    diagnostics = meter.get("Diagnostics") or {}
    return {
        "TimestampUtc": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
        "DeviceId": device_id,
        "FirmwareVersion": info.get("FirmwareVersion", ""),
        "DataVersion": info.get("DataVersion", ""),
        "RelayOn": channels[0].get("State", ""),
        "Voltage": meter.get("Voltage", ""),
        "Current": meter.get("Current", ""),
        "ActivePower": meter.get("ActivePower", ""),
        "Valid": meter.get("Valid", ""),
        "CfEdges": diagnostics.get("CfEdges", ""),
        "Cf1Edges": diagnostics.get("Cf1Edges", ""),
        "VoltageIntervals": diagnostics.get("VoltageIntervals", ""),
        "CurrentIntervals": diagnostics.get("CurrentIntervals", ""),
        "SelectorLevel": diagnostics.get("SelectorLevel", ""),
        "LoadEnabled": diagnostics.get("LoadEnabled", ""),
        "Error": "",
    }


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hours", type=float, default=12, help="Laufzeit in Stunden (Standard: 12)")
    parser.add_argument("--interval", type=float, default=2,
                        help="Abstand zwischen Messrunden in Sekunden (Standard: 2)")
    parser.add_argument("--timeout", type=float, default=4,
                        help="HTTP-Timeout je Anfrage in Sekunden (Standard: 4)")
    parser.add_argument("--output", type=Path,
                        help="CSV-Ziel; Standard: generated/Logs/PowerMonitor_<UTC-Zeit>.csv")
    args = parser.parse_args()
    if args.hours <= 0 or args.interval <= 0 or args.timeout <= 0:
        parser.error("hours, interval und timeout müssen positiv sein")
    return args


def main():
    args = parse_args()
    project_dir = Path(__file__).resolve().parents[1]
    output_path = args.output or project_dir / "generated" / "Logs" / (
        f"PowerMonitor_{datetime.now(timezone.utc).strftime('%Y%m%d_%H%M%S')}Z.csv")
    output_path = output_path.expanduser().resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    deadline = time.monotonic() + args.hours * 3600
    previous = {}
    sample_count = 0
    started = datetime.now(timezone.utc).isoformat(timespec="seconds")

    print(f"Read-only A8T power-meter monitor started: {started}", flush=True)
    print(f"Interval: {args.interval:g} s; duration: {args.hours:g} h", flush=True)
    print(f"CSV: {output_path}", flush=True)
    print("Devices: " + ", ".join(DEVICES), flush=True)

    with output_path.open("x", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=FIELDS, delimiter=";")
        writer.writeheader()
        output.flush()
        try:
            while time.monotonic() < deadline:
                round_start = time.monotonic()
                rows = {}
                with ThreadPoolExecutor(max_workers=len(DEVICES)) as executor:
                    futures = {
                        executor.submit(read_device, device_id, base_url, args.timeout): device_id
                        for device_id, base_url in DEVICES.items()
                    }
                    for future in as_completed(futures):
                        device_id = futures[future]
                        try:
                            rows[device_id] = future.result()
                        except (OSError, ValueError, KeyError, TypeError, URLError) as error:
                            rows[device_id] = {
                                "TimestampUtc": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
                                "DeviceId": device_id,
                                "Error": str(error),
                            }

                for device_id in DEVICES:
                    row = rows[device_id]
                    old = previous.get(device_id, {})
                    for field, delta_field in (
                        ("CfEdges", "CfDelta"),
                        ("Cf1Edges", "Cf1Delta"),
                        ("VoltageIntervals", "VoltageIntervalsDelta"),
                        ("CurrentIntervals", "CurrentIntervalsDelta"),
                    ):
                        current = row.get(field)
                        prior = old.get(field)
                        if isinstance(current, int) and isinstance(prior, int):
                            row[delta_field] = current - prior
                        else:
                            row[delta_field] = ""
                    if not row.get("Error"):
                        previous[device_id] = row
                    writer.writerow({field: row.get(field, "") for field in FIELDS})

                output.flush()
                sample_count += 1
                if sample_count == 1 or sample_count % max(1, round(30 / args.interval)) == 0:
                    details = []
                    for device_id, row in rows.items():
                        if row.get("Error"):
                            details.append(f"{device_id}=OFFLINE")
                        else:
                            details.append(
                                f"{device_id} relay={row.get('RelayOn')} "
                                f"U={row.get('Voltage')} I={row.get('Current')} "
                                f"W={row.get('ActivePower')} CFΔ={row.get('CfDelta', '')} "
                                f"CF1Δ={row.get('Cf1Delta', '')}")
                    print(f"samples={sample_count}: " + " | ".join(details), flush=True)

                remaining = args.interval - (time.monotonic() - round_start)
                if remaining > 0:
                    time.sleep(min(remaining, max(0, deadline - time.monotonic())))
        except KeyboardInterrupt:
            print("Interrupted; closing the CSV cleanly.", flush=True)

    print(f"Monitor stopped after {sample_count} rounds. CSV: {output_path}", flush=True)


if __name__ == "__main__":
    main()
