# test_profile_measurement.py, Version: 1.05
# Test actual native profile parsing without any GPIO/network runtime.
import json, pathlib, subprocess, tempfile
source = pathlib.Path(__file__).resolve().parents[1]
program = r'''
#include "ConfigStore.h"
#include <cassert>
int main(int argc, char ** argv) {
 assert(argc==3);
 TChannelConfigs Channels; THeaterProfiles Profiles; TGNString Error;
 bool Valid = ConfigStore.ChannelsFromJson(argv[1], Channels, Profiles, Error);
 if(TGNString(argv[2])=="INVALID") assert(!Valid);
 else { assert(Valid && Profiles.size()==1); assert(Profiles[0].Measurement==argv[2]); if(Profiles[0].Pi.Active)assert(Profiles[0].Pi.MainDevice=="2C:BC:BB:C8:B6:88@4"); }
}
'''
with tempfile.TemporaryDirectory() as tmp:
 root = pathlib.Path(tmp); (root/'test.cpp').write_text(program)
 subprocess.run(['g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror', '-ffunction-sections', '-fdata-sections', '-DSYSTEM_DUMMY', '-I'+str(source/'include'), '-I'+str(source.parent/'Libs/GnBasicLib'),'-I'+str(source.parent/'Libs/GnAqPrjsLib'), str(root/'test.cpp'), str(source/'src/ConfigStore.cpp'), str(source.parent/'Libs/GnAqPrjsLib/GnHeaterControl.cpp'), '-Wl,--gc-sections', '-ljson-c', '-o', str(root/'test')], check=True)
 profile = {'Id':'Charge','TempId':'LaptopBattery','FloorOn':40,'FloorOff':80,'MainOn':40,'MainOff':80,'NoTempOn':False}
 for field, expected in [(None,'Temperature'), ('BatteryPercent','BatteryPercent'), ('Temperature','Temperature'), ('','INVALID'), ('Kind','INVALID'), ('Status.Temperature','INVALID'), (True,'INVALID')]:
  row = dict(profile)
  if field is not None: row['Measurement'] = field
  subprocess.run([str(root/'test'), json.dumps({'Profiles':[row],'Channels':[]}), expected], check=True)
 row = {**profile, 'Measurement':None}
 subprocess.run([str(root/'test'), json.dumps({'Profiles':[row],'Channels':[]}), 'INVALID'], check=True)
 for fields, expected in [
  ({'Future': {'nested': True}}, 'BatteryPercent'),
  ({'ChargeRunOn': False}, 'BatteryPercent'),
  ({'ChargeRunOn': True}, 'INVALID'),
  ({'ChargeRunOn': True, 'ChargeSecondsPerPercent': 15, 'Future': 7}, 'BatteryPercent'),
  ({'ChargeRunOn': True, 'ChargeSecondsPerPercent': 15.5}, 'INVALID'),
  ({'ChargeRunOn': True, 'ChargeSecondsPerPercent': -1}, 'INVALID'),
  ({'ChargeRunOn': True, 'ChargeSecondsPerPercent': 4294967296}, 'INVALID'),
  ({'ChargeRunOn': True, 'ChargeSecondsPerPercent': 0}, 'INVALID'),
  ({'ChargeRunOn': True, 'ChargeSecondsPerPercent': True}, 'INVALID')]:
  row = {**profile, 'Measurement': 'BatteryPercent', **fields}
  subprocess.run([str(root/'test'), json.dumps({'Profiles':[row],'Channels':[]}), expected], check=True)
 pi={'Mode':'Shadow','Setpoint':28,'FloorDevice':'02:11:22:33:44:55@1',
     'FloorChannel':'floor','FloorWatts':40,'MainMaxWatts':350,'CapacityWhPerK':590,
     'LossWattsPerK':27,'Kp':270,'TiSeconds':31200,'WindowSeconds':600,'DelaySeconds':600}
 pi.update(Mode='Active',MainDevice='2C:BC:BB:C8:B6:88@4')
 row={**profile,'Pi':pi}
 subprocess.run([str(root/'test'),json.dumps({'Profiles':[row],'Channels':[]}),'Temperature'],check=True)
 print('Native profiles: default/named field accepted; empty, null, boolean, reserved and path fields rejected.')
