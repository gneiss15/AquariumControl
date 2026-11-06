#!/usr/bin/env python3
# test_charge_profiles.py, Version: 1.02
"""Loopback: conditional fields, extensibility, persistence and learned-factor sync."""
import copy
import json
import urllib.error
from test_assignment_control import Fixture

f = Fixture()
try:
    profiles = f.request('/api/heatprofiles')['Profiles']
    charge = dict(profiles[0], Measurement='BatteryPercent', FloorOn=40, FloorOff=80,
                  MainOn=40, MainOff=80, NoTempOn=False, ChargeRunOn=True,
                  ChargeSecondsPerPercent=15, Future={'nested': [1, True, 'keep']})
    assert f.request('/api/heatprofiles', {'Profiles': [charge]}, 'PUT')['Ok']
    current = f.request('/api/heatprofiles')
    revision = current['TimeStamp']
    assert current['Profiles'][0] == charge
    assert json.loads((f.root/'data/HeatProfiles.json').read_text())['Profiles'][0] == charge
    for changes in ({'ChargeSecondsPerPercent': None}, {'ChargeSecondsPerPercent': 0},
                    {'ChargeSecondsPerPercent': True}, {'ChargeSecondsPerPercent': 15.5}, {'ChargeSecondsPerPercent': -1}, {'ChargeSecondsPerPercent': 4294967296}, {'ChargeRunOn': 'yes'}, {'Measurement': 'Temperature'}):
        invalid = dict(charge, **changes)
        try:
            f.request('/api/heatprofiles', {'Profiles': [invalid]}, 'PUT')
            raise AssertionError('Invalid active profile accepted')
        except urllib.error.HTTPError as error:
            assert error.code == 400
    missing = dict(charge); missing.pop('ChargeSecondsPerPercent')
    try:
        f.request('/api/heatprofiles', {'Profiles': [missing]}, 'PUT')
        raise AssertionError('Active rule without factor accepted')
    except urllib.error.HTTPError as error:
        assert error.code == 400
    # Simulate the existing device-to-Dashboard sync after local learning.
    device = f.devices[0]
    learned = dict(charge, ChargeSecondsPerPercent=24)
    payload = {'Device': device.identity, 'Type': 'SwitchTimes', 'Format': 'TmrSw',
               'TimeStamp': '2099-01-01T00:00:00.000Z',
               'Content': {'SwitchTimes': [], 'Intervals': [],
                           'Channels': {'Profiles': [learned], 'Channels': [{'Id': 'MainAq1', 'Role': 'MainSw', 'Profile': charge['Id']}]}},
               'Identity': {'Version': 3, 'DeviceId': device.identity, 'Mode': 'MAC',
                            'Function': device.function, 'AppliedRevision': device.applied}}
    f.request('/api/times/sync', payload)
    updated = f.request('/api/heatprofiles')
    assert updated['Profiles'][0]['ChargeSecondsPerPercent'] == 24, updated
    assert updated['Profiles'][0]['Future'] == charge['Future']
    try:
        f.request('/api/heatprofiles', {'Profiles': [charge], 'TimeStamp': revision}, 'PUT')
        raise AssertionError('Stale editor overwrote the learned factor')
    except urllib.error.HTTPError as error:
        assert error.code == 409
    f.stop(); f.start()
    assert f.request('/api/heatprofiles')['Profiles'][0] == learned
    print('Charge profiles: conditional validation, extra fields, learned-factor synchronization, stale-editor conflict and restart persistence passed.')
finally:
    f.close()
