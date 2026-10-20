# test_assignment_control.py, Version: 1.06
"""Loopback-only end-to-end tests of activation, swaps and persisted partial recovery."""
import concurrent.futures, http.server, json, pathlib, socket, subprocess, tempfile, threading, time, urllib.error, urllib.request

SOURCE = pathlib.Path(__file__).resolve().parents[1]
class Device:
 def __init__(self, function, identity, kind):
  self.function=function; self.identity=identity; self.kind=kind; self.mode='MAC'; self.applied='2026-10-13T00:00:00.000Z'; self.endpoint_version=1; self.version=3; self.temperature_enabled=True; self.temperature_valid=True; self.temperature_known=True; self.boot=1; self.restart_pending=False; self.allow_restart=True; self.allow=True; self.feeding=False; self.received=[]; self.sources={}; self.hardware={'Calibration':12345,'PhysicalType':kind}; self.config={'Hostname':function,'ApSsid':function,'ApPw':function,'FtpPw':function,'WlanSsid':'fixed-network','Calibration':12345}
  device=self
  class Handler(http.server.BaseHTTPRequestHandler):
   def do_GET(self):
    if device.restart_pending and device.allow_restart:device.boot+=1;device.restart_pending=False
    if self.path=='/Config.json':value=device.config.copy()
    elif self.path.startswith('/api/v1/status'):
     value={'Channels':[{'Id':0,'Name':device.function,'State':device.feeding}],'Healthy':True,'TimeSynchronized':True}
     if device.temperature_known:value['Measurements']=[{'Id':'Temperature','Value':25 if device.temperature_valid else None,'Valid':device.temperature_valid}] if device.temperature_enabled and device.identity.endswith('@3') else []
    else:
     value={'ApiVersion':1,'EndpointAssignments':device.endpoint_version,'DeviceId':device.identity if device.mode=='MAC' else device.function,'DeviceType':device.kind,'Name':device.function,'FirmwareVersion':'9.99','Capabilities':['Status'],'Identity':{'Version':device.version,'DeviceId':device.identity,'Mode':device.mode,'Function':device.function,'Sources':device.sources,'Prepared':True,'AppliedRevision':device.applied,'BootCounter':device.boot}}
    body=json.dumps(value).encode();self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
    # Model ESP8266: send a complete body but keep the socket open.
    if device.identity.endswith('@3'):
     self.wfile.flush();self.close_connection=False
   def do_POST(self):
    payload=json.loads(self.rfile.read(int(self.headers['Content-Length'])));device.received.append(payload)
    if not device.allow:self.send_response(503);self.end_headers();return
    assert self.path=='/api/v1/times' and payload['Device']==device.identity,payload
    identity=payload['Identity']; assert identity['DeviceId']==device.identity and identity['Mode']=='MAC',identity
    changed=device.function!=identity['Function']
    if 'FunctionConfig' in payload:
     fields=payload['FunctionConfig'];assert set(fields)=={'Hostname','ApSsid','ApPw','FtpPw'}
     changed=changed or any(device.config.get(key)!=value for key,value in fields.items())
     device.config.update(fields)
    device.function=identity['Function'];device.mode=identity['Mode'];device.sources=identity['Sources'];device.applied=identity['Revision']
    if changed:device.restart_pending=True
    body=b'{"Ok":true}';self.send_response(200);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
   def log_message(self,*args):pass
  self.server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler);self.port=self.server.server_address[1]
  threading.Thread(target=self.server.serve_forever,daemon=True).start()

class Fixture:
 def __init__(self):
  self.temporary=tempfile.TemporaryDirectory(prefix='aq-assignment-test-');self.root=pathlib.Path(self.temporary.name);(self.root/'data').mkdir()
  self.devices=[Device('HeaterMainAq1','02:11:22:33:44:55@4','TmrSwA8T'),Device('Shelly1','02:22:33:44:55:66@4','TmrSwShellyC3V1'),Device('FeederAq1','02:33:44:55:66:77@3','Feeder'),Device('FeederAq2','02:44:55:66:77:88@3','Feeder')]
  sources={device.function:device.identity for device in self.devices if device.identity.endswith('@3')}
  rows=[]
  for device in self.devices:
   device.sources=sources.copy() if device.identity.endswith('@4') else {}
   rows.append({'MAC':device.identity[:17],'IP':'127.0.0.1','Function':device.function,'LegacyDeviceId':device.function,'ServiceId':int(device.identity[-1]),'HttpPort':device.port,'DeviceId':device.identity,'Group':'Aq1' if device.function=='HeaterMainAq1' else 'System','DeviceType':device.kind})
  self.store={'Version':1,'Times':[{'Id':'first-feed','Device':'FeederAq1','Type':'FeedTimes','Kind':'FeedTime','Data':['10:00',127,2]},{'Id':'second-feed','Device':'FeederAq2','Type':'FeedTimes','Kind':'FeedTime','Data':['18:00',127,4]}],'Sets':[{'Device':row['Function'],'Type':'FeedTimes' if row['ServiceId']==3 else 'SwitchTimes','Format':'Feeder' if row['ServiceId']==3 else 'TmrSw','TimeStamp':'2026-10-13T00:00:00.000Z','Synced':True} for row in rows],'Channels':{'HeaterMainAq1':{'TimeStamp':'2026-10-13T00:00:00.000Z','Rows':[{'Id':'MainAq1','Role':'HeaterMain','Profile':'HeatProfileAq1'}]},'Shelly1':{'TimeStamp':'2026-10-13T00:00:00.000Z','Rows':[{'Id':'Spare','Role':'Timer'}]}}}
  (self.root/'data/Times.json').write_text(json.dumps(self.store));(self.root/'data/DeviceAssignments.json').write_text(json.dumps({'Version':1,'State':'MAC','Revision':'2026-10-13T00:00:00.000Z','Devices':rows}))
  self.profiles=[{'Id':'HeatProfileAq1','TempId':'FeederAq1','MainOn':25,'MainOff':26,'FloorOn':25,'FloorOff':26,'NoTempOn':True}]
  (self.root/'data/HeatProfiles.json').write_text(json.dumps({'Version':1,'TimeStamp':'2026-10-13T00:00:00.000Z','Profiles':self.profiles}))
  (self.root/'Devices.json').write_text(json.dumps({'Devices':[{'Name':device.function,'Group':'System','BaseUrl':f'http://127.0.0.1:{device.port}'} for device in self.devices]}))
  for name in ['index.html','Dashboard.js','Index.css','Translations.json','StatusClient.js']:(self.root/name).write_bytes((SOURCE/name).read_bytes())
  s=socket.socket();s.bind(('127.0.0.1',0));self.port=s.getsockname()[1];s.close();self.base=f'http://127.0.0.1:{self.port}';self.process=None;self.log=None;self.start()
 def start(self):
  self.log=open(self.root/'server.log','a');self.process=subprocess.Popen([str(SOURCE/'DashboardServer'),str(self.port),str(self.root)],stdout=self.log,stderr=self.log)
  for _ in range(80):
   try:self.request('/api/device-registry');return
   except Exception:
    if self.process.poll() is not None:raise RuntimeError((self.root/'server.log').read_text())
    time.sleep(.05)
  raise RuntimeError('Server did not start')
 def stop(self):
  if self.process:self.process.terminate();self.process.wait(timeout=5);self.log.close();self.process=None
 def close(self):
  self.stop()
  for device in self.devices:device.server.shutdown();device.server.server_close()
  self.temporary.cleanup()
 def request(self,path,data=None,method=None):
  body=None if data is None else json.dumps(data).encode();request=urllib.request.Request(self.base+path,data=body,method=method,headers={'Content-Type':'application/json'})
  with urllib.request.urlopen(request,timeout=10) as result:return json.load(result)
 def operation(self,operation,**values):
  revision=self.request('/api/device-registry')['Revision'];return self.request('/api/device-assignments',{'Operation':operation,'ExpectedRevision':revision,**values})
 def reject(self,operation,code=409,**values):
  try:self.operation(operation,**values);raise AssertionError('Operation unexpectedly accepted')
  except urllib.error.HTTPError as error:assert error.code==code,(error.code,error.read())
 def complete(self):
  for _ in range(80):
   view=self.request('/api/device-assignments')
   if not view['Pending']:assert view['AllApplied'];return view
   time.sleep(.1)
  raise AssertionError('Operation did not complete: '+repr(view))

def run():
 fixture=Fixture()
 try:
  a,b,f1,f2=fixture.devices
  assert fixture.request('/api/device-assignments')['AllApplied']
  # Switches now use endpoint swaps; whole-device swaps remain for feeders.
  fixture.reject('Swap',409,First=a.function,Second=b.function,ConsumersDetached=True)
  f1.version=2;fixture.reject('Swap',First=f1.function,Second=f2.function,ConsumersDetached=True);f1.version=3
  fixture.reject('ActivateMAC',400)
  fixture.reject('Swap',400,First=f1.function,Second=f2.function,ConsumersDetached=False)
  fixture.reject('Swap',400,First=a.function,Second=f1.function,ConsumersDetached=True)
  fixture.reject('Swap',400,First=f1.function,Second=f1.function,ConsumersDetached=True)
  revision=fixture.request('/api/device-registry')['Revision']
  f2.temperature_enabled=False
  view=fixture.request('/api/device-assignments')
  assert next(r for r in view['Devices'] if r['DeviceId']==f2.identity)['CapabilityId']==0
  assert next(r for r in view['Endpoints'] if r['DeviceId']==f2.identity)['Capabilities']==0
  fixture.reject('Swap',409,First=f1.function,Second=f2.function,ConsumersDetached=True)
  fixture.reject('SwapEndpoints',409,First=f1.identity+'/0',Second=f2.identity+'/0',ConsumersDetached=True)
  assert fixture.request('/api/device-registry')['Revision']==revision and not fixture.request('/api/device-registry')['Pending']
  f2.temperature_enabled=True;f2.temperature_valid=False
  assert next(r for r in fixture.request('/api/device-assignments')['Devices'] if r['DeviceId']==f2.identity)['CapabilityId']==2
  f2.temperature_valid=True;f2.temperature_known=False
  fixture.reject('Swap',409,First=f1.function,Second=f2.function,ConsumersDetached=True)
  f2.temperature_known=True
  f1.temperature_enabled=False;f2.temperature_enabled=False
  fixture.operation('Swap',First=f1.function,Second=f2.function,ConsumersDetached=True);fixture.complete()
  fixture.operation('Swap',First=f1.function,Second=f2.function,ConsumersDetached=True);fixture.complete()
  assert not f1.temperature_enabled and not f2.temperature_enabled
  f1.temperature_enabled=True;f2.temperature_enabled=True
  f1.feeding=True;fixture.reject('Swap',409,First=f1.function,Second=f2.function,ConsumersDetached=True);f1.feeding=False
  f1.allow=False;f1.allow_restart=False
  fixture.operation('Swap',First=f1.function,Second=f2.function,ConsumersDetached=True)
  assert fixture.request('/api/device-registry')['Pending']
  response=fixture.request('/api/times/sync',{'Device':f1.identity,'Type':'FeedTimes','Format':'Feeder','TimeStamp':'2099-01-01T00:00:00.000Z','Content':[['09:00',127,99,'stale-feed']],'Identity':{'Version':3,'DeviceId':f1.identity,'Mode':'MAC','Function':'FeederAq1','AppliedRevision':'old'}})
  assert response['Action']=='UseCentral' and response['Identity']['Function']=='FeederAq2'
  fixture.stop();fixture.start();assert fixture.request('/api/device-registry')['Pending']
  f1.allow=True;fixture.operation('Resume');time.sleep(.15)
  assert fixture.request('/api/device-registry')['Pending'], 'Must wait for actual restart'
  f1.allow_restart=True;fixture.complete()
  assert f1.config['Hostname']=='FeederAq2' and f2.config['Hostname']=='FeederAq1'
  assert f1.config['ApPw']=='FeederAq2' and f2.config['FtpPw']=='FeederAq1'
  assert f1.config['WlanSsid']==f2.config['WlanSsid']=='fixed-network'
  assert f1.hardware=={'Calibration':12345,'PhysicalType':'Feeder'}
  assert f1.received[-1]['Content'][0][2]==4 and f2.received[-1]['Content'][0][2]==2
  for device in (a,b):assert device.sources=={'FeederAq1':f2.identity,'FeederAq2':f1.identity},device.sources
  try:fixture.request('/data/DeviceAssignments.json');raise AssertionError('Secret journal served')
  except urllib.error.HTTPError as error:assert error.code==403
  assert 'FunctionConfig' not in json.dumps(fixture.request('/api/device-registry'))
  fixture.reject('Swap',409,First='FeederAq1',Second='FeederAq2',ConsumersDetached=True,ExpectedRevision='old')
  print('Passed: feeder whole-device swap, busy/identity/stale guards, credentials and restart acknowledgement, pending restart/resume, source and schedule routing, hardware retention, protected journal, legacy switch swap rejection.')
 finally:fixture.close()

if __name__=='__main__':run()
