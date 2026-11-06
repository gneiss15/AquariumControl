# CsvStreaming.py, Version: 1.00
"""Bounded CSV record selection and streaming; no conversion of measurement cells."""
import datetime as dt
import re

CSV_TIME=re.compile(rb'^"?(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})(?:\.(\d{1,6}))?(Z|[+-]\d{2}:\d{2})"?$')
FILE_TIME=re.compile(r'^(\d{8}T\d{6})(\d{0,6})Z(?:[_.]|$)')

def csv_time_key(cell):
  match=CSV_TIME.fullmatch(cell)
  if not match:return None
  if match[3] not in (b'Z',b'+00:00'):
    value=dt.datetime.fromisoformat(cell.strip(b'"').decode('ascii').replace('Z','+00:00'))
    return value.astimezone(dt.timezone.utc).strftime('%Y-%m-%dT%H:%M:%S.%f').encode()
  return match[1]+b'.'+(match[2] or b'').ljust(6,b'0')

def filename_key(path):
  match=FILE_TIME.match(path.name)
  if not match:return None
  value=dt.datetime.strptime(match[1],'%Y%m%dT%H%M%S')
  return value.strftime('%Y-%m-%dT%H:%M:%S').encode()+b'.'+match[2].ljust(6,'0').encode()

def file_order(path):return (filename_key(path) or b'',path.name)

def records(stream,limit):
  """Yield complete records and offsets from a fixed snapshot, preserving original bytes."""
  record=[];quoted=False;begin=cursor=stream.tell()
  while cursor<limit:
    line=stream.readline(limit-cursor)
    if not line:break
    cursor+=len(line);record.append(line)
    if line.count(b'"')%2:quoted=not quoted
    if not quoted and line.endswith(b'\n'):
      yield begin,b''.join(record)
      begin=cursor;record=[]
  # Never emit a partial line or an unfinished quoted multiline record.

def inspect_file(path,header):
  if path.is_symlink():return None
  with path.open('rb') as stream:
    stream.seek(0,2);limit=stream.tell();stream.seek(0)
    first=stream.readline()
    if tuple(first.decode('utf-8-sig').strip().split(';'))!=header:
      raise ValueError('Invalid history CSV header: '+path.name)
    begin=stream.tell()
    first_record=next(records(stream,limit),None)
    if first_record is None:return None
    low=csv_time_key(first_record[1].split(b';',1)[0])
    if low is None:raise ValueError('Invalid history CSV timestamp: '+path.name)
    return {'Path':path,'Header':first,'Begin':begin,'Limit':limit,'First':low}

def select_files(paths,header,start,end):
  """Filename intervals select candidates; only Start/End inspect row times."""
  ordered=sorted((path for path in paths if not path.is_symlink()),key=file_order)
  if any(filename_key(path) is None for path in ordered):
    raise ValueError('History filenames require UTC timestamp prefixes; normalize them before serving')
  named=[(path,filename_key(path)) for path in ordered]
  following={path:(named[index+1][1] if index+1<len(named) else None)
             for index,(path,_) in enumerate(named)}
  selected=[]
  for path in ordered:
    first=filename_key(path);next_start=following.get(path)
    if first>end or (next_start is not None and next_start<start):continue
    entry=inspect_file(path,header)
    if entry is None or entry['First']>end:continue
    if entry['First']<start:
      with path.open('rb') as stream:
        stream.seek(entry['Begin']);found=False
        for offset,record in records(stream,entry['Limit']):
          key=csv_time_key(record.split(b';',1)[0])
          if key is None:raise ValueError('Invalid history CSV timestamp: '+path.name)
          if key<start:continue
          if key>end:break
          entry['Begin']=offset;found=True;break
        if not found:continue
    # Next filename bounds a closed Mid segment; the last file uses End filtering.
    entry['Mode']='Mid' if next_start is not None and next_start<=end else 'End'
    selected.append(entry)
  return selected

def stream_file(entry,end):
  yield entry['Header']
  with entry['Path'].open('rb') as stream:
    stream.seek(entry['Begin'])
    for _,record in records(stream,entry['Limit']):
      if entry['Mode']=='End':
        key=csv_time_key(record.split(b';',1)[0])
        if key is None:raise ValueError('Invalid history CSV timestamp: '+entry['Path'].name)
        if key>end:break
      yield record

def chunks(parts,size=65536):
  buffer=bytearray()
  for part in parts:
    buffer.extend(part)
    while len(buffer)>=size:
      yield bytes(buffer[:size]);del buffer[:size]
  if buffer:yield bytes(buffer)
