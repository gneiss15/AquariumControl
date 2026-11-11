# BrowserServer.py, Version: 1.08
"""Static browser charts and bounded, passive JSON history queries. No plot generation."""
from __future__ import annotations
import csv,datetime as dt,gzip,math,re,threading,uuid,zlib
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from urllib.parse import urlsplit,parse_qs
from Messages import ReadJson, WriteJson
from CsvStreaming import csv_time_key, select_files, stream_file, chunks
from EnergyHistory import HEADER as ENERGY_HEADER
from PiHistory import HEADER as PI_HEADER

UTC=dt.timezone.utc
RANGES={'1h':3600,'6h':21600,'1day':86400,'7days':604800,'all':None}
ASSETS=Path(__file__).resolve().parent

def instant(text):
  value=dt.datetime.fromisoformat(text.replace('Z','+00:00'))
  if value.tzinfo is None:raise ValueError('History timestamps must include timezone')
  return value.astimezone(UTC)

def timestamp(value):return value.isoformat(timespec='milliseconds').replace('+00:00','Z')

def active_since(root):
  found=[]
  for path in sorted(root.glob('HeatProfileAq*/Pi/*.pi.csv')):
    with path.open(newline='') as stream:
      reader=csv.DictReader(stream,delimiter=';')
      if tuple(reader.fieldnames or ())!=PI_HEADER:continue
      row=next(reader,None)
      if row and row.get('Mode')=='Active':found.append(instant(row['Time']))
  # The normal energy snapshots also retain the active profile, independently of PI debug.
  if not found:
    for path in sorted(root.glob('HeatProfileAq*/Energy/*.energy.csv')):
      info=metadata(path);profile=info.get('Assignment',{}).get('Prof',{})
      if profile.get('Heat',{}).get('Active') is not True:continue
      with path.open(newline='') as stream:
        row=next(csv.DictReader(stream,delimiter=';'),None)
        if row and row.get('Time'):found.append(instant(row['Time']))
  return min(found) if found else None

def rows(path,header,start,end):
  # Closed daily files before the query need no complete CSV scan.
  with path.open('rb') as tail:
    tail.seek(0,2);size=tail.tell();tail.seek(max(0,size-8192))
    block=tail.read();lines=block.splitlines()
    if block and not block.endswith(b'\n'):lines=lines[:-1]
    for line in reversed(lines):
      try:last=instant(line.split(b';',1)[0].decode('ascii'))
      except (ValueError,UnicodeError):continue
      if last<start:return
      break
  with path.open(newline='',encoding='utf-8-sig') as stream:
    reader=csv.DictReader(stream,delimiter=';')
    if tuple(reader.fieldnames or ())!=header:raise ValueError('Invalid history CSV header: '+path.name)
    for row in reader:
      # A live writer may still be appending its last row. Never invent missing cells.
      if None in row or any(value is None for value in row.values()):continue
      when=instant(row['Time'])
      if start<=when<=end:yield row,when

def number(row,key,valid=True):
  if not valid or not row.get(key):return None
  value=float(row[key]);return value if math.isfinite(value) else None

def metadata(path):
  try:
    value=ReadJson('TJsonHistoryMetadata',path.with_suffix('.json').read_text())
    return value if isinstance(value,dict) else {}
  except (OSError,ValueError):return {}

def target(config):
  value=config.get('Setpoint') if isinstance(config,dict) else None
  return value if isinstance(value,(int,float)) and not isinstance(value,bool) and math.isfinite(value) else None

def history_bounds(root,choice,now,since,retention_days,range_from=None,range_to=None):
  if choice not in RANGES:raise ValueError('Unknown range')
  if (range_from is None)!=(range_to is None):raise ValueError('Both range bounds are required')
  if range_from is not None and choice!='all':raise ValueError('Custom bounds require range=all')
  now=now or dt.datetime.now(UTC)
  origin=instant(since) if since else active_since(root)
  minimum=max(origin or now,now-dt.timedelta(days=retention_days))
  if range_from is None:
    start=minimum
    end=now
    if RANGES[choice] is not None:start=max(start,now-dt.timedelta(seconds=RANGES[choice]))
  else:
    try:
      requested_start=instant(range_from)
      requested_end=instant(range_to)
    except (TypeError,ValueError) as error:
      raise ValueError('Invalid custom history bounds') from error
    start=max(requested_start,minimum)
    end=min(requested_end,now)
    if start>end:raise ValueError('History range is empty after activation/retention limits')
  return start,end,origin

def history(root,choice='1day',now=None,since=None,retention_days=30,range_from=None,range_to=None):
  now=now or dt.datetime.now(UTC)
  start,end,origin=history_bounds(root,choice,now,since,retention_days,range_from,range_to)
  profiles=[]
  if origin is None:
    return {'Ver':1,'Range':choice,'From':timestamp(start),'To':timestamp(end),'AvailableFrom':None,'RetentionDays':retention_days,'Profs':[]}
  for directory in sorted(root.glob('HeatProfileAq*')):
    if not directory.is_dir() or not re.fullmatch(r'HeatProfileAq[0-9]+',directory.name):continue
    record={'Id':directory.name,'Temp':[],'Energy':[],'Heat':[]}
    for path in sorted((directory/'Energy').glob('*.energy.csv')):
      info=metadata(path);setpoint=target(info.get('Assignment',{}).get('Prof',{}).get('Heat',{}))
      boundary=True
      for row,when in rows(path,ENERGY_HEADER,start,end):
        floor=int(row['FloorState']) if row['FloorState'] in ('0','1') else None
        main=int(row['MainState']) if row['MainState'] in ('0','1') else None
        nominal=number(row,'FloorNominalW')
        mi=number(row,'MainIntervalWh',row['MainIntervalQuality'] in ('valid','held','interpolated'))
        fi=number(row,'FloorIntervalWh',row['FloorIntervalQuality'] in ('valid','held','interpolated'))
        # Do not count an interval reaching outside the selected time range.
        for role in ('Main','Floor'):
          seconds=number(row,role+'IntervalSeconds')
          if seconds is not None and when-dt.timedelta(seconds=seconds)<start:
            if role=='Main':mi=None
            else:fi=None
        record['Energy'].append([timestamp(when),number(row,'TempC',row['TempValid']=='1'),
          number(row,'RoomTempC',row['RoomTempValid']=='1'),floor,main,
          number(row,'MainCorrectedW',row['MainPowerValid']=='1'),nominal*floor if nominal is not None and floor is not None else None,
          mi,fi,(row['MainPowerValid']=='1' or row['MainIntervalQuality'] in ('held','interpolated')) and row['MainIntervalQuality'] in ('','valid','held','interpolated'),
          (floor is not None or row['FloorIntervalQuality'] in ('held','interpolated')) and row['FloorIntervalQuality'] in ('','valid','held','interpolated'),boundary,setpoint])
        boundary=False
    for path in sorted((directory/'Pi').glob('*.pi.csv')):
      setpoint=target(metadata(path).get('Cfg'));boundary=True
      for row,when in rows(path,PI_HEADER,start,end):
        if row['Mode']!='Active':continue
        record['Heat'].append([timestamp(when)]+[number(row,key,row['Valid']=='1') for key in
          ('DemandW','ActualWatts','FloorDemandW','MainDemandW','ProportionalW','IntegralW','FeedForwardW')]+[setpoint,boundary])
        boundary=False
    if not record['Energy']:
      for path in sorted(directory.glob('*.csv')):
        for row,when in rows(path,('Time','Temp','Floor','Main'),start,end):
          record['Temp'].append([timestamp(when),number(row,'Temp'),int(row['Floor']),int(row['Main'])])
    for key in ('Energy','Heat','Temp'):record[key].sort(key=lambda row:row[0])
    if any(record[key] for key in ('Energy','Heat','Temp')):profiles.append(record)
  return {'Ver':1,'Range':choice,'From':timestamp(start),'To':timestamp(end),
          'AvailableFrom':timestamp(origin) if origin else None,'RetentionDays':retention_days,'Profs':profiles}


# Keep the buffered helper for existing offline callers; HTTP uses the iterator below.
def selected_csv(path,header,start,end):
  entries=select_files([path],header,start,end)
  return b''.join(stream_file(entries[0],end)) if entries else None

def csv_history_stream(root,choice='1day',now=None,since=None,retention_days=30,range_from=None,range_to=None):
  now=now or dt.datetime.now(UTC)
  start,end,origin=history_bounds(root,choice,now,since,retention_days,range_from,range_to)
  manifest={'Ver':2,'Range':choice,'From':timestamp(start),'To':timestamp(end),
    'AvailableFrom':timestamp(origin) if origin else None,'RetentionDays':retention_days,'Files':[]}
  parts=[]
  if origin is not None:
    low=start.strftime('%Y-%m-%dT%H:%M:%S.%f').encode();high=end.strftime('%Y-%m-%dT%H:%M:%S.%f').encode()
    for directory in sorted(root.glob('HeatProfileAq*')):
      if directory.is_symlink() or not directory.is_dir() or not re.fullmatch(r'HeatProfileAq[0-9]+',directory.name):continue
      energy=False
      for kind,folder,pattern,header in [('Energy',directory/'Energy','*.energy.csv',ENERGY_HEADER),('Pi',directory/'Pi','*.pi.csv',PI_HEADER),('Temperature',directory,'*.csv',('Time','Temp','Floor','Main'))]:
        if kind=='Temperature' and energy:continue
        if folder.is_symlink():continue
        for entry in select_files(folder.glob(pattern),header,low,high):
          info=metadata(entry['Path'])
          config=info.get('Assignment',{}).get('Prof',{}).get('Heat',{}) if kind=='Energy' else info.get('Cfg')
          manifest['Files'].append({'Id':str(len(parts)),'Prof':directory.name,'Kind':kind,'Setpoint':target(config)})
          parts.append(entry)
          if kind=='Energy':energy=True
  boundary='HeatHistoryCSV-'+uuid.uuid4().hex
  def payload():
    yield ('--'+boundary+'\r\nContent-Type: application/json\r\n\r\n').encode()
    yield WriteJson('THttpHistoryManifest',manifest,separators=(',',':')).encode()
    for index,entry in enumerate(parts):
      yield ('\r\n--'+boundary+'\r\nContent-Type: text/csv; charset=utf-8\r\nContent-ID: '+str(index)+'\r\n\r\n').encode()
      yield from stream_file(entry,high)
    yield ('\r\n--'+boundary+'--\r\n').encode()
  return chunks(payload()),'multipart/mixed; boundary='+boundary

def csv_history(root,choice='1day',now=None,since=None,retention_days=30,range_from=None,range_to=None):
  parts,kind=csv_history_stream(root,choice,now,since,retention_days,range_from,range_to)
  return b''.join(parts),kind

def encoded_chunks(parts,compressed):
  if not compressed:
    yield from parts;return
  compressor=zlib.compressobj(3,zlib.DEFLATED,31)
  for part in parts:
    value=compressor.compress(part)
    if value:yield value
  final=compressor.flush()
  if final:yield final

def accepts_gzip(value):
  accepted={}
  for token in value.lower().split(','):
    fields=token.strip().split(';');quality=1.
    for field in fields[1:]:
      if field.strip().startswith('q='):
        try:quality=float(field.strip()[2:])
        except ValueError:quality=0.
    accepted[fields[0]]=quality
  return accepted.get('gzip',accepted.get('*',0))>0


def chart_server(data_dir,host,port,since=None,retention_days=30):
  if not 1<=retention_days<=366:raise ValueError('RetentionDays must be 1..366')
  if since:instant(since)
  gate=threading.BoundedSemaphore(1)
  class Handler(BaseHTTPRequestHandler):
    def do_GET(self):self.respond(False)
    def do_HEAD(self):self.respond(True)
    def respond(self,head):
      parsed=urlsplit(self.path);status=200;compressed=False;streaming=None;locked=False
      assets={'/':'Charts.html','/HeatHistoryCharts.html':'Charts.html','/HeatHistory.js':'HeatHistory.js',
        '/Messages.js':'Messages.js','/Versions.txt':'Versions.txt'}
      if parsed.path in assets:
        path=ASSETS/assets[parsed.path]
        body=path.read_bytes()
        kind='text/javascript' if path.suffix=='.js' else 'text/plain' if path.suffix=='.txt' else 'text/html'
      elif parsed.path in ('/api/history','/api/history.csv'):
        if not gate.acquire(blocking=False):self.send_error(503,'History request already in progress');return
        locked=True
        try:
          parameters=parse_qs(parsed.query,keep_blank_values=True)
          if set(parameters)-{'range','from','to'} or any(len(values)!=1 for values in parameters.values()):raise ValueError('Invalid history query')
          choice=parameters.get('range',['1day'])[0]
          range_from=parameters.get('from',[None])[0]
          range_to=parameters.get('to',[None])[0]
          if parsed.path.endswith('.csv'):
            streaming,kind=csv_history_stream(Path(data_dir),choice,since=since,retention_days=retention_days,range_from=range_from,range_to=range_to)
          else:
            # Compatibility for existing migration/diagnostic clients, not the browser path.
            document=history(Path(data_dir),choice,since=since,retention_days=retention_days,range_from=range_from,range_to=range_to)
            body=WriteJson('THttpHistoryReply',document,separators=(',',':')).encode();kind='application/json'
        except (OSError,ValueError,KeyError) as error:
          status=400 if isinstance(error,ValueError) and (str(error).startswith('Unknown range') or str(error).startswith('Invalid history query') or str(error).startswith('Invalid custom history bounds') or 'range=' in str(error) or 'range is empty' in str(error) or 'bounds' in str(error)) else 503
          body=WriteJson('TUdpLastError',{'Message':str(error)}).encode();kind='application/json'
        finally:
          if streaming is None:gate.release();locked=False
      else:self.send_error(404);return
      compressed=accepts_gzip(self.headers.get('Accept-Encoding',''))
      if streaming is not None:
        # HTTP/1.0 close-delimited response avoids buffering for Content-Length.
        self.close_connection=True
        try:
          self.send_response(status);self.send_header('Content-Type',kind)
          self.send_header('Cache-Control','no-store');self.send_header('Vary','Accept-Encoding')
          self.send_header('Connection','close')
          if compressed:self.send_header('Content-Encoding','gzip')
          self.end_headers()
          if not head:
            for part in encoded_chunks(streaming,compressed):self.wfile.write(part)
        except (BrokenPipeError,ConnectionResetError):pass
        finally:
          streaming.close()
          if locked:gate.release()
        return
      if compressed:body=gzip.compress(body,compresslevel=3)
      self.send_response(status);self.send_header('Content-Type',kind+'; charset=utf-8')
      self.send_header('Content-Length',str(len(body)));self.send_header('Cache-Control','no-store');self.send_header('Vary','Accept-Encoding')
      if compressed:self.send_header('Content-Encoding','gzip')
      self.end_headers()
      if not head:self.wfile.write(body)
  return ThreadingHTTPServer((host,port),Handler)
