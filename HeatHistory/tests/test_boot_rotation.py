# test_boot_rotation.py, Version: 1.01
import csv,json,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from test_energy import EnergyTests
from PiHistory import PiHistory

class BootRotationTests(EnergyTests):
  def test_energy_service_resume_totals_but_not_gap(self):
    self.feed('M',0);self.feed('M',10)
    old=self.rows()[-1];old_path=self.e.sessions['HeatProfileAq1']['Path']
    from EnergyHistory import EnergyHistory
    self.e.csv.close_all()
    self.e=EnergyHistory(self.root,self.e.config)
    self.e.configure([self.profile],self.times,self.ids)
    self.feed('M',20)
    self.assertEqual(self.e.sessions['HeatProfileAq1']['Path'],old_path)
    row=self.rows()[-1]
    self.assertEqual(row['MainIntegratedWh'],old['MainIntegratedWh'])
    self.assertNotEqual(row['MainIntervalQuality'],'valid')
    with patch('EnergyHistory.BOOT_ID','next-system-boot'):
      self.e.csv.close_all()
      self.e=EnergyHistory(self.root,self.e.config);self.e.configure([self.profile],self.times,self.ids)
      self.feed('M',30)
    self.assertEqual(len(list(self.root.rglob('*.energy.csv'))),2)

  def test_pi_service_resume_and_system_boot_rotation(self):
    assignment={'HeatProfileAq1':{'Main':['M','heater']}}
    packet={'ID':'M','Status':{'Identity':{'BootCounter':2},'HeatPi':{
      'Schema':1,'Profile':'HeatProfileAq1','Mode':'Active','Samples':1,'Config':{} }}}
    h=PiHistory(self.root);h.configure(assignment);h.ingest(packet)
    h.csv.close_all()
    h=PiHistory(self.root);h.configure(assignment);h.ingest(packet)
    self.assertEqual(len(list(self.root.rglob('*.pi.csv'))),1)
    packet['Status']['HeatPi']['Samples']=2;h.ingest(packet)
    with next(self.root.rglob('*.pi.csv')).open() as f:
      self.assertEqual(len(list(csv.DictReader(f,delimiter=';'))),2)
    with patch('PiHistory.BOOT_ID','next-system-boot'):
      h.csv.close_all()
      h=PiHistory(self.root);h.configure(assignment);h.ingest(packet)
    self.assertEqual(len(list(self.root.rglob('*.pi.csv'))),2)

    h.csv.close_all()
