# test_room_temperature_udp.py, Version: 1.00
"""Real publisher process over loopback broadcast, using simulated sysfs only."""
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UdpTest(unittest.TestCase):
    def test_publish_request_failure_and_recovery(self):
        with tempfile.TemporaryDirectory(prefix='room-udp-test-') as tmp:
            path = Path(tmp)
            sensor = path / '28-0123456789ab'
            sensor.mkdir()
            reading = sensor / 'w1_slave'

            def write(value, crc='YES'):
                reading.write_text(f'00 : crc=00 {crc}\n00 t={value}\n')

            write(22375)
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
                udp.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                udp.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
                udp.bind(('', 0))
                config = json.loads((ROOT / 'data/Config.json').read_text())
                config.update(SysfsDirectory=str(path), IntervalSeconds=10,
                              BroadcastAddress='127.255.255.255', Port=udp.getsockname()[1])
                cfg = path / 'Config.json'
                cfg.write_text(json.dumps(config))
                launcher = path / 'RunSender.py'
                launcher.write_text('import sys\nsys.path.insert(0,'+repr(str(ROOT))+')\nimport RoomTemperature as S\nS.SYSFS_DIRECTORY='+repr(str(path))+'\nS.BroadcastAddress="127.255.255.255"\nS.UdpPort='+str(udp.getsockname()[1])+'\nraise SystemExit(S.main())\n')
                process = subprocess.Popen([sys.executable, str(launcher),
                                            '--config', str(cfg)], stdout=subprocess.PIPE,
                                           stderr=subprocess.PIPE, text=True)

                def receive(timeout=5):
                    deadline = time.monotonic() + timeout
                    while time.monotonic() < deadline:
                        udp.settimeout(max(.01, deadline - time.monotonic()))
                        try:
                            packet = json.loads(udp.recvfrom(65535)[0])
                        except socket.timeout:
                            break
                        if packet.get('Type') == 2 and packet.get('ID') == config['DeviceId']:
                            return packet
                    self.fail('No status received from actual sender process')

                def request():
                    # Avoid the deliberate one-second request rate limit.
                    time.sleep(1.1)
                    packet = {'Ver': 3, 'Type': 1, 'ID': [config['DeviceId']]}
                    udp.sendto(json.dumps(packet).encode(), ('127.255.255.255', config['Port']))

                try:
                    packet = receive()
                    self.assertEqual(packet['Ver'], 3)
                    self.assertEqual(packet['Status']['Temp'], 22.375)
                    write(24000, 'NO')
                    request()
                    packet = receive(timeout=4)
                    self.assertIsNone(packet['Status']['Temp'])
                    self.assertFalse(packet['Status']['Healthy'])
                    write(24000)
                    request()
                    packet = receive(timeout=4)
                    self.assertEqual(packet['Status']['Temp'], 24)
                    self.assertTrue(packet['Status']['Healthy'])
                finally:
                    process.terminate()
                    try:
                        stdout, stderr = process.communicate(timeout=4)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.communicate()
                        self.fail('Sender did not stop on SIGTERM')
                    self.assertEqual(process.returncode, 0, stdout + stderr)


if __name__ == '__main__':
    unittest.main()
