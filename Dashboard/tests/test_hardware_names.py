# test_hardware_names.py, Version: 1.02
"""Physical hardware labels follow MAC through a swap, without renaming functions."""
import json,pathlib,subprocess
from test_assignment_control import Fixture

def run():
 fixture=Fixture()
 try:
  fixture.stop();path=fixture.root/'Devices.json';config=json.loads(path.read_text())
  first,second=fixture.devices[:2]
  config['HardwareNames']={first.identity[:17]:'Nous1',second.identity[:17]:'Shelly1'}
  for row in config['Devices']:
   if row['Name']==second.function:row['UseHardwareName']=True
  path.write_text(json.dumps(config))
  journal_path=fixture.root/'data/DeviceAssignments.json';journal=json.loads(journal_path.read_text());journal.pop('Endpoints',None);journal_path.write_text(json.dumps(journal))
  fixture.start()
  view=fixture.request('/api/device-registry');by_id={row['DeviceId']:row for row in view['Devices']}
  assert by_id[first.identity]['HardwareName']=='Nous1' and by_id[second.identity]['HardwareName']=='Shelly1'
  endpoints=fixture.request('/api/device-assignments')['Endpoints'];slots={row['DeviceId']:row['EndpointId'] for row in endpoints}
  hostnames=[first.config['Hostname'],second.config['Hostname']]
  fixture.operation('SwapEndpoints',First=slots[first.identity],Second=slots[second.identity],ConsumersDetached=True);fixture.complete()
  assert [first.config['Hostname'],second.config['Hostname']]==hostnames
  view=fixture.request('/api/device-registry');by_id={row['DeviceId']:row for row in view['Devices']}
  assert by_id[first.identity]['HardwareName']=='Nous1' and by_id[second.identity]['HardwareName']=='Shelly1'
  config=fixture.request('/Devices.json');reserve=next(row for row in config['Devices'] if row['DeviceId']==first.identity)
  assert reserve['HardwareName']=='Nous1' and reserve['UseHardwareName']
  assert reserve['EndpointHardwareLabels']['Spare']=='Nous1'
  source=(pathlib.Path(__file__).resolve().parents[1]/'Dashboard.js').read_text()
  snippets=[]
  for name,next_name in [('DeviceDisplayName','ChannelDisplayName'),('ChannelDisplayName','RefreshTranslatedContent')]:
   snippets.append(source[source.index('function '+name+'('):source.index('function '+next_name+'(')])
  script="const Devices=[{TechnicalName:'Shelly1',Configuration:{UseHardwareName:true,HardwareName:'Nous1'}}];const Translations={};const Tr=(key,fallback)=>fallback;"+'\n'.join(snippets)+"\nif(DeviceDisplayName('Shelly1')!=='Nous1'||DeviceDisplayName('LightTimerAq3')!=='LightTimerAq3'||ChannelDisplayName({Id:'Schaltausgang'},'Shelly1')!=='Nous1')throw Error('Wrong hardware display');"
  subprocess.run(['node','-e',script],check=True)
  print('Passed: MAC-bound names survive swap, reserve configuration follows actual hardware, concrete function names preserved, reserve channel label updated.')
 finally:fixture.close()

if __name__=='__main__':run()
