#!/usr/bin/env python3
# RoomTemperature.py, Version: 1.00
"""Standalone DS18B20 reader and aquarium protocol v2 UDP publisher."""
import argparse
import json
import logging
import math
from pathlib import Path
import re
import select
import signal
import socket
import time

ROOT = Path(__file__).resolve().parent
LOG = logging.getLogger('RoomTemperature')


def load_config(path):
    config = json.loads(Path(path).read_text())
    if not isinstance(config, dict):
        raise ValueError('Configuration must be a JSON object')
    if not re.fullmatch(r'(?:[0-9A-F]{2}:){5}[0-9A-F]{2}@3', config.get('DeviceId', '')):
        raise ValueError('DeviceId must be the fixed uppercase MAC@3 identity')
    if config['DeviceId'].startswith('00:00:00:00:00:00'):
        raise ValueError('DeviceId cannot be a dummy identity')
    sensor = config.get('SensorId')
    if sensor is not None and not re.fullmatch(r'28-[0-9a-fA-F]{12}', sensor):
        raise ValueError('SensorId must be null or a DS18B20 id: 28-xxxxxxxxxxxx')
    for key in ('IntervalSeconds', 'MinCelsius', 'MaxCelsius'):
        value = config.get(key)
        if type(value) not in (int, float) or not math.isfinite(value):
            raise ValueError(f'{key} must be a finite number')
    if not 1 <= config['IntervalSeconds'] <= 10:
        raise ValueError('IntervalSeconds must be between 1 and 10 (receiver expiry: 20 s)')
    if not -55 <= config['MinCelsius'] < config['MaxCelsius'] <= 125:
        raise ValueError('Invalid sensor temperature range')
    socket.inet_aton(config['BroadcastAddress'])
    if type(config.get('Port')) is not int or not 1 <= config['Port'] <= 65535:
        raise ValueError('Invalid UDP port')
    if not isinstance(config.get('SysfsDirectory'), str):
        raise ValueError('SysfsDirectory must be a path')
    return config


def sensor_ids(config):
    return sorted(p.name for p in Path(config['SysfsDirectory']).glob('28-*')
                  if re.fullmatch(r'28-[0-9a-fA-F]{12}', p.name))


def read_temperature(config):
    sensor = config.get('SensorId')
    if sensor is None:
        found = sensor_ids(config)
        if len(found) != 1:
            raise ValueError(f'Expected exactly one DS18B20, found {len(found)}; set SensorId if multiple')
        sensor = found[0]
    lines = (Path(config['SysfsDirectory']) / sensor / 'w1_slave').read_text().splitlines()
    # w1_slave can retain old temperature bytes after CRC failure: require YES.
    if len(lines) != 2 or not lines[0].strip().endswith('YES'):
        raise ValueError('DS18B20 CRC/read error')
    match = re.search(r'\bt=(-?\d+)\s*$', lines[1])
    if not match:
        raise ValueError('DS18B20 temperature missing')
    value = int(match[1]) / 1000.0
    if not config['MinCelsius'] <= value <= config['MaxCelsius']:
        raise ValueError(f'Temperature outside configured room range: {value:g} C')
    return value, sensor


def app_version():
    values = dict(line.split('=', 1) for line in (ROOT / 'Versions.txt').read_text().splitlines()
                  if line and not line.startswith('#'))
    return values['App']


def build_packet(config, version, now=None):
    error = None
    sensor = config.get('SensorId')
    try:
        value, sensor = read_temperature(config)
    except (OSError, ValueError) as exc:
        value = None
        error = str(exc)
    timestamp = int(time.time() if now is None else now)
    status = {'Kind': 'RoomTemperature', 'Temperature': value, 'SensorId': sensor,
              'AppVersion': version, 'CurrentTime': timestamp, 'Healthy': error is None,
              'Errors': [] if error is None else [error],
              'Measurements': [{'Id': 'Temperature', 'Name': 'Raumtemperatur',
                                'Value': value, 'Unit': '°C', 'Decimals': 1,
                                'Valid': error is None, 'UpdatedAt': timestamp,
                                'Error': error}]}
    return {'Version': 2, 'Type': 'Status', 'ID': config['DeviceId'], 'Status': status}


def request_matches(raw, device_id):
    try:
        packet = json.loads(raw)
    except (ValueError, UnicodeError):
        return False
    if not isinstance(packet, dict):
        return False
    return (type(packet.get('Version')) is int and packet['Version'] == 2
            and packet.get('Type') == 'Request' and isinstance(packet.get('ID'), list)
            and all(isinstance(item, str) for item in packet['ID'])
            and (not packet['ID'] or device_id in packet['ID']))


def run(config, version):
    running = True

    def stop(_signum, _frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    last_error = object()
    next_sample = 0.0
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
        udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        udp.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        udp.bind(('', config['Port']))
        udp.setblocking(False)
        LOG.info('Started %s, app %s', config['DeviceId'], version)
        while running:
            now = time.monotonic()
            if now >= next_sample:
                packet = build_packet(config, version)
                error = tuple(packet['Status']['Errors'])
                if error != last_error:
                    if error:
                        LOG.error('%s', '; '.join(error))
                    else:
                        LOG.info('Sensor %s valid', packet['Status']['SensorId'])
                    last_error = error
                try:
                    udp.sendto(json.dumps(packet, ensure_ascii=False, allow_nan=False).encode(),
                               (config['BroadcastAddress'], config['Port']))
                except OSError as exc:
                    LOG.warning('UDP send failed: %s', exc)
                next_sample = time.monotonic() + config['IntervalSeconds']
            ready, _, _ = select.select([udp], [], [], min(1.0, max(0, next_sample - time.monotonic())))
            if ready:
                try:
                    raw, _ = udp.recvfrom(65535)
                except BlockingIOError:
                    continue
                # Fresh read on request; rate-limit requests without replaying stale values.
                if request_matches(raw, config['DeviceId']) and next_sample - time.monotonic() <= config['IntervalSeconds'] - 1:
                    next_sample = 0.0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=ROOT / 'data/Config.json')
    parser.add_argument('--list', action='store_true', help='List sensors without sending UDP')
    parser.add_argument('--once', action='store_true', help='Print one status, no UDP; exit 2 if invalid')
    args = parser.parse_args()
    try:
        config = load_config(args.config)
        if args.list:
            print(json.dumps(sensor_ids(config)))
            return 0
        version = app_version()
        if args.once:
            packet = build_packet(config, version)
            print(json.dumps(packet, ensure_ascii=False, allow_nan=False, indent=2))
            return 0 if packet['Status']['Healthy'] else 2
        run(config, version)
        return 0
    except (OSError, ValueError, KeyError) as exc:
        LOG.error('%s', exc)
        return 1


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(name)s: %(message)s')
    raise SystemExit(main())
