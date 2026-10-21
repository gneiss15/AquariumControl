# test_udp_v2.py, Version: 1.02
# Isolated loopback server: profile-field preservation and version-2-only UDP reception.
import json, socket, time, urllib.error
from test_assignment_control import Fixture
import contextlib, io, pathlib, runpy
validate = runpy.run_path(str(pathlib.Path(__file__).resolve().parents[1]/'tools/ValidateHeatProfiles.py'))['ValidateProfile']
row = {'Id':'Test','TempId':'Source','FloorOn':0,'FloorOff':1,'MainOn':0,'MainOff':1,'NoTempOn':False}
for field in ('Temperature', 'BatteryPercent'):
 assert validate({**row, 'Measurement':field}, 0) == 'Test'
for field in ('', 'Status.Temperature', 'Kind', None, True):
 with contextlib.redirect_stderr(io.StringIO()):
  try: validate({**row, 'Measurement':field}, 0); raise AssertionError('Transfer validator accepted invalid field')
  except SystemExit as e: assert e.code == 1
f = Fixture()
try:
 f.stop()
 path = f.root/'data/HeatProfiles.json'
 document = json.loads(path.read_text()); document.pop('Version', None)
 path.write_text(json.dumps(document)); f.start()
 original = f.request('/api/heatprofiles')['Profiles']
 selected = json.loads(json.dumps(original))
 selected[0]['Measurement'] = 'BatteryPercent'
 assert f.request('/api/heatprofiles', {'Profiles': selected}, 'PUT')['Ok']
 assert f.request('/api/heatprofiles')['Profiles'] == selected
 assert json.loads((f.root/'data/HeatProfiles.json').read_text())['Profiles'] == selected
 for value in ('', 'Status.Temperature', 'Kind', None, True):
  invalid = json.loads(json.dumps(selected)); invalid[0]['Measurement'] = value
  try:
   f.request('/api/heatprofiles', {'Profiles': invalid}, 'PUT')
   raise AssertionError('Invalid Measurement accepted')
  except urllib.error.HTTPError as e: assert e.code == 400
 assert f.request('/api/heatprofiles')['Profiles'] == selected
 device = f.devices[0]
 with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp:
  for version in (2,):
   status = {'Marker': version, 'Channels': []}
   packet = {'Version': version, 'Type': 'Status', 'ID': device.identity, 'Status': status}
   for _ in range(20):
    udp.sendto(json.dumps(packet).encode(), ('127.0.0.1', 44444))
    time.sleep(.05)
    received = f.request('/api/device-status/' + device.function)
    if received.get('Status', {}).get('Marker') == version: break
   else: raise AssertionError('UDP version not received: ' + str(version))
  for unsupported in (1, 3):
   udp.sendto(json.dumps({**packet, 'Version': unsupported, 'Status': {'Marker': unsupported}}).encode(), ('127.0.0.1', 44444))
   time.sleep(.1)
   assert f.request('/api/device-status/' + device.function)['Status']['Marker'] == 2
 print('Profile Measurement persisted and validated; UDP v2 accepted, v1/v3 rejected.')
finally:
 f.close()
