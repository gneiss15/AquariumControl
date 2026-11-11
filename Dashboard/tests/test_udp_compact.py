# test_udp_compact.py, Version: 1.01
"""Loopback metadata queries, unicast replies and source lookup, no real devices."""
import json
import socket
import time
from test_assignment_control import Fixture

fixture = Fixture()
# SourceInfo identifies the Dashboard service just like the production registry.
fixture.stop()
path = fixture.root / 'data/DeviceAssignments.json'
registry = json.loads(path.read_text())
registry['Devices'].append({'MAC':'02:55:66:77:88:99','IP':'127.0.0.1',
  'Function':'Dashboard','ServiceId':2,'HttpPort':fixture.port,'DeviceId':'02:55:66:77:88:99@2',
  'Group':'System','DeviceType':'Dashboard'})
path.write_text(json.dumps(registry))
fixture.start()
try:
  identity = fixture.devices[0].identity
  revision = '0123456789abcdef'
  parts = {'Root': {'OnlineSince': 100, 'Errors': [], 'LastError': None},
           'Identity': {'Version': 3, 'Mode': 'MAC', 'Function': fixture.devices[0].function},
           'Channel:0': {'Name': 'HeaterMainAq1', 'HardwareChannel': 0, 'HasOutput': True, 'HasInput': True, 'NextChange': None, 'Error': None}}
  status = dict(Version=2, Type='Status', ID=identity, Compact=True, InfoRevision=revision,
                Status=dict(CurrentTime=123, Healthy=True, BootCounter=7, Channels=[dict(Id=0, State=True)]))
  with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
    sender.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sender.bind(('127.0.0.2', 44504))
    sender.settimeout(4)
    sender.sendto(json.dumps(status).encode(), ('127.0.0.1', 44444))
    while True:
      data, target = sender.recvfrom(4096)
      query = json.loads(data)
      if query.get('Type') == 'InfoRequest': break
    assert target[1] != 44444, 'description request needs a dedicated reply socket'
    assert query['ID'] == identity and set(query['Fields']) == set(parts)
    for field, value in parts.items():
      reply = dict(Version=2, Type='Info', ID=identity, InfoRevision=revision, Field=field,
                   Data=json.dumps(value))
      sender.sendto(json.dumps(reply).encode(), target)
    time.sleep(.8)
    received = fixture.request('/api/device-status/' + fixture.devices[0].function)
    assert received['Status']['Channels'][0]['Name'] == 'HeaterMainAq1'
    assert received['AgeMilliseconds'] >= 700, 'resolved pending status must retain its original reception time'
    sender.sendto(json.dumps(status).encode(), ('127.0.0.1', 44444))
    time.sleep(.8)
    received = fixture.request('/api/device-status/' + fixture.devices[0].function)
    assert received['Online'] and received['Status']['Channels'][0]['Name'] == 'HeaterMainAq1'
    assert received['Status']['Identity']['BootCounter'] == 7
    old_revision = received['Revision']
    time.sleep(.1)
    sender.sendto(json.dumps(reply).encode(), target)
    time.sleep(.6)
    received = fixture.request('/api/device-status/' + fixture.devices[0].function)
    assert received['Revision'] == old_revision
  with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as query:
    query.bind(('127.0.0.1', 0)); query.settimeout(4)
    query.sendto(json.dumps(dict(Version=2, Type='SourceRequest', Function='FeederAq1')).encode(), ('127.0.0.1', 44444))
    source, _ = query.recvfrom(4096); source = json.loads(source)
    assert source['Type'] == 'SourceInfo' and source['Confirmed'] is True
    assert source['Source'] == fixture.devices[2].identity
  print('Dashboard: targeted metadata reconstruction, dedicated reply socket, no metadata heartbeat and confirmed source lookup passed.')
finally:
  fixture.close()
