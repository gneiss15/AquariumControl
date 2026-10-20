#!/usr/bin/env python3
# test_endpoint_assignments.py, Version: 1.04
"""Loopback-only swaps of physical endpoints with preserved pins and role/schedule routing."""
import json
import time
from test_assignment_control import Fixture, Device


def run():
 f=Fixture()
 try:
  f.stop()
  mt=Device('GN-Pi','02:55:66:77:88:99@1','MyTimer'); f.devices.append(mt)
  mt.sources={d.function:d.identity for d in f.devices if d.identity.endswith('@3')}
  journal=json.loads((f.root/'data/DeviceAssignments.json').read_text())
  journal.pop('Endpoints',None) # Rebuild this fixture's new topology from its configured groups.
  journal['Devices'].append({'MAC':mt.identity[:17],'IP':'127.0.0.1','Function':'GN-Pi','ServiceId':1,'HttpPort':mt.port,'DeviceId':mt.identity,'DeviceType':'MyTimer','Group':'System'})
  (f.root/'data/DeviceAssignments.json').write_text(json.dumps(journal))
  config=json.loads((f.root/'Devices.json').read_text())
  config['HardwareNames']={d.identity[:17]:('MultiTimer1' if d==mt else 'Hardware'+str(i)) for i,d in enumerate(f.devices)}
  for row in config['Devices']:
   if row['Name']=='FeederAq1':row['Group']='Aq1'
   if row['Name']=='FeederAq2':row['Group']='Aq2'

  config['Devices'].append({'Name':'MultiTimer Aq1 & 2','BaseUrl':f'http://127.0.0.1:{mt.port}', 'GroupViews':[{'Group':'Aq1','Channels':['FloorAq1','LightAq1']} ]})
  (f.root/'Devices.json').write_text(json.dumps(config))
  times=json.loads((f.root/'data/Times.json').read_text())
  times['Channels']['GN-Pi']={'TimeStamp':mt.applied,'Rows':[
   {'Id':'FloorAq1','Role':'HeaterFloor','Profile':'HeatProfileAq1','PhysPos':'Main 8','InputPin':24,'OutputPin':17,'InputPullup':True},
   {'Id':'LightAq1','Role':'Timer','PhysPos':'Main 4','InputPin':11,'OutputPin':16,'InputPullup':True}]}
  times['Sets'].append({'Device':'GN-Pi','Type':'SwitchTimes','Format':'MyTimer','TimeStamp':mt.applied,'Synced':True})
  times['Times'] += [
   {'Id':'light-on','Device':'GN-Pi','Type':'SwitchTimes','Kind':'SwitchTime','Data':['10:00',255,'LightAq1',True]},
   {'Id':'light-off','Device':'GN-Pi','Type':'SwitchTimes','Kind':'SwitchTime','Data':['22:00',255,'LightAq1',False]},
   {'Id':'interval','Device':'GN-Pi','Type':'SwitchTimes','Kind':'Interval','Data':['10:00','01:00','00:10',255,'LightAq1']},
   {'Id':'floor-on','Device':'GN-Pi','Type':'SwitchTimes','Kind':'SwitchTime','Data':['08:00',255,'FloorAq1',True]},
   {'Id':'main-on','Device':'HeaterMainAq1','Type':'SwitchTimes','Kind':'SwitchTime','Data':['09:00',255,1]}]
  (f.root/'data/Times.json').write_text(json.dumps(times));f.start()
  view=f.request('/api/device-assignments');assert view['AllApplied'],view
  raw=(f.root/'data/DeviceAssignments.json').read_text()
  assert '\\/' not in raw, 'Optional JSON slash escapes should not be emitted'
  persisted=json.loads(raw)
  assert persisted['Version']==2 and 'State' not in persisted
  assert persisted['Endpoints']==[{k:v for k,v in row.items() if k!='Available'} for row in view['Endpoints']] and 'PendingOperation' not in persisted
  assert persisted['Revision']==journal['Revision']
  slots={r['Function']:r['EndpointId'] for r in view['Endpoints']}
  assert slots['FloorAq1'].endswith('/8') and slots['LightAq1'].endswith('/4')
  f.reject('SwapEndpoints',First=slots['FloorAq1'],Second=slots['FeederAq1'],ConsumersDetached=True)
  f.devices[0].endpoint_version=0
  f.reject('SwapEndpoints',First=slots['FloorAq1'],Second=slots['MainAq1'],ConsumersDetached=True)
  f.devices[0].endpoint_version=1
  f.operation('SwapEndpoints',First=slots['FloorAq1'],Second=slots['MainAq1'],ConsumersDetached=True);f.complete()
  current=json.loads((f.root/'data/Times.json').read_text());rows=current['Channels']
  assert rows['GN-Pi']['Rows'][0]['Id']=='MainAq1' and rows['GN-Pi']['Rows'][0]['Role']=='HeaterMain'
  assert rows['GN-Pi']['Rows'][0]['PhysPos']=='Main 8' and rows['GN-Pi']['Rows'][0]['OutputPin']==17
  assert rows['HeaterMainAq1']['Rows'][0]['Id']=='FloorAq1' and rows['HeaterMainAq1']['Rows'][0]['Role']=='HeaterFloor'
  migrated={r['Id']:r for r in current['Times']}
  assert migrated['floor-on']['Device']=='HeaterMainAq1' and migrated['floor-on']['Data']==['08:00',255,1]
  assert migrated['main-on']['Device']=='GN-Pi' and migrated['main-on']['Data']==['09:00',255,'MainAq1',True]
  assert not any('FunctionConfig' in r for d in f.devices for r in d.received), 'Endpoint swaps must not transfer credentials'
  # Same-container swap keeps array order and moves the two schedule groups.
  view=f.request('/api/device-assignments');slots={r['Function']:r['EndpointId'] for r in view['Endpoints']}
  f.operation('SwapEndpoints',First=slots['MainAq1'],Second=slots['LightAq1'],ConsumersDetached=True);f.complete()
  current=json.loads((f.root/'data/Times.json').read_text())
  assert [r['PhysPos'] for r in current['Channels']['GN-Pi']['Rows']]==['Main 8','Main 4']
  assert [r['Id'] for r in current['Channels']['GN-Pi']['Rows']]==['LightAq1','MainAq1']
  # MT -> standalone transfers switch-time shape and interval channel name.
  view=f.request('/api/device-assignments');slots={r['Function']:r['EndpointId'] for r in view['Endpoints']}
  f.operation('SwapEndpoints',First=slots['LightAq1'],Second=slots['Spare'],ConsumersDetached=True);f.complete()
  current=json.loads((f.root/'data/Times.json').read_text());migrated={r['Id']:r for r in current['Times']}
  assert migrated['light-on']['Data']==['10:00',255,1] and migrated['light-on']['Device']=='Shelly1'
  assert migrated['interval']['Data'][4]=='LightAq1' and migrated['interval']['Device']=='Shelly1'
  assert current['Channels']['GN-Pi']['Rows'][0]['Id']=='Spare'
  # Sensors exchange only source functions, never feeding schedules.
  view=f.request('/api/device-assignments');slots={r['Function']:r['EndpointId'] for r in view['Endpoints']}
  feed_before=[r for r in current['Times'] if r['Type']=='FeedTimes']
  f.operation('SwapEndpoints',First=slots['FeederAq1'],Second=slots['FeederAq2'],ConsumersDetached=True);f.complete()
  current=json.loads((f.root/'data/Times.json').read_text())
  assert [r for r in current['Times'] if r['Type']=='FeedTimes']==feed_before
  assert mt.sources['FeederAq1']==f.devices[3].identity
  display=f.request('/Devices.json')
  feeder=next(row for row in display['Devices'] if row['Name']=='FeederAq1')
  assert next(v for v in feeder['GroupViews'] if v['Group']=='Aq1')['Measurements']==[]
  assert next(v for v in feeder['GroupViews'] if v['Group']=='Aq2')['Measurements']==['Temperature']

  # Recovery of a committed central operation across server restart, with one unavailable client.
  f.devices[1].allow=False
  view=f.request('/api/device-assignments');slots={r['Function']:r['EndpointId'] for r in view['Endpoints']}
  f.operation('SwapEndpoints',First=slots['LightAq1'],Second=slots['Spare'],ConsumersDetached=True)
  f.stop();f.start();f.devices[1].allow=True;f.complete()
  current=json.loads((f.root/'data/Times.json').read_text())
  assert next(r for r in current['Times'] if r['Id']=='light-on')['Data']==['10:00',255,'LightAq1',True]
  view=f.request('/api/device-assignments');slots={r['Function']:r['EndpointId'] for r in view['Endpoints']}
  f.operation('SwapEndpoints',First=slots['FloorAq1'],Second=slots['Spare'],ConsumersDetached=True);f.complete()
  current=json.loads((f.root/'data/Times.json').read_text())
  assert next(r for r in current['Times'] if r['Id']=='floor-on')['Device']=='Shelly1'
  assert current['Channels']['Shelly1']['Rows'][0]['Role']=='HeaterFloor'
  print('Passed endpoint swaps: strict capabilities, firmware guard, heater roles, physical positions/pins, time conversion, intervals, MT/MT, MT/NS, sensor sources, no credential writes, restart recovery.')
 finally:f.close()

if __name__=='__main__':run()
