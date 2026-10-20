# test_registry_transition.py, Version: 1.05
# Historical filename retained; MAC-only protocol and invalid-table regression.
import json,pathlib,subprocess,urllib.error
from test_assignment_control import Fixture

def run():
 fixture=Fixture()
 try:
  device=fixture.devices[0]
  view=fixture.request('/api/device-registry')
  assert view['Mode']=='MAC' and not view['Pending']
  request={'Device':device.function,'Type':'SwitchTimes','Format':'TmrSw','TimeStamp':'2026-10-13T00:00:00.000Z','Content':{'SwitchTimes':[],'Intervals':[]},'Identity':{'Version':3,'DeviceId':device.identity,'Mode':'MAC','Function':device.function,'AppliedRevision':view['Revision']}}
  try:fixture.request('/api/times/sync',request);raise AssertionError('Hostname wire ID accepted')
  except urllib.error.HTTPError as error:assert error.code==409
  request['Device']=device.identity
  response=fixture.request('/api/times/sync',request)
  assert response['Identity']['DeviceId']==device.identity and response['Identity']['Mode']=='MAC'
  assert response['Identity']['Sources']=={r.function:r.identity for r in fixture.devices if r.identity.endswith('@3')}
  request['Identity']['Mode']='Legacy'
  try:fixture.request('/api/times/sync',request);raise AssertionError('Legacy identity accepted')
  except urllib.error.HTTPError as error:assert error.code==409
  fixture.stop()
  path=fixture.root/'data/DeviceAssignments.json';original=json.loads(path.read_text())
  for change,message in [('legacy','MAC assignment table required'),('mac','Invalid registry MAC/IP')]:
   data=json.loads(json.dumps(original))
   if change=='legacy':data['State']='Transition'
   else:data['Devices'][0]['MAC']='invalid'
   path.write_text(json.dumps(data))
   result=subprocess.run([str(pathlib.Path(__file__).resolve().parents[1]/'DashboardServer'),str(fixture.port),str(fixture.root)],capture_output=True,text=True)
   assert result.returncode==1 and message in result.stderr,result.stderr
  print('Passed: MAC-only wire IDs, physical source mapping, legacy identity/table rejection, malformed MAC rejection. Loopback only.')
 finally:fixture.close()

if __name__=='__main__':run()
