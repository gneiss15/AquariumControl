# test_device_editor.py, Version: 1.03
"""Nested JSON editor API: validation, atomic metadata and lifecycle distribution."""
import json,time,urllib.error
from test_assignment_control import Fixture,Device

def run():
 f=Fixture()
 try:
  f.stop();p=f.root/'Devices.json';config=json.loads(p.read_text());config['HardwareNames']={d.identity[:17]:'Hardware'+str(i) for i,d in enumerate(f.devices,1)};p.write_text(json.dumps(config));f.start()
  def editor():return f.request('/api/device-editor')
  def submit(data):return f.request('/api/device-assignments',{'Operation':'EditDevices','ExpectedRevision':data['Revision'],'Devices':data['Devices']})
  def reject(data):
   old=f.request('/api/device-registry')['Revision']
   try:submit(data);raise AssertionError('Bad edit accepted')
   except urllib.error.HTTPError as error:assert error.code==409
   assert f.request('/api/device-registry')['Revision']==old
  original=editor();assert len(original['Devices'])==4
  bad=json.loads(json.dumps(original));bad['Devices'][1]['MAC']=bad['Devices'][0]['MAC'];reject(bad)
  bad=json.loads(json.dumps(original));bad['Devices'][0]['Function']='Renamed';reject(bad)
  bad=json.loads(json.dumps(original));bad['Devices']=[r for r in bad['Devices'] if r['Function']!='FeederAq1'];reject(bad)
  changed=editor();next(r for r in changed['Devices'] if r['Function']=='Shelly1')['Group']='Aq2'
  submit(changed);f.complete()
  views=f.request('/Devices.json');assert next(r for r in views['Devices'] if r['Name']=='Shelly1')['Group']=='Aq2'
  f.stop();f.start();assert next(r for r in f.request('/Devices.json')['Devices'] if r['Name']=='Shelly1')['Group']=='Aq2'
  reject(original)
  new=Device('Reserve3','02:55:66:77:88:99@4','TmrSwA8T');f.devices.append(new)
  added=editor();added['Devices'].append({'Function':'Reserve3','MAC':new.identity[:17],'IP':'127.0.0.1','HttpPort':new.port,'ServiceId':4,'DeviceType':new.kind,'HardwareName':'Nous9','Group':'System','UseHardwareName':True,'Active':True})
  submit(added);f.complete();assert new.config['Hostname']=='Nous9'
  assert any(r['Function']=='Reserve3' for r in f.request('/api/device-registry')['Devices'])
  assert any(r['Device']=='Reserve3' for r in f.request('/api/times')['Sets'])
  deleted=editor();deleted['Devices']=[r for r in deleted['Devices'] if r['Function']!='Reserve3']
  submit(deleted);f.complete();assert all(r['Function']!='Reserve3' for r in f.request('/api/device-registry')['Devices'])
  assert all(r['Device']!='Reserve3' for r in f.request('/api/times')['Sets'])
  assert all(r['Name']!='Reserve3' for r in f.request('/Devices.json')['Devices'])
  f.stop();native=Device('GN-Pi','02:77:88:99:AA:BB@1','MultiTimer');f.devices.append(native)
  table_path=f.root/'data/DeviceAssignments.json';doc=json.loads(table_path.read_text());native.applied=doc['Revision'];native.sources={d.function:d.identity for d in f.devices if d.identity.endswith('@3')}
  doc['Devices'].append({'MAC':native.identity[:17],'IP':'127.0.0.1','Function':'GN-Pi','DeviceType':'MultiTimer','ServiceId':1,'HttpPort':native.port,'DeviceId':native.identity,'Group':'Aq1','AdditionalServices':[{'Function':'DashboardSystem','ServiceId':2,'HttpPort':f.port,'DeviceId':native.identity[:17]+'@2'}]})
  doc['EditorConfiguration']['Devices'].append({'Name':'GN-Pi','BaseUrl':f'http://127.0.0.1:{native.port}','Group':'Aq1'})
  table_path.write_text(json.dumps(doc));f.start()
  assert next(r for r in f.request('/api/device-registry')['Devices'] if r['ServiceId']==2)['Function']=='Dashboard'
  renamed=json.loads(table_path.read_text());assert renamed['Revision']==doc['Revision']
  assert renamed['Devices'][-1]['AdditionalServices'][0]['Function']=='Dashboard'
  assert f.request('/api/v1/device')['DeviceId']==native.identity[:17]+'@2'
  bad=editor();next(r for r in bad['Devices'] if r['Function']=='GN-Pi')['Group']='System';reject(bad)
  no_op=editor();revision=no_op['Revision'];assert submit(no_op)['Pending']==False
  assert f.request('/api/device-registry')['Revision']==revision
  inactive=editor();inactive['Devices'].append({'Function':'FeederAq3','MAC':'00:00:00:00:00:00','IP':'127.0.0.254','HttpPort':80,'ServiceId':3,'DeviceType':'FeederSimple','HardwareName':'','Group':'Aq3','UseHardwareName':False,'Active':False})
  submit(inactive);f.complete()
  assert all(r['Function']!='FeederAq3' for r in f.request('/api/device-registry')['Devices'])
  assert next(r for r in f.request('/Devices.json')['Devices'] if r['Name']=='FeederAq3')['Active']==False
  print('Passed: duplicate/identity/reference/stale rejection without writes, metadata survives restart, verified new device and hostname, new empty time set, removal cleans registry/view/sync sets.')
 finally:f.close()

if __name__=='__main__':run()
