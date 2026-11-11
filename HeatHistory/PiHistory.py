# PiHistory.py, Version: 1.08
"""Record controller telemetry separately from unchanged energy CSVs."""
from CsvFiles import CsvFiles
from BootSession import BOOT_ID, latest_segment, last_row
import csv
import datetime as dt
import math
import json
from Messages import ReadJson, WriteJson, ValidateType
from pathlib import Path
FIELDS=('Valid','Reason','Learning','Temperature','ErrorK','ProportionalW','IntegralW','FeedForwardW',
        'DemandW','FloorDemandW','MainDemandW','FloorRequest','MainRequest','FloorAvailable',
        'ActualWatts','CapacityWhPerK','LossWattsPerK','Kp','TiSeconds','Samples','LearnedWindows','ActualFloorOn','LearningReason','WindowAgeSeconds')
HEADER=('Time','Device','Profile','Mode','ActualMainOn')+FIELDS
# Keep the established CSV column names when recording the short message fields.
FIELD_KEYS={
  'Learning': 'Learn',
  'LearningReason': 'LearnReason',
  'Temperature': 'Temp',
  'ErrorK': 'ErrK',
  'ActualWatts': 'ActualW',
  'LossWattsPerK': 'LossWPerK',
  'TiSeconds': 'TiSec',
  'WindowAgeSeconds': 'WindowAgeSec',
}
class PiHistory:
  def __init__(self,root):
    self.root=Path(root);self.files={};self.assignments={};self.csv=CsvFiles()
  def configure(self,assignments):
    updated={key:tuple(value['Main']) for key,value in assignments.items()}
    for key in list(self.files):
      if self.assignments.get(key[0])!=updated.get(key[0]):
        self.csv.close(self.files.pop(key)[0])
    self.assignments=updated
  def ingest(self,packet):
    status=packet.get('Status',{})
    if not isinstance(status,dict):return
    pi=status.get('HeatDiag')
    if not isinstance(pi,dict) or pi.get('Ver')!=1 or pi.get('Active') is not True:return
    profile=pi.get('Prof');assignment=self.assignments.get(profile)
    if assignment is None or assignment[0]!=packet.get('ID'):return
    if not isinstance(pi.get('Samples'),int) or isinstance(pi.get('Samples'),bool):return
    boot=status.get('BootCnt')
    now=dt.datetime.now(dt.timezone.utc)
    key=(profile,packet['ID'],boot,'Active',json.dumps(pi.get('Cfg'),sort_keys=True),BOOT_ID);entry=self.files.get(key)
    if entry and entry[1]==pi['Samples']:return
    if not entry:
      expected={'HostBootId':BOOT_ID,'Dev':packet['ID'],'BootCnt':boot,
                'Prof':profile,'Cfg':pi.get('Cfg'),'Active':True}
      resumed=latest_segment(self.root/profile/'Pi','.pi.csv',expected)
      if resumed:
        row=last_row(resumed,HEADER)
        entry=(resumed,int(row['Samples']) if row else None)
        if entry[1]==pi['Samples']:
          self.files[key]=entry
          return
    if not entry:
      path=self.root/profile/'Pi'/ (now.strftime('%Y%m%dT%H%M%S%fZ')+'.pi.csv')
      path.parent.mkdir(parents=True,exist_ok=True)
      metadata=WriteJson('TJsonHistoryMetadata',dict(expected,Ver=1,StartedAt=now.isoformat()),indent=2)
      path.with_suffix('.json').write_text(metadata+'\n',encoding='utf-8')
      with path.open('x',newline='') as f:csv.writer(f,delimiter=';').writerow(HEADER)
    else:path=entry[0]
    channels=status.get('Chs',[])
    if not isinstance(channels,list):channels=[]
    actual=[r.get('State') for r in channels if isinstance(r,dict) and r.get('Id')==assignment[1]]
    def cell(value):
      if isinstance(value,bool):return int(value)
      if isinstance(value,float) and not math.isfinite(value):return ''
      return value if isinstance(value,(int,float,str)) else ''
    row=[now.isoformat(timespec='milliseconds'),packet['ID'],profile,'Active',cell(actual[0]) if len(actual)==1 else '']
    row.extend(cell(pi.get(FIELD_KEYS.get(field,field))) for field in FIELDS)
    for old_key,old in list(self.files.items()):
      if old_key[0]==profile and old[0]!=path:self.csv.close(old[0])
    self.csv.write(path,row)
    self.files={k:v for k,v in self.files.items() if k[0]!=profile}
    self.files[key]=(path,pi['Samples'])
