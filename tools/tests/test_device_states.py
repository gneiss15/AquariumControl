# test_device_states.py, Version: 1.04
"""PC adoption of actual FS snapshots; no hardware/image cross-copy or background sync."""
import importlib.util,json,pathlib,tempfile,shutil
ROOT=pathlib.Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('read_states',ROOT/'tools/ReadDeviceStates.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='aq-readstate-test-') as temporary:
 aq=pathlib.Path(temporary);(aq/'Dashboard/data').mkdir(parents=True);rows=[]
 for i,(fn,kind) in enumerate([('LightTimerAq3','TmrSwA8T'),('Shelly1','TmrSwShellyC3V1'),('FeederAq1','Feeder'),('FeederAq2','FeederSimple')],1):
  mac=f'02:11:22:33:44:{i:02X}';service=4 if i<3 else 3
  row={'Function':fn,'DeviceId':mac+'@'+str(service),'MAC':mac,'IP':f'127.0.0.{i}','HttpPort':80,'DeviceType':kind,'ServiceId':service,'Group':fn};rows.append(row)
  directory=aq/module.PROJECTS[kind]/'DevState'/fn;directory.mkdir(parents=True)
  (directory/'Config.json').write_text(json.dumps({'Hostname':fn,'HardwarePin':i}));(directory/'hardware.txt').write_text(str(i))
 table=aq/'Dashboard/data/DeviceAssignments.json';table.write_text(json.dumps({'Version':1,'State':'MAC','Revision':'old','Devices':rows}))
 live=json.loads(json.dumps(rows));live[0]['Function'],live[1]['Function']=live[1]['Function'],live[0]['Function'];live[2]['Function'],live[3]['Function']=live[3]['Function'],live[2]['Function']
 view={'Mode':'MAC','Pending':False,'Revision':'new','Devices':live};snapshots=[]
 for i,row in enumerate(live,1):
  directory=aq/'downloads'/str(i);directory.mkdir(parents=True)
  config={key:row['Function']+'-'+key for key in ['Hostname','ApSsid','ApPw','FtpPw']};config.update({'HardwarePin':i,'WlanSsid':'same-network'})
  (directory/'Config.json').write_text(json.dumps(config));(directory/'Identity.json').write_text('{"DeviceId":"runtime-cache"}');(directory/'Index.html').write_text('old-web-source')
  if row['ServiceId']==4:(directory/'Channels.json').write_text(json.dumps({'Channels':[{'Id':row['Function'],'Role':'Timer'}]}))
  if row['DeviceType']=='Feeder':(directory/'ServoSteps.json').write_text('[123]')
  snapshots.append({**row,'Project':module.PROJECTS[row['DeviceType']],'Snapshot':str(directory)})
 trash=aq/'Trash';trash.mkdir();counter=[0]
 def recoverable(path):counter[0]+=1;shutil.move(path,trash/str(counter[0]))
 try:module.adopt(view,snapshots[:-1],aq,recoverable);raise AssertionError('Partial readback accepted')
 except ValueError:pass
 assert len(module.adopt(view,snapshots,aq,recoverable))==8
 for i,row in enumerate(live,1):
  directory=aq/module.PROJECTS[row['DeviceType']]/'DevState'/row['Function'];config=json.loads((directory/'Config.json').read_text())
  assert config['HardwarePin']==i and config['WlanSsid']=='same-network' and (directory/'hardware.txt').read_text()==str(i)
  assert all(config[key]==row['Function']+'-'+key for key in ['Hostname','ApSsid','ApPw','FtpPw'])
  assert not (directory/'Identity.json').exists() and not (directory/'Index.html').exists()
  if row['ServiceId']==4:assert json.loads((directory/'Channels.json').read_text())['Channels'][0]['Id']==row['Function']
 assert not (aq/'TmrSwA8T/DevState/LightTimerAq3').exists() and not (aq/'TmrSwShelly/DevState/Shelly1').exists()
 assert len(list(trash.iterdir()))==4
 try:module.adopt({**view,'Pending':True},snapshots,aq,recoverable);raise AssertionError('Pending assignment accepted')
 except ValueError:pass
 before={path:(path.read_bytes(),path.stat().st_mtime_ns) for path in aq.glob('*/DevState/*/*') if path.is_file()}
 assert module.adopt(view,snapshots,aq,recoverable)==[]
 assert all((path.read_bytes(),path.stat().st_mtime_ns)==value for path,value in before.items())
 changed=aq/'downloads/1/Config.json';value=json.loads(changed.read_text());value['HardwarePin']=99;changed.write_text(json.dumps(value))
 result=module.adopt(view,snapshots,aq,recoverable)
 assert result==[str(aq/'TmrSwA8T/DevState/Shelly1/Config.json')]
 assert all(path.stat().st_mtime_ns==value[1] for path,value in before.items() if str(path)!=result[0])
 residual=aq/'TmrSwA8T/generated/Instances/Shelly1/ftp-after-swap-test';residual.mkdir(parents=True);(residual/'Config.json').write_text('{}')
 module.cleanup_readbacks(aq);assert not residual.exists() and not (aq/'generated/device-state-readbacks').exists()
 extra={**live[0],'Function':'Reserve9','DeviceId':'02:99:88:77:66:55@4','MAC':'02:99:88:77:66:55','Project':'TmrSwA8T','DeviceType':'TmrSwA8T','Group':'System'}
 extra_dir=aq/'downloads/extra';extra_dir.mkdir();(extra_dir/'Config.json').write_text('{"Hostname":"Nous9"}');(extra_dir/'Channels.json').write_text('{"Profiles":[],"Channels":[]}')
 extra['Snapshot']=str(extra_dir);new_view={**view,'Devices':live+[extra],'Revision':'newer'}
 added=module.adopt(new_view,snapshots+[extra],aq,recoverable)
 assert (aq/'TmrSwA8T/DevState/Reserve9/Config.json').exists() and len(added)==3
 removed=module.adopt({**view,'Revision':'removed'},snapshots,aq,recoverable)
 assert not (aq/'TmrSwA8T/DevState/Reserve9').exists() and str(table) in removed
 print('Passed: complete-readback guard, persistent actual-device data, function folders in correct hardware projects, hardware/servo retained, runtime/web excluded, obsolete dirs recoverable, pending guard.')
