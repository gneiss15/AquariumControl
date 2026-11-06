# test_pi_history.py, Version: 1.04
import csv,pathlib,sys,tempfile,unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from PiHistory import PiHistory
class PiHistoryTests(unittest.TestCase):
  def test_assignment_dedup_and_reboot_samples(self):
    with tempfile.TemporaryDirectory() as tmp:
      h=PiHistory(tmp);h.configure({'HeatProfileAq1':{'Main':['M','heater']}})
      p={'ID':'wrong','Status':{'HeatPi':{'Schema':1,'Mode':'Shadow','Profile':'HeatProfileAq1','Samples':4,'Config':{'Setpoint':28},'DemandW':80,'Valid':True},'Channels':[{'Name':'heater','State':True}]}}
      h.ingest(p);self.assertFalse(list(pathlib.Path(tmp).rglob('*.csv')))
      p['ID']='M';h.ingest(p);h.ingest(p);p['Status']['HeatPi']['Samples']=1;h.ingest(p)
      file=next(pathlib.Path(tmp).rglob('*.csv'))
      with file.open() as f:rows=list(csv.DictReader(f,delimiter=';'))
      self.assertEqual(len(rows),2);self.assertEqual(rows[0]['ActualMainOn'],'1');self.assertEqual(rows[0]['DemandW'],'80')
      p['Status']['HeatPi']['Config']['Setpoint']=27;p['Status']['HeatPi']['Mode']='Active';p['Status']['HeatPi']['Samples']=2;h.ingest(p)
      with max(pathlib.Path(tmp).rglob('*.csv'),key=lambda p:p.stat().st_mtime).open() as f:active_rows=list(csv.DictReader(f,delimiter=';'))
      self.assertEqual(active_rows[-1]['Mode'],'Active')
      from CreateCharts import pi_chart_data,render_report
      values=pi_chart_data(file.parent);self.assertEqual(values['demand'],[80.,80.,None,80.]);self.assertEqual(values['setpoint'],[28,28,28,27])
      (pathlib.Path(tmp)/'HeatProfileAq1/test.csv').write_text('Time;Temp;Floor;Main\n2026-10-27T10:00:00Z;28;1;0\n')
      text,count=render_report(pathlib.Path(tmp));self.assertEqual(count,1);self.assertIn('const piSets',text)
      h.csv.close_all()

  def test_without_pi_debug_creates_no_pi_files(self):
    with tempfile.TemporaryDirectory() as tmp:
      h=PiHistory(tmp);h.configure({'HeatProfileAq1':{'Main':['M','heater']}})
      h.ingest({'ID':'M','Status':{'HeatControl':{'Mode':'Active','Profile':'HeatProfileAq1','FloorRequest':True},'Channels':[{'Name':'heater','State':True}]}})
      self.assertFalse(list(pathlib.Path(tmp).rglob('*.pi.csv')))
