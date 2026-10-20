#!/usr/bin/env python3
# CalibrateA8T.py, Version: 1.02
"""Compare a Shelly reference with one NOUS A8T and optionally calibrate the A8T."""

import argparse
import csv
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import statistics
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit, urlunsplit
from urllib.request import Request, urlopen


EXPECTED_SHELLY_TYPE = "TmrSwShellyC3V1"
EXPECTED_NOUS_TYPE = "TmrSwA8T"
CSV_FIELDS = (
    "TimestampUtc", "Round", "PairGapMs", "ShellyValid", "ShellyState",
    "ShellyVoltage", "ShellyCurrent", "ShellyPower", "NousValid", "NousState",
    "NousVoltage", "NousCurrent", "NousPower", "Accepted", "Note",
)
METRICS = ("Voltage", "Current", "Power")


class DeviceUnavailable(RuntimeError):
    pass


class Device:
    def __init__(self, url, label):
        parts = urlsplit(url if "://" in url else f"http://{url}")
        if parts.scheme not in ("http", "https") or not parts.hostname or parts.username or parts.password:
            raise ValueError(f"Ungültige Geräteadresse für {label}: {url}")
        self.base = urlunsplit((parts.scheme, parts.netloc, parts.path.rstrip("/"), "", ""))
        self.label = label

    def request(self, path, payload=None, timeout=5):
        data = None if payload is None else json.dumps(payload).encode("utf-8")
        headers = {} if data is None else {"Content-Type": "application/json; charset=UTF-8"}
        request = Request(self.base + path, data=data, headers=headers, method="GET" if data is None else "POST")
        try:
            with urlopen(request, timeout=timeout) as response:
                body = response.read().decode("utf-8", errors="replace")
                if response.status < 200 or response.status >= 300:
                    raise RuntimeError(f"{self.label}: HTTP {response.status}: {body.strip()}")
                return body
        except HTTPError as error:
            body = error.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"{self.label}: HTTP {error.code}: {body.strip()}") from error
        except (URLError, TimeoutError, OSError) as error:
            raise DeviceUnavailable(f"{self.label} ({self.base}) nicht erreichbar: {error}") from error

    def json(self, path):
        try:
            return json.loads(self.request(path))
        except json.JSONDecodeError as error:
            raise RuntimeError(f"{self.label}: ungültiges JSON von {path}") from error

    def status(self):
        return self.json("/api/v1/status")

    def set_override(self, active, state=True):
        body = self.request("/api/v1/calibration/override", {"Active": active, "State": state})
        if body.strip() != "1":
            raise RuntimeError(f"{self.label}: Relais-Override abgewiesen: {body.strip()}")


def device_meter(status, label):
    channels = status.get("Channels")
    meter = status.get("PowerMeter")
    if not isinstance(channels, list) or not channels or not isinstance(meter, dict):
        raise RuntimeError(f"{label}: Status enthält keinen Kanal oder Leistungsmesser.")
    return channels[0], meter


def verify_device(device, expected_type):
    info = device.json("/api/v1/device")
    actual_type = info.get("DeviceType")
    if actual_type != expected_type:
        raise RuntimeError(f"{device.label}: erwartet DeviceType {expected_type}, erhalten {actual_type!r}.")
    status = device.status()
    channel, meter = device_meter(status, device.label)
    if channel.get("HasOutput") is not True:
        raise RuntimeError(f"{device.label}: schaltbarer Ausgang fehlt.")
    if channel.get("CalibrationOverrideActive") is True:
        raise RuntimeError(f"{device.label}: ein früherer Kalibrier-Override ist noch aktiv.")
    if not all(key in meter for key in ("Voltage", "Current", "ActivePower", "Valid")):
        raise RuntimeError(f"{device.label}: Leistungsmesser liefert unvollständige Werte.")
    return info, status


def numeric(meter, name):
    value = meter.get(name)
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        return None
    return float(value)


def blank_row(round_name, note):
    return {
        "TimestampUtc": datetime.now(timezone.utc).isoformat(timespec="milliseconds"),
        "Round": round_name, "PairGapMs": "", "ShellyValid": "", "ShellyState": "",
        "ShellyVoltage": "", "ShellyCurrent": "", "ShellyPower": "", "NousValid": "",
        "NousState": "", "NousVoltage": "", "NousCurrent": "", "NousPower": "",
        "Accepted": "false", "Note": note,
    }


def sample_round(shelly, nous, round_name, seconds, interval, min_power, writer, output):
    print(f"\n{round_name}: Messe {seconds:g} s. Beide Relais bleiben per Override EIN.")
    deadline = time.monotonic() + seconds
    ratios = {metric: [] for metric in METRICS}
    accepted = 0
    while time.monotonic() < deadline:
        cycle_start = time.monotonic()
        row = blank_row(round_name, "")
        try:
            shelly_before = time.monotonic()
            shelly_status = shelly.status()
            shelly_after = time.monotonic()
            nous_before = time.monotonic()
            nous_status = nous.status()
            nous_after = time.monotonic()
            shelly_channel, shelly_meter = device_meter(shelly_status, shelly.label)
            nous_channel, nous_meter = device_meter(nous_status, nous.label)

            row["PairGapMs"] = round(abs((shelly_before + shelly_after) / 2 -
                                             (nous_before + nous_after) / 2) * 1000, 1)
            row["ShellyValid"] = shelly_meter.get("Valid") is True
            row["ShellyState"] = shelly_channel.get("State")
            row["NousValid"] = nous_meter.get("Valid") is True
            row["NousState"] = nous_channel.get("State")
            shelly_values = {
                "Voltage": numeric(shelly_meter, "Voltage"),
                "Current": numeric(shelly_meter, "Current"),
                "Power": numeric(shelly_meter, "ActivePower"),
            }
            nous_values = {
                "Voltage": numeric(nous_meter, "Voltage"),
                "Current": numeric(nous_meter, "Current"),
                "Power": numeric(nous_meter, "ActivePower"),
            }
            for key, metric in (("ShellyVoltage", "Voltage"), ("ShellyCurrent", "Current"),
                                ("ShellyPower", "Power"), ("NousVoltage", "Voltage"),
                                ("NousCurrent", "Current"), ("NousPower", "Power")):
                row[key] = shelly_values[metric] if key.startswith("Shelly") else nous_values[metric]

            if (shelly_channel.get("CalibrationOverrideActive") is not True or
                    nous_channel.get("CalibrationOverrideActive") is not True or
                    shelly_channel.get("State") is not True or nous_channel.get("State") is not True):
                raise RuntimeError("Relais-Override wurde beendet oder ein Ausgang ist AUS.")
            if row["PairGapMs"] > 2500:
                raise RuntimeError("Messwerte liegen zeitlich zu weit auseinander.")
            if shelly_meter.get("Valid") is not True or nous_meter.get("Valid") is not True:
                raise RuntimeError("Mindestens ein Leistungsmesser meldet ungültige Werte.")
            if any(shelly_values[key] is None or nous_values[key] is None for key in METRICS):
                raise RuntimeError("Mindestens ein Messwert ist nicht numerisch.")
            if min(shelly_values["Voltage"], nous_values["Voltage"],
                   shelly_values["Current"], nous_values["Current"]) <= 0:
                raise RuntimeError("Spannung oder Strom ist null/negativ.")
            if min(shelly_values["Power"], nous_values["Power"]) < min_power:
                raise RuntimeError(f"Last liegt unter {min_power:g} W Mindestleistung.")

            for metric in METRICS:
                ratios[metric].append(shelly_values[metric] / nous_values[metric])
            row["Accepted"] = "true"
            accepted += 1
        except Exception as error:  # Keep every failed sample visible in the CSV.
            row["Note"] = str(error)

        writer.writerow(row)
        output.flush()
        remaining = deadline - time.monotonic()
        if remaining > 0:
            time.sleep(min(interval, remaining))

    if accepted < max(5, int(seconds / 3)):
        raise RuntimeError(f"{round_name}: nur {accepted} gültige Messpaare; Test wird verworfen.")
    return {metric: statistics.median(values) for metric, values in ratios.items()}, accepted


def wait_until_online(device, timeout):
    print(f"Warte auf {device.label} …")
    deadline = time.monotonic() + timeout
    last_error = "noch keine Antwort"
    while time.monotonic() < deadline:
        try:
            status = device.status()
            channel, _ = device_meter(status, device.label)
            if channel.get("HasOutput") is not True:
                raise RuntimeError(f"{device.label}: kein schaltbarer Ausgang.")
            return status
        except DeviceUnavailable as error:
            last_error = str(error)
            time.sleep(2)
    raise RuntimeError(f"{device.label} nach {timeout:g} s nicht erreichbar: {last_error}")


def activate_override(device, override_devices):
    if device not in override_devices:
        # Register before the request so cleanup still tries to clear an
        # override if the device applies it but the HTTP response is lost.
        override_devices.append(device)
    device.set_override(True, True)


def prepare_order(upstream, downstream, upstream_type, downstream_type,
                  timeout, override_devices):
    wait_until_online(upstream, timeout)
    upstream_info, _ = verify_device(upstream, upstream_type)
    activate_override(upstream, override_devices)

    # The downstream smart plug may be unpowered while the upstream relay is
    # off. Switch on the upstream device first, then wait for the downstream one.
    wait_until_online(downstream, timeout)
    downstream_info, _ = verify_device(downstream, downstream_type)
    activate_override(downstream, override_devices)
    return upstream_info, downstream_info


def parse_args():
    parser = argparse.ArgumentParser(
        description="Erfasst zwei Kreuztausch-Messreihen; Referenz ist Shelly, kalibriert wird nur der A8T.")
    parser.add_argument("--shelly", required=True, metavar="URL", help="URL/IP des Shelly-Referenzgeräts")
    parser.add_argument("--nous", required=True, metavar="URL", help="URL/IP des zu kalibrierenden NOUS A8T")
    parser.add_argument("--seconds", type=float, default=30, help="Messdauer je Anordnung (Standard: 30 s)")
    parser.add_argument("--interval", type=float, default=1, help="Abstand der Messpaare (Standard: 1 s)")
    parser.add_argument("--min-power", type=float, default=100, help="Mindestleistung beider Geräte in W (Standard: 100 W)")
    parser.add_argument("--reconnect-timeout", type=float, default=180, help="Wartezeit nach dem Umstecken in s")
    parser.add_argument("--output", type=Path, help="CSV-Ziel; Standard: PowerCalibration_<Zeit>.csv")
    parser.add_argument("--apply", action="store_true", help="Nach ausdrücklicher Terminalbestätigung nur A8T-Werte schreiben")
    args = parser.parse_args()
    if args.seconds < 10 or args.interval <= 0 or args.min_power <= 0 or args.reconnect_timeout <= 0:
        parser.error("seconds muss mindestens 10 sein; interval, min-power und reconnect-timeout müssen positiv sein")
    return args


def main():
    args = parse_args()
    shelly = Device(args.shelly, "Shelly")
    nous = Device(args.nous, "NOUS A8T")
    output_path = args.output or Path(f"PowerCalibration_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
    output_path = output_path.expanduser().resolve()
    if output_path.exists():
        raise RuntimeError(f"CSV-Datei existiert bereits: {output_path}")

    print(f"Shelly-Referenz: {shelly.base}")
    print(f"Kalibrierziel:   {nous.base}")
    print("Es werden ausschließlich Kalibrierwerte des NOUS A8T geschrieben; der Shelly wird nur gelesen.")
    print("Beide Geräte werden während der Messungen per temporärem Override eingeschaltet.")
    pairs = []
    old_cal = None
    writer = None
    output = None
    override_devices = []
    try:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output = output_path.open("x", newline="", encoding="utf-8")
        writer = csv.DictWriter(output, fieldnames=CSV_FIELDS)
        writer.writeheader()
        print("\nMessung 1 vorbereiten: spannungsfrei aufbauen: Shelly → NOUS A8T → Heizlast.")
        print("Dann Shelly mit dem Netz verbinden. Sein Relais muss nicht manuell eingeschaltet werden;")
        print("das Skript schaltet zuerst den Shelly ein und wartet danach auf den NOUS A8T.")
        input("Wenn der Shelly mit dem Netz verbunden ist, Enter drücken … ")
        shelly_info, nous_info = prepare_order(
            shelly, nous, EXPECTED_SHELLY_TYPE, EXPECTED_NOUS_TYPE,
            args.reconnect_timeout, override_devices)
        old_cal = nous.json("/api/v1/power/calibration")
        for key in ("VoltageCal", "CurrentCal", "PowerCal"):
            value = old_cal.get(key)
            if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
                raise RuntimeError(f"NOUS A8T liefert keinen gültigen ganzzahligen {key}-Wert.")
        print(f"Shelly-Referenz: {shelly_info.get('DeviceId', '?')}")
        print(f"Kalibrierziel:   {nous_info.get('DeviceId', '?')}")
        pairs.append(sample_round(shelly, nous, "Shelly → Nous → Last", args.seconds, args.interval,
                                  args.min_power, writer, output))

        print("\nMessung 2 vorbereiten: Netz trennen, Reihenfolge tauschen: NOUS A8T → Shelly → Heizlast.")
        print("Nach dem Umstecken den NOUS mit dem Netz verbinden. Das Skript schaltet")
        print("zuerst den NOUS ein und wartet danach auf den Shelly dahinter.")
        input("Wenn der NOUS mit dem Netz verbunden ist, Enter drücken … ")
        prepare_order(nous, shelly, EXPECTED_NOUS_TYPE, EXPECTED_SHELLY_TYPE,
                      args.reconnect_timeout, override_devices)
        pairs.append(sample_round(shelly, nous, "Nous → Shelly → Last", args.seconds, args.interval,
                                  args.min_power, writer, output))
    finally:
        for device in reversed(override_devices):
            try:
                device.set_override(False, False)
            except Exception as error:
                print(f"WARNUNG: temporären Override auf {device.label} nicht lösen können: {error}", file=sys.stderr)
        if output is not None:
            output.close()

    factors = {
        metric: math.sqrt(pairs[0][0][metric] * pairs[1][0][metric])
        for metric in METRICS
    }
    new_cal = {
        "VoltageCal": round(old_cal["VoltageCal"] * factors["Voltage"]),
        "CurrentCal": round(old_cal["CurrentCal"] * factors["Current"]),
        "PowerCal": round(old_cal["PowerCal"] * factors["Power"]),
    }
    if any(value < 1 or value > 0xFFFFFFFF for value in new_cal.values()):
        raise RuntimeError(f"Berechnete A8T-Kalibrierwerte außerhalb uint32: {new_cal}")

    print(f"\nMessdaten gespeichert: {output_path}")
    print(f"Gültige Messpaare: Shelly → Nous → Last {pairs[0][1]}, Nous → Shelly → Last {pairs[1][1]}")
    print("Korrekturfaktoren Shelly/A8T (geometrisches Mittel des Kreuztauschs):")
    for metric in METRICS:
        first, second = pairs[0][0][metric], pairs[1][0][metric]
        drift = abs(first - second) / ((first + second) / 2) * 100
        print(f"  {metric}: {factors[metric]:.8f} (Positionsabweichung {drift:.3f} %)")
    print("\nA8T-Kalibrierwerte, bisher → berechnet:")
    for key in ("VoltageCal", "CurrentCal", "PowerCal"):
        print(f"  {key}: {old_cal[key]} → {new_cal[key]}")
    print("Shelly-Kalibrierwerte wurden nicht verändert.")

    if args.apply:
        confirmation = input("\nZum Schreiben NUR auf den A8T exakt 'A8T SCHREIBEN' eingeben: ")
        if confirmation != "A8T SCHREIBEN":
            print("Keine Kalibrierwerte geschrieben.")
            return
        result = nous.request("/api/v1/power/calibration", new_cal)
        if result.strip() != "1":
            raise RuntimeError(f"A8T hat das Speichern abgewiesen: {result.strip()}")
        verified = nous.json("/api/v1/power/calibration")
        if any(verified.get(key) != value for key, value in new_cal.items()):
            raise RuntimeError(f"A8T-Kalibrierwerte stimmen nach dem Schreiben nicht: {verified}")
        print("A8T-Kalibrierwerte geschrieben und zurückgelesen geprüft.")
    else:
        print("Nur berechnet, noch nicht geschrieben. Mit --apply und Bestätigung auf den A8T übertragen.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, ValueError, OSError, KeyboardInterrupt) as error:
        print(f"FEHLER: {error}", file=sys.stderr)
        sys.exit(1)
