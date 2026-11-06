# test_storage_migration.py, Version: 1.05
import csv,datetime as dt,json,os,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from StorageRetention import StorageRetention
from ExportActive import export
from EnergyHistory import HEADER,EnergyHistory

class StorageMigrationTests(unittest.TestCase):
  def test_expiry_requires_consent_protects_active_pairs_and_resume_state(self):
    with tempfile.TemporaryDirectory() as tmp:
      root=Path(tmp);directory=root/'HeatProfileAq1'/'Energy';directory.mkdir(parents=True)
      old=directory/'20200101T000000Z.energy.csv';old.write_text('old');old.with_suffix('.json').write_text('{}')
      active=directory/'20200102T000000Z.energy.csv';active.write_text('active')
      active.with_suffix('.json').write_text('{}')
      state=directory.parent/'state.json';state.write_text('{}')
      os.utime(state,(1,1))
      for path in directory.iterdir():os.utime(path,(1,1))
      StorageRetention(root,{}).maintain();self.assertTrue(old.exists())
      StorageRetention(root,{'StorageDeleteExpired':True}).maintain([active])
      self.assertFalse(old.exists());self.assertFalse(old.with_suffix('.json').exists())
      self.assertTrue(active.exists());self.assertTrue(active.with_suffix('.json').exists());self.assertTrue(state.exists())
  def test_byte_cap_removes_oldest_closed_pairs_even_before_expiry(self):
    with tempfile.TemporaryDirectory() as tmp:
      root=Path(tmp);directory=root/'HeatProfileAq1'/'Energy';directory.mkdir(parents=True)
      old=directory/'20261029T000000Z.energy.csv';old.write_bytes(b'x'*700000)
      active=directory/'20261029T010000Z.energy.csv';active.write_bytes(b'x'*700000)
      os.utime(old,(1,1))
      StorageRetention(root,{'StorageDeleteExpired':True,'StorageMaxBytes':1048576}).maintain([active])
      self.assertFalse(old.exists());self.assertTrue(active.exists())
  def test_age_cleanup_then_byte_cap_uses_remaining_sorted_segments(self):
    with tempfile.TemporaryDirectory() as tmp:
      root=Path(tmp)
      expired=root/'expired.csv';expired.write_bytes(b'x'*700000);os.utime(expired,(1,1))
      recent=root/'recent.csv';recent.write_bytes(b'x'*700000)
      active=root/'active.csv';active.write_bytes(b'x'*700000)
      StorageRetention(root,{'StorageDeleteExpired':True,'StorageMaxBytes':1048576}).maintain([active])
      self.assertFalse(expired.exists());self.assertFalse(recent.exists())
      self.assertTrue(active.exists())
  def test_metadata_age_does_not_remove_a_recent_recording(self):
    with tempfile.TemporaryDirectory() as tmp:
      root=Path(tmp);path=root/'segment.csv';path.write_text('new')
      meta=path.with_suffix('.json');meta.write_text('{}');os.utime(meta,(1,1))
      StorageRetention(root,{'StorageDeleteExpired':True}).maintain()
      self.assertTrue(path.exists());self.assertTrue(meta.exists())
  def test_active_data_over_limit_reports_failure(self):
    with tempfile.TemporaryDirectory() as tmp:
      root=Path(tmp);path=root/'active.csv';path.write_bytes(b'x'*1100000)
      with self.assertRaisesRegex(OSError,'active logs'):
        StorageRetention(root,{'StorageDeleteExpired':True,'StorageMaxBytes':1048576}).maintain([path])
      self.assertTrue(path.exists())
  def test_no_utc_rotation_keeps_valid_interval(self):
    with tempfile.TemporaryDirectory() as tmp:
      history=EnergyHistory(Path(tmp),{'FloorPowerWatts':{'HeatProfileAq1':40}})
      history.configure([{'Id':'HeatProfileAq1','TempId':'T'}],{'Channels':{'M':[{'Role':'MainSw','Profile':'HeatProfileAq1','Id':'main'}],'F':[{'Role':'HeaterFloor','Profile':'HeatProfileAq1','Id':'floor'}]}},{'M':'m','F':'f','T':'t'})
      def packet(second):return {'ID':'m','Status':{'CurrentTime':1000+second,'TimeSynchronized':True,'Identity':{'BootCounter':1},'Channels':[{'Name':'main','State':True}],'PowerMeter':{'Valid':True,'ActivePower':100}}}
      history.ingest(packet(0),0,1000)
      history.sessions['HeatProfileAq1']['Day']='previous-day'
      history.ingest(packet(10),10,1010)
      paths=list(Path(tmp).rglob('*.energy.csv'));self.assertEqual(len(paths),1)
      with history.sessions['HeatProfileAq1']['Path'].open() as stream:row=list(csv.DictReader(stream,delimiter=';'))[-1]
      self.assertEqual(row['MainIntervalQuality'],'valid');self.assertAlmostEqual(float(row['MainIntervalWh']),100*10/3600)
      history.csv.close_all()

  def test_export_filters_and_preserves_source_and_metadata(self):
    with tempfile.TemporaryDirectory() as tmp:
      root=Path(tmp)/'source';root.mkdir();target=Path(tmp)/'export'
      now=dt.datetime.now(dt.timezone.utc);origin=now-dt.timedelta(hours=1)
      (root/'config.json').write_text(json.dumps({'DataDir':'data','ViewFromUTC':origin.isoformat(),'UnrelatedPrivate':'never copy'}))
      for aq in [1,2]:
        path=root/'data'/f'HeatProfileAq{aq}'/'Energy'/'20261029T000000Z.energy.csv';path.parent.mkdir(parents=True)
        with path.open('w',newline='') as stream:
          writer=csv.DictWriter(stream,fieldnames=HEADER,delimiter=';');writer.writeheader()
          writer.writerow({'Time':(origin-dt.timedelta(seconds=1)).isoformat()});writer.writerow({'Time':now.isoformat()})
        path.with_suffix('.json').write_text('{"Assignment":{}}')
      before={str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*') if p.is_file()}
      result=export(root,target);self.assertEqual(result['Rows'],2)
      self.assertNotIn('UnrelatedPrivate',json.loads((target/'config.json').read_text()))
      self.assertEqual(before,{str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*') if p.is_file()})
      self.assertFalse(list(target.rglob('state.json')))
      with self.assertRaises(ValueError):export(root,target)
