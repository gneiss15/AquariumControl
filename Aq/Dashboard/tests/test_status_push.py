# test_status_push.py, Version: 1.00
"""Exercise actual HTTP/SSE sockets and UDP ingress without touching devices."""
import http.client,json,socket,time
from test_assignment_control import Fixture
f=Fixture();streams=[]
try:
 f.stop();p=f.root/'data/DeviceAssignments.json';table=json.loads(p.read_text());identity='02:99:88:77:66:55@3';table['ExternalSources']={'LaptopBattery':identity};p.write_text(json.dumps(table));f.start()
 def connect():
  connection=http.client.HTTPConnection('127.0.0.1',f.port,timeout=8)
  connection.request('GET','/api/events');response=connection.getresponse()
  assert response.status==200 and response.getheader('Content-Type')=='text/event-stream'
  streams.append((connection,response));return response
 def event(response,target,predicate):
  deadline=time.monotonic()+8
  while time.monotonic()<deadline:
   line=response.readline()
   assert line,'stream unexpectedly closed'
   if not line.startswith(b'data: '):continue
   batch=json.loads(line[6:]);assert response.readline()==b'\n'
   for row in batch:
    if row['ID']==target and predicate(row):return row
  raise AssertionError('event not received')
 first=connect();event(first,identity,lambda row:not row['Online'])
 # An idle live stream must not block ordinary requests or a second browser.
 assert 'Devices' in f.request('/api/device-registry')
 second=connect();event(second,identity,lambda row:not row['Online'])
 with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as udp:
  status={'Kind':'LaptopPower','BatteryPercent':19,'Note':'line\none'}
  packet={'Version':2,'Type':'Status','ID':identity,'Status':status}
  started=time.monotonic();udp.sendto(json.dumps(packet).encode(),('127.0.0.1',44444))
  a=event(first,identity,lambda row:row['Online']);b=event(second,identity,lambda row:row['Online'])
  assert a['Status']==status and b['Status']==status
  assert time.monotonic()-started<2,'UDP-to-stream forwarding delayed'
  # A reconnect receives the cached snapshot without requiring a fresh UDP packet.
  third=connect();assert event(third,identity,lambda row:row['Online'])['Status']==status
  for version in (1,):
   packet['Version']=version;packet['Status']={'BatteryPercent':99};udp.sendto(json.dumps(packet).encode(),('127.0.0.1',44444))
  time.sleep(.1);assert f.request('/api/device-status/'+identity)['Status']==status
  # Silence alone must push the online-to-offline transition.
  started=time.monotonic()
  for _ in range(5):
   try:offline=event(first,identity,lambda row:not row['Online']);break
   except AssertionError:
    if time.monotonic()-started>25:raise
  else:raise AssertionError('offline transition missing')
  assert offline['Status']==status
 # The stream limit bounds resources and does not consume ordinary HTTP service.
 for _ in range(29):connect()
 overflow=http.client.HTTPConnection('127.0.0.1',f.port,timeout=5);overflow.request('GET','/api/events');assert overflow.getresponse().status==503;overflow.close()
 assert 'Devices' in f.request('/api/device-registry')
 print('SSE/UDP: initial snapshot, multiple browsers, immediate delivery, reconnect, newline framing, V1 rejection, offline transition, capacity and HTTP responsiveness passed.')
finally:
 for connection,response in streams:response.close();connection.close()
 f.close()
