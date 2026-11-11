# test_energy.py, Version: 1.07
import csv,json,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from EnergyHistory import EnergyHistory, HEADER
from CreateCharts import render_report, energy_chart_data

class EnergyTests(unittest.TestCase):
  def setUp(self):
    self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
    self.e=EnergyHistory(self.root,{'FloorPowerWatts':{'HeatProfileAq1':60},'PowerFactors':{'M':2}})
    self.profile={'Id':'HeatProfileAq1','TempId':'Temp'}
    self.times={'Channels':{'Main':[{'Id':'heater','Profile':'HeatProfileAq1','Role':'HeaterMain'}],'Floor':[{'Id':'floor','Profile':'HeatProfileAq1','Role':'HeaterFloor'}]}}
    self.ids={'Main':'M','Floor':'F','Temp':'T','GN-Pi':'F'}
    self.e.configure([self.profile],self.times,self.ids)
  def test_neutral_main_role_preserves_assignment(self):
    old=self.e.assignments['HeatProfileAq1']['Signature']
    self.times['Channels']['Main'][0]['Role']='MainSw'
    self.e.configure([self.profile],self.times,self.ids)
    self.assertEqual(old,self.e.assignments['HeatProfileAq1']['Signature'])
  def tearDown(self):self.e.csv.close_all();self.tmp.cleanup()
  def packet(self,identity,t,power=100,state=True,valid=True,boot=1):
    status={'CurrentTime':1000+t,'TimeSynchronized':True,'Identity':{'BootCounter':boot},'Channels':[{'Name':'heater' if identity=='M' else 'floor','State':state}]}
    if identity=='M':status['PowerMeter']={'ActivePower':power,'Valid':valid}
    else:status['Measurements']=[{'Id':'RoomTemperature','Value':24.5,'Valid':True}]
    return {'ID':identity,'Status':status}
  def feed(self,identity,t,**kw):self.e.ingest(self.packet(identity,t,**kw),t,1000+t)
  def rows(self):
    p=next(self.root.rglob('*.energy.csv'))
    with p.open() as f:return list(csv.DictReader(f,delimiter=';'))
  def test_measured_energy_zero_room_and_floor(self):
    self.e.ingest({'ID':'T','Status':{'Temperature':28.125}},0,1000)
    self.feed('F',0);self.feed('M',0,power=100);self.feed('F',10);self.feed('M',10,power=0)
    row=self.rows()[-1]
    self.assertEqual(float(row['MainReportedW']),0);self.assertEqual(row['MainPowerValid'],'1')
    self.assertAlmostEqual(float(row['MainIntervalWh']),100*10/3600)
    self.assertAlmostEqual(float(row['FloorEstimatedWh']),60*10/3600)
    self.assertEqual(float(row['RoomTempC']),24.5);self.assertEqual(float(row['TempC']),28.125)
  def test_gap_is_held_then_corrected_but_restart_is_not_bridged(self):
    self.feed('M',0);self.feed('M',10,valid=False)
    self.assertAlmostEqual(float(self.rows()[-1]['MainIntegratedWh']),200*10/3600)
    self.assertEqual(self.rows()[-1]['MainIntervalQuality'],'held')
    self.feed('M',20,power=0)
    self.assertAlmostEqual(float(self.rows()[-1]['MainIntegratedWh']),100*20/3600)
    self.assertEqual(self.rows()[-1]['MainIntervalQuality'],'interpolated')
    self.feed('M',40,power=100);before=float(self.rows()[-1]['MainIntegratedWh'])
    self.feed('M',50,boot=2)
    self.assertEqual(self.rows()[-1]['MainIntervalQuality'],'device_restart')
    self.assertAlmostEqual(float(self.rows()[-1]['MainIntegratedWh']),before)
    self.assertEqual(float(self.rows()[-1]['MainUnknownSeconds']),10)
  def test_interpolation_negative_correction_and_sixty_second_limit(self):
    self.feed('M',0,power=100)
    for t in (10,20,30):self.feed('M',t,valid=False)
    self.feed('M',40,power=0)
    row=self.rows()[-1]
    self.assertLess(float(row['MainIntervalWh']),0)
    self.assertAlmostEqual(float(row['MainIntegratedWh']),100*40/3600)
    self.assertEqual(float(row['MainCoveredSeconds']),40)
    self.feed('M',50,valid=False);self.feed('M',100,power=100)
    self.assertEqual(self.rows()[-1]['MainIntervalQuality'],'interpolated')
    self.feed('M',170,power=0)
    row=self.rows()[-1]
    self.assertAlmostEqual(float(row['MainUnknownSeconds']),10)
    self.assertAlmostEqual(float(row['MainIntegratedWh']),(4000+6000+12000)/3600)
  def test_missing_temperature_does_not_stop_measured_energy(self):
    self.e.ingest({'ID':'T','Status':{'Temperature':None}},0,1000)
    self.feed('M',0);self.feed('M',10)
    self.assertEqual(self.rows()[-1]['TempValid'],'0')
    self.assertAlmostEqual(float(self.rows()[-1]['MainIntervalWh']),200*10/3600)
  def test_pause_and_resume_unknown_time(self):
    self.feed('M',0);self.e.pause();self.feed('M',10)
    self.e.configure([self.profile],self.times,self.ids);self.feed('M',20)
    self.assertEqual(float(self.rows()[-1]['MainUnknownSeconds']),20)
    self.assertEqual(float(self.rows()[-1]['MainIntegratedWh']),0)
  def test_source_change_rotates_and_duplicate_time_excluded(self):
    self.feed('M',0);self.e.ingest(self.packet('M',0),1,1001)
    self.assertEqual(self.rows()[-1]['MainIntervalQuality'],'held')
    self.ids['Main']='N';self.e.configure([self.profile],self.times,self.ids)
    self.e.ingest({'ID':'N','Status':self.packet('M',10)['Status']},10,1010)
    self.assertEqual(len(list(self.root.rglob('*.energy.csv'))),2)
  def test_factor_validation_and_negative_power(self):
    for value in [0,-1,True,float('nan')]:
      with self.assertRaises(ValueError):EnergyHistory(self.root,{'PowerFactors':{'M':value}})
    self.feed('M',0,power=-1);self.assertEqual(self.rows()[-1]['MainPowerValid'],'0')
  def test_chart_carries_raw_totals_across_sessions_and_keeps_invalid_gaps(self):
    carry={'mainwh':0.,'floorwh':0.}
    for index in range(2):
      path=self.root/f'{index}.energy.csv'
      with path.open('w',newline='') as stream:
        writer=csv.DictWriter(stream,fieldnames=HEADER,delimiter=';');writer.writeheader()
        for second,quality,main,floor in [(0,'valid',0,0),(10,'valid',5,2),(20,'invalid',6,3)]:
          row={key:'' for key in HEADER}
          row.update(Time=f'2026-10-25T0{index}:00:{second:02d}Z',MainIntegratedWh=main,
                     FloorEstimatedWh=floor,MainPowerValid='1',FloorState='1',
                     MainIntervalQuality=quality,FloorIntervalQuality=quality)
          writer.writerow(row)
      values=energy_chart_data(path,carry)
      self.assertEqual(values['mainwh'],[index*6,index*6+5,None])
      self.assertEqual(values['floorwh'],[index*3,index*3+2,None])
    self.assertEqual(carry,{'mainwh':12,'floorwh':6})

  def test_energy_files_dont_break_legacy_charts_and_are_rendered(self):
    p=self.root/'HeatProfileAq1'/'new.csv';p.parent.mkdir(parents=True)
    p.write_text('Time;Temp;Floor;Main\n2026-10-24T12:00:00Z;28;1;0\n')
    self.feed('F',0);self.feed('M',0);self.feed('F',10);self.feed('M',10)
    text,count=render_report(self.root)
    self.assertEqual(count,1);self.assertIn('Water and room temperature share one plot',text);self.assertIn('const energySets',text)
    self.assertIn('24.5',text);self.assertEqual(text.count('<article>'),1);self.assertNotIn('id="energy-',text);self.assertIn("matches:pi?'x7':'x5'",text);self.assertIn('const source = energy || chartSets[index]',text); self.assertIn('provisional',next(self.root.rglob('*.energy.json')).read_text())

if __name__=='__main__':unittest.main()
