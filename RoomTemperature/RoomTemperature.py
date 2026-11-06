#!/usr/bin/env python3
# RoomTemperature.py, Version: 1.02
"""Standalone DS18B20 reader and aquarium protocol v3 UDP publisher."""
import argparse
import json
from Messages import ReadJson, NewStatus, WriteMessage, BroadcastAddress, UdpPort, SendIntervalSeconds
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
SYSFS_DIRECTORY = '/sys/bus/w1/devices'
MIN_CELSIUS = -20
MAX_CELSIUS = 60


def load_config(path):
    # Hardware selection is local, not a remotely exchanged message.
    config = json.loads(Path(path).read_text())
    if not isinstance(config, dict) or not re.fullmatch(r'(?:[0-9A-F]{2}:){5}[0-9A-F]{2}@3', str(config.get('DeviceId', ''))) or config['DeviceId'].startswith('00:00:00:00:00:00'):
        raise ValueError('Invalid room service address')
    sensor = config.get('SensorId')
    if sensor is not None and (not isinstance(sensor,str) or not re.fullmatch(r'28-[0-9a-fA-F]{12}',sensor)):
        raise ValueError('Invalid local sensor selection')
    return dict(DeviceId=config['DeviceId'], SensorId=sensor,
                IntervalSeconds=SendIntervalSeconds, BroadcastAddress=BroadcastAddress,
                Port=UdpPort, SysfsDirectory=SYSFS_DIRECTORY,
                MinCelsius=MIN_CELSIUS, MaxCelsius=MAX_CELSIUS)


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
    status = {'Kind': 'RoomTemperature', 'Temp': value,
              'AppVer': version, 'CurrentTime': timestamp, 'Healthy': error is None,
              'Errs': [] if error is None else [error],
              'Meas': [{'Kind': 'Temperature', 'Name': 'Raumtemperatur',
                                'Value': value, 'Unit': '°C', 'Decimals': 1,
                                'Valid': error is None, 'UpdatedAt': timestamp,
                                'Err': error}]}
    return NewStatus(ID=config['DeviceId'], Status=status)


def request_matches(raw, device_id):
    try:
        packet = ReadJson('TUdpRequestMessage', raw)
    except (ValueError, UnicodeError):
        return False
    return isinstance(packet['ID'],list) and (not packet['ID'] or device_id in packet['ID'])


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
                error = tuple(packet['Status']['Errs'])
                if error != last_error:
                    if error:
                        LOG.error('%s', '; '.join(error))
                    else:
                        LOG.info('Local sensor %s valid', config.get('SensorId') or '(automatic selection)')
                    last_error = error
                try:
                    udp.sendto(WriteMessage(packet).encode(),
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
