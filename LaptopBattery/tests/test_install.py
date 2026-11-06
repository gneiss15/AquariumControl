#!/usr/bin/env python3
# test_install.py, Version: 1.03
"""Execute the installer with fake privilege/service commands and simulated sensors."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

Root=Path(__file__).resolve().parents[1]


class InstallerTests(unittest.TestCase):
  def test_install_and_preflight(self):
    with tempfile.TemporaryDirectory() as temporary:
      Temp=Path(temporary);Project=Temp/'project';Project.mkdir();Bin=Temp/'bin';Bin.mkdir();Home=Temp/'home';Home.mkdir()
      for Name in ['Install.sh','BatteryUdpSender.py','BatteryUdpSender.service','ChargeLearning.py','ChargeLearning.sh','Versions.txt','Messages.py']:
        shutil.copy2(Root/Name,Project/Name)
      Source=(Project/'BatteryUdpSender.py').read_text()
      import re
      for Key,Value in {'BatteryCapacityFile':'63','AcOnlineFile':'1','BatteryStateFile':'Charging','LaptopMacFile':'02:11:22:33:44:55'}.items():
        File=Temp/Key;File.write_text(Value)
        Source,Count=re.subn(r'^'+Key+r' = ".*"$',Key+' = '+json.dumps(str(File)),Source,flags=re.MULTILINE)
        self.assertEqual(Count,1)
      (Project/'BatteryUdpSender.py').write_text(Source)
      (Bin/'id').write_text('#!/bin/sh\nprintf "nobody\\n"\n')
      (Bin/'systemctl').write_text('#!/bin/sh\nexit 0\n')
      (Bin/'sudo').write_text('''#!/usr/bin/env python3
import json,os,sys,pathlib
with open(os.environ['INSTALL_AUDIT'],'a') as f:f.write(json.dumps(sys.argv[1:])+'\\n')
if sys.argv[-1]=='/etc/systemd/system/BatteryUdpSender.service':
 pathlib.Path(os.environ['INSTALL_UNIT']).write_text(pathlib.Path(sys.argv[-2]).read_text())
''')
      for File in Bin.iterdir():File.chmod(0o755)
      Audit=Temp/'audit';Unit=Temp/'BatteryUdpSender.service'
      Env=dict(os.environ,PATH=str(Bin)+':'+os.environ['PATH'],HOME=str(Home),INSTALL_AUDIT=str(Audit),INSTALL_UNIT=str(Unit))
      Result=subprocess.run(['bash',str(Project/'Install.sh')],env=Env,capture_output=True,text=True)
      self.assertEqual(Result.returncode,0,Result.stdout+Result.stderr)
      Calls=[json.loads(Line) for Line in Audit.read_text().splitlines()]
      self.assertIn(['systemctl','daemon-reload'],Calls)
      self.assertIn(['systemctl','enable','BatteryUdpSender.service'],Calls)
      self.assertIn(['systemctl','restart','BatteryUdpSender.service'],Calls)
      self.assertIn('User=nobody',Unit.read_text());self.assertNotIn('@LAPTOP_USER@',Unit.read_text())
      if shutil.which('systemd-analyze'):
        Verify=subprocess.run(['systemd-analyze','verify',str(Unit)],capture_output=True,text=True)
        self.assertEqual(Verify.returncode,0,Verify.stderr)
      self.assertTrue((Home/'.local/state/LaptopBattery/ChargeLearning.json').exists())
      # Updating in place must preserve permission, counter and boot identity.
      StateFile=Home/'.local/state/LaptopBattery/ChargeLearning.json'
      State=json.loads(StateFile.read_text());State['ChargeLearnAllowed']=False
      StateFile.write_text(json.dumps(State))
      PreviousState=StateFile.read_bytes()
      Result=subprocess.run(['bash',str(Project/'Install.sh')],env=Env,capture_output=True,text=True)
      self.assertEqual(Result.returncode,0,Result.stdout+Result.stderr)
      self.assertEqual(StateFile.read_bytes(),PreviousState)
      Calls=[json.loads(Line) for Line in Audit.read_text().splitlines()]
      self.assertEqual(Calls.count(['systemctl','restart','BatteryUdpSender.service']),2)
      # Missing target hardware must stop before any privileged install/service operation.
      Audit.unlink();(Temp/'BatteryCapacityFile').unlink()
      Result=subprocess.run(['bash',str(Project/'Install.sh')],env=Env,capture_output=True,text=True)
      self.assertNotEqual(Result.returncode,0);self.assertFalse(Audit.exists())


if __name__=='__main__':unittest.main()
