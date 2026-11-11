# test_recording_events.py, Version: 1.00
"""Exercise event routing, per-profile rotation and persistent file handles."""
import copy,json,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import test_heathistory as fixture

class RecordingEventsTests(unittest.TestCase):
  def setUp(self):
    self.fixture=fixture.HeatHistoryTests();self.fixture.setUp();f=self.fixture
    self.monitor=f.monitor
    profiles={'HeatProfileAq1':fixture.profile(), 'HeatProfileAq2':{**fixture.profile(),'Id':'HeatProfileAq2','TempId':'FeederAq2'}}
    channels=f.fake_fetch('http://dashboard.test/api/times',1)['Channels']
    channels['GN-Pi']['Rows'].append({'Id':'FloorAq2','Role':'HeaterFloor','Profile':'HeatProfileAq2'})
    channels['HeaterMainAq2']={'Rows':[{'Id':'MainAq2','Role':'MainSw','Profile':'HeatProfileAq2'}]}
    f.mapping.update({'FeederAq2':'02:33:44:55:66:88@3','HeaterMainAq2':'02:22:33:44:55:88@4'})
    registry=f.fake_fetch('http://dashboard.test/api/device-registry',1)
    self.snapshot={'Snapshot':True,'Profiles':profiles,'Channels':channels,'Registry':registry}
    self.monitor.OnDashboardEvent(copy.deepcopy(self.snapshot));f.fetch_calls.clear()

  def tearDown(self):self.fixture.tearDown()

  def feed(self,now=100):
    f=self.fixture
    statuses={'GN-Pi':{'Channels':[{'Name':'FloorAq1','State':True},{'Name':'FloorAq2','State':False}]},
      'HeaterMainAq1':{'Channels':[{'Name':'MainAq1','State':False}]},
      'HeaterMainAq2':{'Channels':[{'Name':'MainAq2','State':False}]},
      'FeederAq1':{'Temperature':f.temp_value},'FeederAq2':{'Temperature':26.5}}
    for function,status in statuses.items():
      self.monitor.OnUdpPacket({'ID':f.mapping[function],'Type':'Status','Version':2,'Status':status},now,1000+now)

  def test_packets_record_without_dashboard_polling_and_keep_files_open(self):
    self.feed()
    before={path:entry[0] for path,entry in self.monitor.csv.files.items()}
    self.assertEqual(len(before),2)
    self.fixture.temp_value=27.6;self.feed(110)
    self.assertEqual(self.fixture.fetch_calls,[])
    for path,stream in before.items():self.assertIs(self.monitor.csv.files[path][0],stream)
    self.assertEqual(len(list(self.fixture.root.rglob('*_HeatProfileAq*.csv'))),2)

  def test_profile_patch_rotates_only_affected_aquarium_and_closes_old_handle(self):
    self.feed();before=dict(self.monitor.csv.files)
    changed={**self.snapshot['Profiles']['HeatProfileAq1'],'MainOn':27.7}
    self.monitor.OnDashboardEvent({'Profiles':{'HeatProfileAq1':changed}})
    self.feed(110)
    counts={aq:len(list((self.monitor.data_dir/f'HeatProfileAq{aq}').glob('*.csv'))) for aq in (1,2)}
    self.assertEqual(counts,{1:2,2:1})
    for path,(stream,_) in before.items():self.assertEqual(stream.closed,path.parent.name=='HeatProfileAq1')

  def test_disconnect_and_unchanged_reconnect_do_not_rotate_or_stop_recording(self):
    self.feed()
    self.monitor.OnDashboardEvent({'ConnectionError':'test disconnect'})
    self.fixture.temp_value=27.6;self.feed(110)
    self.monitor.OnDashboardEvent(copy.deepcopy(self.snapshot));self.feed(120)
    self.assertEqual(len(list(self.fixture.root.rglob('*_HeatProfileAq*.csv'))),2)
    self.assertTrue(self.monitor.configured)
    self.assertEqual(self.fixture.fetch_calls,[])

  def test_channel_change_routes_to_new_channel(self):
    self.feed()
    group={'Rows':[{'Id':'NewMain','Role':'MainSw','Profile':'HeatProfileAq1'}]}
    self.monitor.OnDashboardEvent({'Channels':{'HeaterMainAq1':group}})
    self.assertEqual(self.monitor.energy.assignments['HeatProfileAq1']['Main'][1],'NewMain')

if __name__=='__main__':unittest.main()
