# EnergyHistory.py, Version: 1.09
"""Passive, timestamped power observations and explicitly bounded energy estimates."""
from __future__ import annotations
from CsvFiles import CsvFiles
from BootSession import BOOT_ID, latest_segment, last_row
import csv
import datetime as dt
import json
from Messages import ReadJson, WriteJson, ValidateType
import math
import re
from pathlib import Path
from typing import Any

HEADER = ('Time','Event','Profile','MainDevice','FloorDevice','TempC','TempValid',
          'RoomTempC','RoomTempValid','RoomAgeSeconds','MainState','MainReportedW',
          'MainPowerFactor','MainCorrectedW','MainPowerValid','MainAgeSeconds',
          'MainSourceTime','FloorSourceTime','MainBootCounter','FloorBootCounter',
          'FloorState','FloorNominalW','MainIntervalSeconds','MainIntervalWh',
          'FloorIntervalSeconds','FloorIntervalWh','MainIntegratedWh','FloorEstimatedWh',
          'MainCoveredSeconds','MainUnknownSeconds','FloorCoveredSeconds','FloorUnknownSeconds',
          'MainIntervalQuality','FloorIntervalQuality','PowerCalibration')


def number(value: Any) -> bool:
  return isinstance(value,(int,float)) and not isinstance(value,bool) and math.isfinite(value)


def role_matches(value, expected):
  return ('HeaterMain' if value == 'MainSw' else value) == expected


def channel(status: dict, name: str) -> bool | None:
  channels=status.get('Chs',[])
  rows = [row for row in (channels if isinstance(channels,list) else []) if isinstance(row,dict) and row.get('Id')==name]
  return rows[0].get('State') if len(rows)==1 and isinstance(rows[0].get('State'),bool) else None


def stamp() -> str:
  return dt.datetime.now(dt.timezone.utc).isoformat(timespec='milliseconds').replace('+00:00','Z')


class EnergyHistory:
  def __init__(self, root: Path, config: dict):
    self.csv = CsvFiles()
    self.root = root
    self.config = config
    self.maximum_gap = float(config.get('EnergyMaxIntervalSec',15))
    if not 10 <= self.maximum_gap <= 30: raise ValueError('EnergyMaxIntervalSeconds must be 10..30')
    self.floor_watts = config.get('FloorPowerW',{})
    self.factors = config.get('PowerFactors',{})
    if not isinstance(self.floor_watts,dict) or not isinstance(self.factors,dict):
      raise ValueError('FloorPowerWatts and PowerFactors must be objects')
    for value in self.floor_watts.values():
      if not number(value) or value<=0: raise ValueError('FloorPowerWatts must be positive numbers')
    for value in self.factors.values():
      if not number(value) or value<=0: raise ValueError('PowerFactors must be positive numbers')
    self.assignments: dict[str,dict] = {}
    self.sessions: dict[str,dict] = {}
    self.cache: dict[str,tuple[float,dict]] = {}
    self.needed=set()
    self.paused = False

  def configure(self, profiles: list[dict], times: dict, identities: dict[str,str]) -> None:
    assignments = {}
    room = identities.get(self.config.get('RoomTempSrc','GN-Pi'))
    for profile in profiles:
      roles = {}
      for role in ('HeaterMain','HeaterFloor'):
        found = []
        for host,state in times.get('Chs',{}).items():
          rows = state.get('Rows',[]) if isinstance(state,dict) else state
          if not isinstance(rows,list):continue
          service=identities.get(host)
          if not service:continue
          for row in rows:
            if not isinstance(row,dict) or not role_matches(row.get('Role'),role) or row.get('Prof')!=profile['ProfName']:continue
            position=re.fullmatch(r'Main ([1-8])',row.get('PhysPos',''))
            if position:
              endpoint=f'{service}/{position[1]}'
            elif service.endswith('@4'):
              endpoint=f'{service}/0'
            else:continue
            found.append((service,endpoint))
        if len(found)!=1 or not all(found[0]): break
        roles[role]=found[0]
      if len(roles)!=2: continue
      temperature=profile.get('MeasSrc')
      if not isinstance(temperature,str) or not re.fullmatch(r'(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}@[1-4](?:/[0-9]+)?',temperature):continue
      assignment = {'Prof':profile,'Main':roles['HeaterMain'],'Floor':roles['HeaterFloor'],
                    'Temp':temperature.split('/')[0],'Room':room,
                    'FloorW':self.floor_watts.get(profile['ProfName']),
                    'Factor':self.factors.get(roles['HeaterMain'][0],1.0)}
      assignment['Signature']=json.dumps(assignment,sort_keys=True,separators=(',',':'))
      old = self.assignments.get(profile['ProfName'])
      if old is None or old['Signature']!=assignment['Signature']:
        session=self.sessions.pop(profile['ProfName'],None)
        if session:self.csv.close(session['Path'])
      assignments[profile['ProfName']]=assignment
    for removed in self.sessions.keys()-assignments.keys():
      self.csv.close(self.sessions.pop(removed)['Path'])
    self.assignments=assignments
    needed={identity for a in assignments.values() for identity in (a['Main'][0],a['Floor'][0],a['Temp'],a['Room']) if identity}
    self.needed=needed
    self.cache={key:value for key,value in self.cache.items() if key in needed}
    self.paused=False

  def pause(self) -> None:
    self.paused=True
    for session in self.sessions.values():
      session['Bridges']={}
      for previous in session['Previous'].values(): previous['Valid']=False

  def _session(self, profile_id: str, assignment: dict) -> dict:
    previous=self.sessions.get(profile_id)
    if previous:return previous
    directory=self.root/profile_id/'Energy'
    # Runtime routes hold (device, endpoint); metadata stores the endpoint.
    recorded_assignment=dict(assignment, Main=assignment['Main'][1], Floor=assignment['Floor'][1])
    path=latest_segment(directory,'.energy.csv',{'HostBootId':BOOT_ID,'Assignment':recorded_assignment})
    if path:
      row=last_row(path,HEADER)
      session={'Path':path,'Previous':{}}
      for key,column in (('MainWh','MainIntegratedWh'),('FloorWh','FloorEstimatedWh'),
                         ('MainCovered','MainCoveredSeconds'),('MainUnknown','MainUnknownSeconds'),
                         ('FloorCovered','FloorCoveredSeconds'),('FloorUnknown','FloorUnknownSeconds')):
        session[key]=float(row[column]) if row else 0.
      self.sessions[profile_id]=session
      return session
    directory=self.root/profile_id/'Energy';directory.mkdir(parents=True,exist_ok=True)
    prefix=dt.datetime.now(dt.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    path=directory/(prefix+'.energy.csv')
    metadata={'HostBootId':BOOT_ID,'Ver':1,'StartedAt':stamp(),'Assignment':recorded_assignment,
              'Integration':'Main: trapezoidal reported watts; floor: previous reported state times measured nominal watts. Missing readings held provisionally for at most 60 s, corrected linearly on recovery within 60 s.',
              'Limitations':'Not a hardware energy counter. Switching within the sampling period and meter-window alignment are not exactly reconstructed. Held/interpolated energy is estimated, not a fresh measurement. Boot/clock discontinuities and gaps beyond 60 s remain unknown. Corrections are appended as signed interval energy; raw samples are not rewritten.',
              'PowerCalAtStart':self.config.get('PowerCalAtStart',{}),
              'PowerCal':'provisional until calibration periods/factors are confirmed'}
    path.with_suffix('.json').write_text(WriteJson('TJsonHistoryMetadata',metadata,indent=2)+'\n',encoding='utf-8')
    with path.open('x',encoding='utf-8',newline='') as f:
      csv.writer(f,delimiter=';').writerow(HEADER)
    session={'Path':path,'Previous':previous['Previous'] if previous else {},'MainWh':0.,'FloorWh':0.,
             'MainCovered':0.,'MainUnknown':0.,'FloorCovered':0.,'FloorUnknown':0.}
    self.sessions[profile_id]=session
    return session

  def _snapshot(self, identity: str | None, now: float) -> tuple[dict,float | None]:
    value=self.cache.get(identity)
    if value is None: return {},None
    age=now-value[0]
    return (value[1] if 0<=age<=self.maximum_gap else {}),age

  def ingest(self, packet: dict, now: float, wall: float) -> None:
    identity=packet['ID'];status=packet['Status']
    if identity not in self.needed:return
    self.cache[identity]=(now,status)
    if self.paused: return
    for profile_id,a in self.assignments.items():
      event=[]
      if identity==a['Main'][0]:event.append('Main')
      if identity==a['Floor'][0]:event.append('Floor')
      if not event:continue
      s=self._session(profile_id,a)
      main,main_age=self._snapshot(a['Main'][0],now)
      floor,_=self._snapshot(a['Floor'][0],now)
      temp,_=self._snapshot(a['Temp'],now)
      room,room_age=self._snapshot(a['Room'],now)
      measurements=room.get('Meas',[])
      room_rows=[r for r in (measurements if isinstance(measurements,list) else []) if isinstance(r,dict) and r.get('Kind')=='RoomTemperature']
      room_row=room_rows[0] if len(room_rows)==1 else {}
      room_value=room_row.get('Value');room_valid=room_row.get('Valid') is True and number(room_value)
      kind=a['Prof'].get('Meas','Temperature')
      temp_value=temp.get({'Temperature':'Temp','BatteryPercent':'BatteryPct'}.get(kind,kind));temp_valid=number(temp_value)
      meter=main.get('PowerMeter',{});meter=meter if isinstance(meter,dict) else {}
      reported=meter.get('ActivePower');main_state=channel(main,a['Main'][1]);floor_state=channel(floor,a['Floor'][1])
      main_time=main.get('CurrentTime')
      main_valid=meter.get('Valid') is True and number(reported) and reported>=0 and main_state is not None and main.get('TimeSynchronized') is True and number(main_time) and -3<=wall-main_time<=self.maximum_gap
      corrected=reported*a['Factor'] if main_valid else None
      if corrected is not None and not math.isfinite(corrected):main_valid=False;corrected=None
      intervals={'Main':(None,None,''),'Floor':(None,None,'')}
      for role in event:
        source=main if role=='Main' else floor
        state=main_state if role=='Main' else floor_state
        watts=corrected if role=='Main' else (a['FloorW']*int(state) if a['FloorW'] is not None and state is not None else None)
        boot=source.get('BootCnt')
        source_time=source.get('CurrentTime')
        valid=watts is not None and number(watts) and isinstance(boot,int) and not isinstance(boot,bool) and number(source_time) and source.get('TimeSynchronized') is True
        if valid and not -3<=wall-source_time<=self.maximum_gap:valid=False
        current={'Now':now,'SourceTime':source_time,'Boot':boot,'State':state,'Watts':watts,'Valid':valid}
        previous=s['Previous'].get(role);elapsed=None;energy=None;quality='first_sample'
        bridges=s.setdefault('Bridges',{})
        anchor=bridges.get(role)
        if previous:
          elapsed=now-previous['Now']
          restart=anchor is not None and isinstance(boot,int) and boot!=anchor['Boot']
          clock_error=valid and anchor is not None and source_time>anchor['SourceTime'] and abs((source_time-anchor['SourceTime'])-(now-anchor['Now']))>2
          if elapsed<=0 or restart or clock_error or (anchor is None and not previous['Valid']):
            quality='device_restart' if restart else 'duplicate_or_clock_or_delivery_gap' if clock_error or elapsed<=0 else 'invalid_or_stale'
            if elapsed>0:s[role+'Unknown']+=elapsed
            bridges.pop(role,None);anchor=None
          elif anchor is not None:
            span=now-anchor['Now']
            fresh=valid and source_time>anchor['SourceTime']
            covered=max(0.,min(now,anchor['Now']+60)-previous['Now'])
            if covered>0:
              energy=anchor['Watts']*covered/3600
              anchor['ProvisionalWh']+=energy
              s[role+'Covered']+=covered
              quality='held'
            if elapsed>covered:
              s[role+'Unknown']+=elapsed-covered
              quality='held' if covered>0 else 'gap'
            if fresh and 0<span<=60:
              bridged=anchor['Held'] or span>self.maximum_gap
              target=((anchor['Watts']+watts)/2 if role=='Main' or bridged else anchor['Watts'])*span/3600
              # Include the signed correction in this row, preserving all raw observations.
              correction=target-anchor['ProvisionalWh']
              energy=(energy or 0.)+correction
              quality='interpolated' if bridged else 'valid'
            if energy is not None:s[role+'Wh']+=energy
            if not fresh:anchor['Held']=True
        # Only a new, synchronized observation may establish or advance the anchor.
        if valid and (anchor is None or source_time>anchor['SourceTime']):
          bridges[role]={**current,'ProvisionalWh':0.,'Held':False}
        s['Previous'][role]=current
        intervals[role]=(elapsed,energy,quality)
      def value(v):return '' if v is None else (int(v) if isinstance(v,bool) else v)
      mi,fi=intervals['Main'],intervals['Floor']
      row=[stamp(),'+'.join(event),profile_id,a['Main'][0],a['Floor'][0],temp_value if temp_valid else None,temp_valid,
           room_value if room_valid else None,room_valid,room_age,main_state,reported if number(reported) else None,
           a['Factor'],corrected,main_valid,main_age,main.get('CurrentTime'),floor.get('CurrentTime'),
           main.get('BootCnt'),
           floor.get('BootCnt'),
           floor_state,a['FloorW'],mi[0],mi[1],fi[0],fi[1],
           s['MainWh'],s['FloorWh'],s['MainCovered'],s['MainUnknown'],s['FloorCovered'],s['FloorUnknown'],mi[2],fi[2],'provisional']
      self.csv.write(s['Path'],map(value,row))
