# test_energy_archive.py, Version: 1.01
"""Execute the installer's real archive block against temporary user-owned data."""
import json,pathlib,pwd,re,subprocess,sys,tempfile,unittest
class ArchiveTests(unittest.TestCase):
  def test_archive_preserves_data_and_configuration(self):
    project=pathlib.Path(__file__).resolve().parents[1]
    code=re.search(r"<<'PY_SERIES'\n(.*?)\nPY_SERIES",(project/'Install.sh').read_text(),re.S).group(1)
    with tempfile.TemporaryDirectory() as directory:
      root=pathlib.Path(directory);(root/'data'/'HeatProfileAq1').mkdir(parents=True)
      data=root/'data/HeatProfileAq1/old.csv';data.write_bytes(b'Time;Temp;Floor;Main\nold-data\n')
      config={'DataDir':'data','DashboardUrl':'http://keep.test','Custom':{'preserve':True}}
      (root/'config.json').write_text(json.dumps(config));snapshot=root/'snapshot.json';snapshot.write_text('{"PowerCal":12345}')
      subprocess.run([sys.executable,'-',str(root),pwd.getpwuid(__import__('os').getuid()).pw_name,str(project/'config.example.json'),str(snapshot)],input=code,text=True,check=True,capture_output=True)
      self.assertEqual(list((root/'data').iterdir()),[])
      archives=list((root/'archive').iterdir());self.assertEqual(len(archives),1)
      self.assertEqual((archives[0]/'data/HeatProfileAq1/old.csv').read_bytes(),b'Time;Temp;Floor;Main\nold-data\n')
      new=json.loads((root/'config.json').read_text())
      for k,v in config.items():self.assertEqual(new[k],v)
      self.assertEqual(new['FloorPowerWatts'],{'HeatProfileAq1':41.052,'HeatProfileAq2':21.216})
      self.assertEqual(new['PowerCalibrationAtStart'],{'PowerCal':12345})
      self.assertIn('SeriesStartedAt',new)
if __name__=='__main__':unittest.main()
