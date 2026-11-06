# test_recording_events.py, Version: 1.00
"""Loopback-only configuration snapshot, targeted patch and reconnect test."""
import copy,select,sys,time
from pathlib import Path
from test_assignment_control import Fixture
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'HeatHistory'))
from HeatHistory import DashboardEvents

fixture=Fixture();events=None
try:
 profiles=fixture.request('/api/heatprofiles')['Profiles']
 second={**copy.deepcopy(profiles[0]),'Id':'HeatProfileAq2','TempId':'FeederAq2'}
 assert fixture.request('/api/heatprofiles',{'Profiles':profiles+[second]},'PUT')['Ok']
 events=DashboardEvents(fixture.base);events.Start()
 def receive_snapshot_or_patch(snapshot=False):
  deadline=time.monotonic()+12
  while time.monotonic()<deadline:
   if not select.select([events.reader],[],[],max(0,deadline-time.monotonic()))[0]:break
   for event in events.Read():
    if 'ConnectionError' not in event and (not snapshot or event.get('Snapshot')):return event
  raise AssertionError('No configuration event received')
 first=receive_snapshot_or_patch(True)
 assert set(first['Profiles'])=={'HeatProfileAq1','HeatProfileAq2'},first
 assert first['Channels'] and first['Registry']['Mode']=='MAC'
 changed=copy.deepcopy(profiles[0]);changed['MainOn']-=0.1
 assert fixture.request('/api/heatprofiles',{'Profiles':[changed,second]},'PUT')['Ok']
 patch=receive_snapshot_or_patch()
 assert patch['Snapshot'] is False and set(patch['Profiles'])=={'HeatProfileAq1'},patch
 assert patch['Profiles']['HeatProfileAq1']['MainOn']==changed['MainOn']
 # No periodic configuration messages while the configuration is unchanged.
 assert not select.select([events.reader],[],[],1)[0]
 fixture.stop();fixture.start()
 resumed=receive_snapshot_or_patch(True)
 assert set(resumed['Profiles'])=={'HeatProfileAq1','HeatProfileAq2'}
 assert resumed['Profiles']['HeatProfileAq1']['MainOn']==changed['MainOn']
 print('Recording SSE: complete snapshot, targeted profile patch, quiet idle and full reconnect passed')
finally:
 if events:events.Close()
 fixture.close()
