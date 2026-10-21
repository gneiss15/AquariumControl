#!/usr/bin/env python3
# ReadDeviceStates.py, Version: 1.05
"""Explicit PC-side readback after a function swap; never launched by Dashboard.
All ESP filesystem contents are downloaded temporarily. --apply additionally
adopts persistent device data in function-named, hardware-specific DevState dirs.
Web sources and central runtime caches are not promoted to build sources.
Successful adoption removes readback copies; failures retain them.
Only changed persistent files are written; no device writes occur.
"""
import argparse, concurrent.futures, datetime, importlib.util, json, pathlib, re, shutil, subprocess, urllib.request, urllib.error
AQ=pathlib.Path(__file__).resolve().parents[1]
PROJECTS={"Feeder":"Feeder","FeederSimple":"FeederSimple","TmrSwA8T":"TmrSwA8T","TmrSwShellyC3V1":"TmrSwShelly"}

def get_json(url):
  with urllib.request.urlopen(url,timeout=10) as response:return json.load(response)

def download_all(view,aq=AQ):
  if view.get("Mode")!="MAC" or view.get("Pending") or not view.get("Revision"):
    raise ValueError("Wait for confirmed completion of the Dashboard assignment")
  stamp=datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
  spec=importlib.util.spec_from_file_location('download_fs',aq/'tools/DownloadLittleFS.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
  rows=[row for row in view['Devices'] if row['ServiceId'] in (3,4)]
  def read(row):
    function=row['Function']
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_-]*',function):raise ValueError('Invalid function name')
    base='http://'+row['IP']+(':'+str(row['HttpPort']) if row['HttpPort']!=80 else '')
    info=get_json(base+'/api/v1/device')
    if info.get('Identity',{}).get('DeviceId')!=row['DeviceId']:raise ValueError('Physical identity mismatch')
    kind=info.get('DeviceType');project=PROJECTS.get(kind)
    if not project:raise ValueError('Unsupported hardware type')
    config=get_json(base+'/Config.json')
    if not isinstance(config,dict) or not isinstance(config.get('Hostname'),str) or not isinstance(config.get('FtpPw'),str):raise ValueError('Invalid device Config.json')
    target=aq/project/'generated/Instances'/function/('ftp-after-swap-'+stamp);target.mkdir(parents=True)
    try:helper.download(row['IP'],config['Hostname'],config['FtpPw'],target,True)
    except Exception:helper.download(row['IP'],config['Hostname'],config['FtpPw'],target,False)
    if json.loads((target/'Config.json').read_text())!=config:raise ValueError('Config changed during readback; repeat the operation')
    return {**row,'DeviceType':kind,'Project':project,'Snapshot':str(target)}
  with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:return list(pool.map(read,rows))

def equivalent(first,second):
  if not first.is_file():return False
  if first.suffix=='.json':
    try:return json.loads(first.read_text())==json.loads(second.read_text())
    except (ValueError,OSError):pass
  return first.read_bytes()==second.read_bytes()

def cleanup_readbacks(aq=AQ):
  # These exact tool-owned directories contain disposable readback/staging copies.
  for project in set(PROJECTS.values()):
    base=aq/project/'generated/Instances'
    for path in base.glob('*/ftp-after-swap-*'):
      if path.is_dir() and not path.is_symlink():shutil.rmtree(path)
  base=aq/'generated/device-state-readbacks'
  if base.is_dir() and not base.is_symlink():shutil.rmtree(base)

def adopt(view,snapshots,aq=AQ,trash=None):
  if view.get('Mode')!='MAC' or view.get('Pending'):raise ValueError('Assignment still pending')
  table=aq/'Dashboard/data/DeviceAssignments.json';original=table.read_bytes();document=json.loads(original)
  old={row['DeviceId']:row for row in document['Devices']};live={row['DeviceId']:row for row in view['Devices']}
  by_id={item['DeviceId']:item for item in snapshots}
  expected={row['DeviceId'] for row in view['Devices'] if row['ServiceId'] in (3,4)}
  if set(by_id)!=expected:raise ValueError('Incomplete ESP readback; no DevState changes')
  backup=aq/'generated/device-state-readbacks'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
  plans=[];changes=[];obsolete=set()
  for identity,item in by_id.items():
    prior=old.get(identity)
    fresh = prior is None
    if fresh:prior={'DeviceType':item['DeviceType'],'Function':item['Function']}
    project=item['Project'];function=item['Function']
    if PROJECTS.get(prior['DeviceType'])!=project:raise ValueError('Physical firmware type changed unexpectedly')
    source=aq/project/'DevState'/prior['Function'];destination=aq/project/'DevState'/function
    if not fresh and not source.is_dir():raise ValueError('Missing existing hardware DevState')
    required=['Config.json']
    if item['ServiceId']==4:required.append('Channels.json')
    if item['DeviceType']=='Feeder':required.append('ServoSteps.json')
    inputs=[]
    for name in required:
      downloaded=pathlib.Path(item['Snapshot'])/name
      if not downloaded.is_file():raise ValueError('Required persistent file missing: '+name)
      json.loads(downloaded.read_text())
      if fresh or source!=destination or not equivalent(destination/name,downloaded):
        inputs.append((name,downloaded));changes.append(str(destination/name))
    plans.append((source,destination,inputs,fresh))
    if source!=destination or fresh:obsolete.update((source,destination))
  active={(row['Project'],row['Function']) for row in snapshots}
  for item in snapshots:
    for row in old.values():
      if row['Function']==item['Function']:
        project=PROJECTS.get(row.get('DeviceType'))
        if project and (project,row['Function']) not in active:obsolete.add(aq/project/'DevState'/row['Function'])
  groups={row['Function']:row.get('Group','System') for row in document['Devices']}
  for row in document['Devices']:
    if row['DeviceId'] not in live:
      project=PROJECTS.get(row.get('DeviceType'))
      if project:obsolete.add(aq/project/'DevState'/row['Function'])
  document['Devices']=[row for row in document['Devices'] if row['DeviceId'] in live]
  for identity,item in by_id.items():
    if identity not in old:
      current=live[identity]
      document['Devices'].append({**{key:current[key] for key in ('Function','IP','HttpPort','DeviceType','ServiceId','DeviceId')},'MAC':identity[:17],'Group':current.get('Group','System')})
  for row in document['Devices']:
    current=live[row['DeviceId']]
    row.update({key:current[key] for key in ('Function','IP','HttpPort')});row['Group']=current.get('Group',groups.get(row['Function'],'System'))
    row.pop('LegacyDeviceId',None)
    for service in row.get('AdditionalServices',[]):
      current=live[service['DeviceId']];service.update({key:current[key] for key in ('Function','HttpPort')})
  document['Unresolved']=[row for row in document.get('Unresolved',[]) if row['Function'] not in {item['Function'] for item in live.values()}]
  document.pop('State',None);document['Version']=2;document['Revision']=view['Revision'];document.pop('PendingOperation',None)
  if 'Endpoints' in view:document['Endpoints']=view['Endpoints']
  # Validate the complete plan before changing any existing source files.
  if table.read_bytes()!=original:raise ValueError('Local assignment metadata changed during adoption')
  if obsolete and trash is None:
    if not shutil.which('gio'):raise ValueError('gio required for recoverable DevState relocation')
    trash=lambda path:subprocess.run(['gio','trash','--',str(path)],check=True)
  stages=[]
  for source,destination,inputs,fresh in plans:
    if source!=destination or fresh:
      stage=backup/destination.parent.parent.name/destination.name
      if fresh:stage.mkdir(parents=True)
      else:shutil.copytree(source,stage)
      for name,downloaded in inputs:shutil.copy2(downloaded,stage/name)
      stages.append((destination,stage))
  for path in sorted(obsolete):
    if path.exists():trash(path)
  for destination,stage in stages:shutil.copytree(stage,destination)
  for source,destination,inputs,fresh in plans:
    if source==destination and not fresh:
      for name,downloaded in inputs:
        target=destination/name;temporary=target.with_name(target.name+'.readback-new')
        shutil.copy2(downloaded,temporary);temporary.replace(target)
  if document!=json.loads(original):
    temporary=table.with_name(table.name+'.new');temporary.write_text(json.dumps(document,ensure_ascii=False,indent=2)+'\n');temporary.replace(table)
    changes.append(str(table))
  return changes

def main():
  parser=argparse.ArgumentParser(description=__doc__)
  parser.add_argument('--apply',action='store_true',help='explicitly adopt downloaded Config/Channels/ServoSteps into DevState')
  parser.add_argument('--dashboard',default='http://192.168.99.51:8090')
  args=parser.parse_args()
  try:
    url=args.dashboard.rstrip('/')+'/api/device-registry';view=get_json(url)
    try:
      editor=get_json(args.dashboard.rstrip('/')+'/api/device-editor')
      if editor['Revision']!=view['Revision']:raise ValueError('Assignment changed; repeat')
      metadata={row['MAC']+'@'+str(row['ServiceId']):row for row in editor['Devices']}
      for row in view['Devices']:
        if row['DeviceId'] in metadata:row['Group']=metadata[row['DeviceId']]['Group']
    except urllib.error.HTTPError as error:
      if error.code!=404:raise
    snapshots=download_all(view)
    current=get_json(url)
    if current.get('Pending') or current.get('Revision')!=view['Revision']:raise ValueError('Assignment changed during readback; repeat')
    if args.apply:
      changes=adopt(view,snapshots);cleanup_readbacks()
      if changes:
        print('Geänderte Quelldateien:')
        for path in sorted(changes):print('  '+path)
        print('Betroffene FS-Abbilder vor einem Upload neu bauen.')
      else:print('Keine Änderungen an den Quelldateien.')
    else:print('Nur eingelesen; mit --apply vergleichen, Änderungen übernehmen und Rücklesekopien löschen.')
  except (OSError,ValueError,KeyError,subprocess.CalledProcessError) as error:raise SystemExit(str(error))

if __name__=='__main__':main()
