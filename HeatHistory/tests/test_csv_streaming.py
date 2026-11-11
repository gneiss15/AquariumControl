# test_csv_streaming.py, Version: 1.00
import csv,datetime as dt,gzip,io,pathlib,sys,tempfile,threading,unittest,urllib.request
from unittest.mock import patch
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from CsvStreaming import select_files,stream_file,records
from BrowserServer import chart_server,accepts_gzip
HEADER=('Time','Reason')

def key(hour):return f'2026-11-01T{hour:02d}:00:00.000000'.encode()

class StreamingTests(unittest.TestCase):
  def setUp(self):
    self.tmp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.tmp.name)
  def tearDown(self):self.tmp.cleanup()
  def file(self,name,hours):
    path=self.root/name
    with path.open('w',newline='') as f:
      writer=csv.writer(f,delimiter=';');writer.writerow(HEADER)
      for hour in hours:writer.writerow([key(hour).decode()+'Z','reason'])
    return path
  def test_start_mid_end_exclusion_and_inclusive_bounds(self):
    self.file('20261031T000000Z.csv',[0,1])
    first=self.file('20261101T000000Z.csv',[2,3,4])
    middle=self.file('20261101T050000Z.csv',[5,6])
    last=self.file('20261101T070000Z.csv',[7,8,9])
    self.file('20261101T100000Z.csv',[10,11])
    selected=select_files(self.root.glob('*.csv'),HEADER,key(3),key(8))
    self.assertEqual([v['Path'] for v in selected],[first,middle,last])
    self.assertEqual([v['Mode'] for v in selected],['Mid','Mid','End'])
    result=[]
    for entry in selected:
      raw=b''.join(stream_file(entry,key(8)))
      result.extend([row[0] for row in list(csv.reader(io.StringIO(raw.decode()),delimiter=';'))[1:]])
    self.assertEqual(result,[key(hour).decode()+'Z' for hour in range(3,9)])
  def test_filename_exclusion_happens_before_opening_csv(self):
    old=self.file('20261031T000000Z.csv',[0])
    valid=self.file('20261101T000000Z.csv',[3,4])
    future=self.file('20261101T100000Z.csv',[10])
    old.write_text('not a CSV header');future.write_text('not a CSV header')
    entries=select_files([future,valid,old],HEADER,key(3),key(4))
    self.assertEqual([entry['Path'] for entry in entries],[valid])

  def test_single_file_both_boundaries_and_empty_window(self):
    self.file('20261101T000000Z.csv',[0,2,4,6])
    entries=select_files(self.root.glob('*.csv'),HEADER,key(2),key(4))
    self.assertEqual(len(entries),1)
    raw=b''.join(stream_file(entries[0],key(4)))
    self.assertNotIn(key(0),raw);self.assertNotIn(key(6),raw)
    self.assertEqual(select_files(self.root.glob('*.csv'),HEADER,key(3),key(3)),[])
  def test_snapshot_multiline_and_partial_record(self):
    path=self.file('20261101T000000Z.csv',[])
    with path.open('a',newline='') as f:
      csv.writer(f,delimiter=';').writerow([key(2).decode()+'Z','quoted\n2026-10-01T00:00:00Z;not another record\nend'])
      f.write(key(3).decode()+'Z;"unfinished\n')
    entries=select_files([path],HEADER,key(1),key(4));self.assertEqual(len(entries),1)
    with path.open('a') as f:f.write('complete now"\n')
    raw=b''.join(stream_file(entries[0],key(4)))
    self.assertIn(b'not another record',raw);self.assertNotIn(b'unfinished',raw)
    parsed=list(csv.reader(io.StringIO(raw.decode()),delimiter=';'))
    self.assertEqual(len(parsed),2)
  def test_mid_stream_does_not_parse_timestamps(self):
    path=self.file('20261101T000000Z.csv',range(1,12))
    following=self.file('20261101T130000Z.csv',[13])
    entry=select_files([path,following],HEADER,key(0),key(13))[0]
    with patch('CsvStreaming.csv_time_key',side_effect=AssertionError('Mid must not parse cells')):
      self.assertEqual(b''.join(stream_file(entry,key(13))),path.read_bytes())
  def test_unknown_filename_requires_separate_normalization(self):
    path=self.file('legacy.csv',[10,11])
    with self.assertRaisesRegex(ValueError,'normalize'):
      select_files([path],HEADER,key(10),key(11))
  def test_gzip_negotiation_and_http_stream(self):
    self.assertFalse(accepts_gzip('gzip;q=0, identity'));self.assertTrue(accepts_gzip('gzip'))
    profile=self.root/'HeatProfileAq1';profile.mkdir()
    path=profile/'20261101T000000Z_HeatProfileAq1.csv'
    path.write_text('Time;Temp;Floor;Main\n2026-11-01T01:00:00Z;28;1;0\n')
    server=chart_server(self.root,'127.0.0.1',0,since='2026-11-01T00:00:00Z')
    worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
    import email.parser,email.policy,json
    def fetch(encoding,method='GET'):
      request=urllib.request.Request(f'http://127.0.0.1:{server.server_port}/api/history.csv?range=all',headers={'Accept-Encoding':encoding},method=method)
      with urllib.request.urlopen(request) as response:
        headers=response.headers;data=response.read()
      if method=='HEAD':return headers,data
      if headers.get('Content-Encoding')=='gzip':data=gzip.decompress(data)
      message=email.parser.BytesParser(policy=email.policy.default).parsebytes(('Content-Type: '+headers['Content-Type']+'\r\n\r\n').encode()+data)
      return headers,[part.get_payload(decode=True) for part in message.iter_parts()]
    try:
      headers,plain=fetch('identity');self.assertIsNone(headers.get('Content-Length'))
      headers,zipped=fetch('gzip');self.assertEqual(headers['Content-Encoding'],'gzip');self.assertEqual(plain[1:],zipped[1:]);self.assertEqual(json.loads(plain[0])['Files'],json.loads(zipped[0])['Files'])
      headers,_=fetch('gzip;q=0');self.assertIsNone(headers.get('Content-Encoding'))
      _,head=fetch('gzip','HEAD');self.assertEqual(head,b'')
      fetch('identity') # HEAD must release the single-reader gate.
    finally:server.shutdown();server.server_close();worker.join()
