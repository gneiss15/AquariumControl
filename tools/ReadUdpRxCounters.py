#!/usr/bin/env python3
# ReadUdpRxCounters.py, Version: 1.03
"""Take one daily snapshot of the temporary UDP receive counters."""
import argparse
from datetime import date
import json
from pathlib import Path
import sys
from urllib.request import urlopen


def read_counters(base_url):
    with urlopen(base_url.rstrip("/") + "/api/v1/udp-rx-counters", timeout=5) as response:
        counters = json.loads(response.read(256))
    if (
        not isinstance(counters, dict)
        or counters.keys() != {"InvalidContent"}
        or any(type(value) is not int or not 0 <= value < 2**32 for value in counters.values())
    ):
        raise ValueError("Unexpected counter reply")
    return counters


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dashboard", help="Dashboard base URL")
    parser.add_argument("directory", type=Path, help="Output directory for daily JSON snapshots")
    args = parser.parse_args()
    destination = args.directory / f"{date.today().isoformat()}.json"
    if destination.exists():
        return 0

    with urlopen(args.dashboard.rstrip("/") + "/api/device-registry", timeout=5) as response:
        devices = json.load(response)["Devices"]

    results = {}
    for device in devices:
        identity = device["DevId"]
        base_url = device.get("Url")
        if not base_url:
            continue
        try:
            results[identity] = read_counters(base_url)
        except (OSError, ValueError) as error:
            print(f"{device['Func']}: {error}", file=sys.stderr)

    if not results:
        return 1
    args.directory.mkdir(parents=True, exist_ok=True)
    with destination.open("x", encoding="utf-8") as output:
        json.dump(results, output, indent=2, sort_keys=True)
        output.write("\n")
    print(destination)
    return 0


if __name__ == "__main__":
    sys.exit(main())
