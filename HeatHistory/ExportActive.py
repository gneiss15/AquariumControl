#!/usr/bin/env python3
# ExportActive.py, Version: 1.02
"""Copy complete active-PI rows, leaving the source runtime untouched."""
from Messages import ReadJson, WriteJson
import argparse,csv,datetime as dt,json,shutil
from pathlib import Path
from BrowserServer import active_since,instant,timestamp

def export(source,target):
  source=Path(source);target=Path(target)
  if target.exists():raise ValueError('Export destination must not exist')
  config=ReadJson('TJsonHeatHistoryConfig',(source/'config.json').read_text())
  data=Path(config.get('DataDir','data'))
  if not data.is_absolute():data=source/data
  origin=instant(config['ActiveFrom']) if config.get('ActiveFrom') else active_since(data)
  if origin is None:raise ValueError('No recorded active PI start; export cancelled')
  cutoff=max(origin,dt.datetime.now(dt.timezone.utc)-dt.timedelta(days=30))
  target.mkdir(parents=True)
  copied=0;counts={}
  for directory in sorted(data.glob('HeatProfileAq*')):
    if not directory.name.removeprefix('HeatProfileAq').isdigit() or directory.is_symlink():continue
    for sub,pattern in [(directory,'*.csv'),(directory/'Energy','*.energy.csv'),(directory/'Pi','*.pi.csv')]:
      if sub.is_symlink():raise ValueError('Symlink history directory refused')
      for path in sorted(sub.glob(pattern)):
        if path.is_symlink():raise ValueError('Symlink history file refused')
        with path.open(newline='') as stream:
          reader=csv.DictReader(stream,delimiter=';');header=reader.fieldnames
          selected=[row for row in reader if row.get('Time') and None not in row and all(v is not None for v in row.values()) and instant(row['Time'])>=cutoff]
        if not selected:continue
        destination=target/'data'/path.relative_to(data);destination.parent.mkdir(parents=True,exist_ok=True)
        with destination.open('x',newline='') as stream:
          writer=csv.DictWriter(stream,fieldnames=header,delimiter=';');writer.writeheader();writer.writerows(selected)
        metadata=path.with_suffix('.json')
        if metadata.is_file() and not metadata.is_symlink():shutil.copy2(metadata,destination.with_suffix('.json'))
        copied+=len(selected);counts[directory.name]=counts.get(directory.name,0)+len(selected)
  if not counts.get('HeatProfileAq1') or not counts.get('HeatProfileAq2'):raise ValueError('Both aquarium histories are required')
  # Copy only known application settings, never unrelated/private configuration fields.
  keys=('DashboardUrl','StatusMaxAgeSec','FloorPowerW','RoomTempSrc','PowerFactors','EnergyMaxIntervalSec')
  settings={key:config[key] for key in keys if key in config}
  settings.update(DataDir='data',ActiveFrom=timestamp(origin),ViewRetentionDays=30,StorageRetentionDays=30,StorageMaxBytes=512*1024*1024,StorageDeleteExpired=False)
  (target/'config.json').write_text(WriteJson('TJsonHeatHistoryConfig',settings,indent=2)+'\n')
  result={'ExportedAt':timestamp(dt.datetime.now(dt.timezone.utc)),'ActiveFrom':timestamp(origin),'CopiedFrom':timestamp(cutoff),'Rows':copied,'Profs':counts}
  return result

if __name__=='__main__':
  parser=argparse.ArgumentParser();parser.add_argument('source',type=Path);parser.add_argument('target',type=Path);args=parser.parse_args()
  print(json.dumps(export(args.source,args.target),indent=2))
