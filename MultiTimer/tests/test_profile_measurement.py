# test_profile_measurement.py, Version: 1.00
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
 else { assert(Valid && Profiles.size()==1); assert(Profiles[0].Measurement==argv[2]); }
}
'''
with tempfile.TemporaryDirectory() as tmp:
 root = pathlib.Path(tmp); (root/'test.cpp').write_text(program)
 subprocess.run(['g++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror', '-ffunction-sections', '-fdata-sections', '-DSYSTEM_DUMMY', '-I'+str(source/'include'), '-I'+str(source.parent/'Libs/GnBasicLib'), str(root/'test.cpp'), str(source/'src/ConfigStore.cpp'), str(source.parent/'Libs/GnBasicLib/GnHeaterControl.cpp'), '-Wl,--gc-sections', '-ljson-c', '-o', str(root/'test')], check=True)
 profile = {'Id':'Charge','TempId':'LaptopBattery','FloorOn':40,'FloorOff':80,'MainOn':40,'MainOff':80,'NoTempOn':False}
 for field, expected in [(None,'Temperature'), ('BatteryPercent','BatteryPercent'), ('Temperature','Temperature'), ('','INVALID'), ('Kind','INVALID'), ('Status.Temperature','INVALID'), (True,'INVALID')]:
  row = dict(profile)
  if field is not None: row['Measurement'] = field
  subprocess.run([str(root/'test'), json.dumps({'Profiles':[row],'Channels':[]}), expected], check=True)
 row = {**profile, 'Measurement':None}
 subprocess.run([str(root/'test'), json.dumps({'Profiles':[row],'Channels':[]}), 'INVALID'], check=True)
 print('Native profiles: default/named field accepted; empty, null, boolean, reserved and path fields rejected.')
