# test_room_temperature.py, Version: 1.01
import importlib.util
import json
import sys
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
spec = importlib.util.spec_from_file_location('room_sender', ROOT / 'RoomTemperature.py')
sender = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sender)


class RoomTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='room-temperature-test-')
        self.root = Path(self.temp.name)
        self.config = json.loads((ROOT / 'data/Config.json').read_text())
        self.config['SysfsDirectory'] = str(self.root)
        self.sensor = self.root / '28-0123456789ab'
        self.sensor.mkdir()
        self.write_sensor()

    def tearDown(self):
        self.temp.cleanup()

    def write_sensor(self, value=22375, crc='YES'):
        (self.sensor / 'w1_slave').write_text(f'00 00 00 00 00 00 00 00 00 : crc=00 {crc}\n00 00 t={value}\n')

    def packet(self):
        return sender.build_packet(self.config, '1.00', now=123)

    def test_protocol_and_measurement(self):
        p = self.packet()
        self.assertEqual((p['Ver'], p['Type'], p['ID']), (3, 2, 'B8:27:EB:7B:96:D7@3'))
        self.assertEqual(p['Status']['Temp'], 22.375)
        self.assertTrue(p['Status']['Healthy'])
        self.assertNotIn('Value', p)
        self.assertNotIn('Temperature', p)
        self.assertEqual(p['Status']['Meas'][0]['Value'], 22.375)
        json.dumps(p, allow_nan=False)

    def test_crc_cannot_reuse_previous_value(self):
        self.assertIsNotNone(self.packet()['Status']['Temp'])
        self.write_sensor(crc='NO')
        p = self.packet()['Status']
        self.assertIsNone(p['Temp'])
        self.assertFalse(p['Meas'][0]['Valid'])
        self.assertFalse(p['Healthy'])
        self.write_sensor(24000)
        self.assertEqual(self.packet()['Status']['Temp'], 24)

    def test_missing_ambiguous_and_fixed_sensor(self):
        (self.root / '28-ffffffffffff').mkdir()
        self.assertIsNone(self.packet()['Status']['Temp'])
        self.config['SensorId'] = self.sensor.name
        self.assertEqual(self.packet()['Status']['Temp'], 22.375)
        (self.sensor / 'w1_slave').unlink()
        self.assertIsNone(self.packet()['Status']['Temp'])

    def test_bad_values_and_malformed_reads(self):
        for value in (85000, -127000, 61000):
            self.write_sensor(value)
            self.assertIsNone(self.packet()['Status']['Temp'])
        (self.sensor / 'w1_slave').write_text('junk YES\nmissing value\n')
        self.assertIsNone(self.packet()['Status']['Temp'])
        self.write_sensor(0)
        self.assertEqual(self.packet()['Status']['Temp'], 0)

    def test_requests(self):
        def check(p):
            return sender.request_matches(json.dumps(p).encode(), self.config['DeviceId'])
        p = {'Ver': 3, 'Type': 1, 'ID': []}
        self.assertTrue(check(p))
        self.assertTrue(check({**p, 'ID': [self.config['DeviceId']]}))
        for altered in ({'ID': ['unknown']}, {'Ver': 1}, {'Ver': 2},
                        {'ID': self.config['DeviceId']}, {'Type': 2}, {'ID': [123]}):
            self.assertFalse(check({**p, **altered}))
        self.assertFalse(sender.request_matches(b'not json', self.config['DeviceId']))
        self.assertFalse(sender.request_matches(b'[]', self.config['DeviceId']))

    def test_config_validation(self):
        p = self.root / 'config.json'
        p.write_text(json.dumps(self.config))
        sender.load_config(p)
        for altered in ({'DeviceId': 'B8:27:EB:7B:96:D7@1'}, {'SensorId': '../bad'}):
            p.write_text(json.dumps({**self.config, **altered}))
            with self.assertRaises(ValueError):
                sender.load_config(p)


if __name__ == '__main__':
    unittest.main()
