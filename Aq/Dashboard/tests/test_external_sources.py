# test_external_sources.py, Version: 1.01
# Loopback-only regression for measurement senders without managed-device APIs.
import json,subprocess,pathlib,socket,time
from test_assignment_control import Fixture
f=Fixture()
try:
 f.stop();p=f.root/'data/DeviceAssignments.json';d=json.loads(p.read_text());d['ExternalSources']={'LaptopBattery':'F4:06:69:2A:E9:CD@3'};p.write_text(json.dumps(d));f.start()
 view=f.request('/api/device-registry');assert view['ExternalSources']==d['ExternalSources'];assert not any(r['DeviceId']=='F4:06:69:2A:E9:CD@3' for r in view['Devices'])
 # Display metadata does not register the sender as a managed device.
 (f.root/'Devices.json').write_text(json.dumps({'Devices':[]}))
 assert f.request('/Devices.json')['ExternalSources']==d['ExternalSources']
 identity=d['ExternalSources']['LaptopBattery'];url='/api/device-status/'+identity
 assert f.request(url)['Online'] is False
 status={'Kind':'LaptopPower','BatteryPercent':67,'AcOnline':False,'BatteryState':'Discharging'}
 with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
  for _ in range(20):
   udp.sendto(json.dumps({'Version':2,'Type':'Status','ID':identity,'Status':status}).encode(),('127.0.0.1',44444))
   time.sleep(.05);received=f.request(url)
   if received['Status']==status:break
  else:raise AssertionError('External status missing from display cache')
  assert received['Online'] and received['AgeMilliseconds']<20000
  unknown='F4:06:69:2A:E9:CE@3'
  udp.sendto(json.dumps({'Version':2,'Type':'Status','ID':unknown,'Status':status}).encode(),('127.0.0.1',44444))
  time.sleep(.1);assert f.request('/api/device-status/'+unknown)['Status'] is None
  udp.sendto(json.dumps({'Version':1,'Type':'Status','ID':identity,'Status':{'BatteryPercent':1}}).encode(),('127.0.0.1',44444))
  time.sleep(.1);assert f.request(url)['Status']==status
 device=f.devices[0]
 req={'Device':device.identity,'Type':'SwitchTimes','Format':'TmrSw','TimeStamp':'2026-10-13T00:00:00.000Z','Content':{'SwitchTimes':[],'Intervals':[]},'Identity':{'Version':3,'DeviceId':device.identity,'Mode':'MAC','Function':device.function,'AppliedRevision':view['Revision']}}
 response=f.request('/api/times/sync',req);assert response['Identity']['Sources']['LaptopBattery']=='F4:06:69:2A:E9:CD@3'
 f.stop()
 original=json.loads(p.read_text())
 for source in [{'LaptopBattery':'F4:06:69:2A:E9:CD@4'},{'LaptopBattery':'00:00:00:00:00:00@3'},{'LaptopBattery':'f4:06:69:2a:e9:cd@3'},{f.devices[0].function:'F4:06:69:2A:E9:CD@3'}]:
  invalid=json.loads(json.dumps(original));invalid['ExternalSources']=source;p.write_text(json.dumps(invalid))
  result=subprocess.run([str(pathlib.Path(__file__).resolve().parents[1]/'DashboardServer'),str(f.port),str(f.root)],capture_output=True,text=True)
  assert result.returncode==1 and 'external source' in result.stderr,result.stderr
 print('External source distributed without managed-device registration; invalid/conflicting identities rejected.')
finally:f.close()
