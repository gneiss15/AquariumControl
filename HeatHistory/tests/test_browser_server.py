# test_browser_server.py, Version: 1.02
import csv,datetime as dt,json,gzip,pathlib,sys,tempfile,threading,unittest,urllib.request,urllib.error
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from BrowserServer import history,chart_server,active_since,csv_history
from EnergyHistory import HEADER as ENERGY_HEADER
from PiHistory import HEADER as PI_HEADER
UTC=dt.timezone.utc
class BrowserServerTests(unittest.TestCase):
  def setUp(self):
    self.tmp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.tmp.name)
    self.profile=self.root/'HeatProfileAq1';(self.profile/'Energy').mkdir(parents=True);(self.profile/'Pi').mkdir()
    self.now=dt.datetime.now(UTC).replace(microsecond=0)
  def tearDown(self):self.tmp.cleanup()
  def file(self,path,header,values):
    with path.open('w',newline='') as f:
      w=csv.DictWriter(f,delimiter=';',fieldnames=header);w.writeheader()
      for value in values:w.writerow({key:value.get(key,'') for key in header})
  def pi(self,when,name='one',mode='Active'):
    path=self.profile/'Pi'/(when.strftime('%Y%m%dT%H%M%S%fZ')+'_'+name+'.pi.csv')
    self.file(path,PI_HEADER,[{'Time':when.isoformat(),'Mode':mode,'Valid':1,'DemandW':50,'ActualWatts':40,'Samples':1}])
    path.with_suffix('.json').write_text(json.dumps({'Config':{'Setpoint':28}}))
  def energy(self,when,name='one',target=28):
    path=self.profile/'Energy'/(when.strftime('%Y%m%dT%H%M%S%fZ')+'_'+name+'.energy.csv')
    self.file(path,ENERGY_HEADER,[{'Time':when.isoformat(),'TempC':27.5,'TempValid':1,'RoomTempC':24,'RoomTempValid':1,
      'FloorState':1,'MainState':0,'MainCorrectedW':0,'MainReportedW':0,'MainPowerValid':1,'FloorNominalW':41.052,
      'MainIntervalQuality':'valid','FloorIntervalQuality':'valid','MainIntervalWh':0,'FloorIntervalWh':.1,
      'MainIntervalSeconds':10,'FloorIntervalSeconds':10}])
    path.with_suffix('.json').write_text(json.dumps({'Assignment':{'Profile':{'Pi':{'Setpoint':target}}}}))
  def test_active_boundary_default_day_and_all(self):
    self.pi(self.now-dt.timedelta(days=4),'shadow','Shadow');self.pi(self.now-dt.timedelta(days=2),'active')
    self.energy(self.now-dt.timedelta(days=3),'old');self.energy(self.now-dt.timedelta(days=2),'early');self.energy(self.now-dt.timedelta(hours=1),'latest')
    day=history(self.root,now=self.now);all_data=history(self.root,'all',now=self.now)
    self.assertEqual(len(day['Profiles'][0]['Energy']),1);self.assertEqual(len(all_data['Profiles'][0]['Energy']),2)
    self.assertEqual(active_since(self.root),self.now-dt.timedelta(days=2))
    self.assertEqual(day['Profiles'][0]['Energy'][0][6],41.052)
    self.assertEqual(day['Profiles'][0]['Energy'][0][12],28)
  def test_all_is_limited_to_retention_and_sources_unchanged(self):
    self.pi(self.now-dt.timedelta(days=40),'old');self.energy(self.now-dt.timedelta(days=35),'old');self.energy(self.now-dt.timedelta(days=2),'new')
    before={p:p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
    result=history(self.root,'all',now=self.now)
    self.assertEqual(len(result['Profiles'][0]['Energy']),1)
    self.assertEqual(before,{p:p.read_bytes() for p in self.root.rglob('*') if p.is_file()})
  def test_missing_activation_has_no_pre_pi_history(self):
    self.energy(self.now);self.assertEqual(history(self.root,now=self.now)['Profiles'],[])
  def test_active_energy_snapshot_works_without_pi_debug(self):
    self.energy(self.now-dt.timedelta(hours=1))
    path=next((self.profile/'Energy').glob('*.energy.json'));value=json.loads(path.read_text());value['Assignment']['Profile']['Pi']['Mode']='Active';path.write_text(json.dumps(value))
    self.assertEqual(len(history(self.root,now=self.now)['Profiles']),1)
  def test_stored_targets_and_cutoff_interval(self):
    self.pi(self.now-dt.timedelta(hours=2));self.energy(self.now-dt.timedelta(hours=2),'one',28);self.energy(self.now-dt.timedelta(hours=1),'two',27)
    values=history(self.root,'all',now=self.now)['Profiles'][0]['Energy']
    self.assertIsNone(values[0][8]);self.assertEqual([row[12] for row in values],[28,27])
  def test_static_page_json_gzip_and_no_private_paths(self):
    self.pi(self.now-dt.timedelta(hours=2));self.energy(self.now-dt.timedelta(hours=1))
    server=chart_server(self.root,'127.0.0.1',0);worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
    base='http://127.0.0.1:'+str(server.server_port)
    try:
      with urllib.request.urlopen(base) as f:page=f.read().decode()
      self.assertIn('value="1day" selected',page);self.assertNotIn('27.5',page);self.assertLess(len(page),4000)
      with urllib.request.urlopen(urllib.request.Request(base+'/api/history',headers={'Accept-Encoding':'gzip'})) as f:
        self.assertEqual(f.headers['Content-Encoding'],'gzip');payload=json.loads(gzip.decompress(f.read()))
      self.assertEqual(payload['Range'],'1day');self.assertEqual(len(payload['Profiles']),1)
      for route,code in [('/config.json',404),('/../config.json',404),('/api/history?range=forever',400),('/api/history?file=config.json',400)]:
        with self.assertRaises(urllib.error.HTTPError) as caught:urllib.request.urlopen(base+route)
        self.assertEqual(caught.exception.code,code)
    finally:server.shutdown();server.server_close();worker.join()

class CsvTransportTests(BrowserServerTests):
  def test_csv_bundle_keeps_original_cells_and_metadata(self):
    from email.parser import BytesParser
    from email.policy import default
    self.pi(self.now-dt.timedelta(hours=2));self.energy(self.now-dt.timedelta(hours=1),target=27)
    body,kind=csv_history(self.root,now=self.now)
    message=BytesParser(policy=default).parsebytes(('Content-Type: '+kind+'\r\nMIME-Version: 1.0\r\n\r\n').encode()+body)
    parts=list(message.iter_parts());manifest=json.loads(parts[0].get_payload(decode=True))
    self.assertEqual(manifest['Schema'],2);self.assertEqual(manifest['Files'][0]['Setpoint'],27)
    self.assertEqual(parts[1].get_payload(decode=True),next((self.profile/'Energy').glob('*.energy.csv')).read_bytes())
  def test_csv_clamps_range_keeps_quotes_and_ignores_incomplete_tail(self):
    from BrowserServer import selected_csv
    path=self.profile/'Pi/20261031T000000Z.pi.csv';header=('Time','Reason')
    before=self.now-dt.timedelta(hours=2);inside=self.now-dt.timedelta(minutes=1)
    self.file(path,header,[{'Time':before.isoformat(),'Reason':'old'},{'Time':inside.isoformat(),'Reason':'a;"quoted"\nline'}])
    with path.open('ab') as f:f.write(b'2026-10-31T10:00:00Z;incomplete')
    low=(self.now-dt.timedelta(hours=1)).strftime('%Y-%m-%dT%H:%M:%S.%f').encode();high=self.now.strftime('%Y-%m-%dT%H:%M:%S.%f').encode()
    raw=selected_csv(path,header,low,high).decode()
    parsed=list(csv.reader(raw.splitlines(keepends=True),delimiter=';'))
    self.assertEqual(len(parsed),2);self.assertEqual(parsed[1][1],'a;"quoted"\nline')
