# test_heat_pi_profile.py, Version: 1.04
import json,pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
program=r'''
#include <GnHeatPiProfileJson.h>
#include <iostream>
int main(){std::string s;std::getline(std::cin,s);auto* o=json_tokener_parse(s.c_str());bool ok=GnHeatPiProfileJsonValid(o);json_object_put(o);return ok?0:1;}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'GnBasics.h').write_text('#pragma once\n#include <string>\n#include <cstdint>\nusing TGNString=std::string;\n');(p/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-I'+tmp,'-I'+str(root),'-I'+str(root.parent/'GnBasicLib'),str(p/'test.cpp'),'-ljson-c','-o',str(p/'test')],check=True)
 example={'Pi':{'Mode':'Shadow','Setpoint':28,'FloorDevice':'02:11:22:33:44:55@1',
  'FloorChannel':'floor','FloorWatts':40,'MainMaxWatts':350,'CapacityWhPerK':590,
  'LossWattsPerK':27,'Kp':270,'TiSeconds':31200,'WindowSeconds':600,'DelaySeconds':600}}
 def valid(row):return subprocess.run([str(p/'test')],input=json.dumps(row),text=True).returncode==0
 assert valid({}) and valid({'Pi':{'Mode':'Off'}}) and valid(example)
 for key,value in [('Mode','Active'),('Kp',True),('Kp',0),('Kp',float('nan')),('TiSeconds',1),('FloorDevice','GN-Pi'),('FloorChannel','')]:
  row=json.loads(json.dumps(example));row['Pi'][key]=value;assert not valid(row),(key,value)
 row=json.loads(json.dumps(example));row['Pi']['FutureField']=42;assert valid(row)
 row['Pi']['Mode']='Active';row['Pi']['MainDevice']='2C:BC:BB:C8:B6:88@4';assert valid(row)
 row['Pi']['MainDevice']='GN-Pi';assert not valid(row)
 row['Measurement']='BatteryPercent';assert not valid(row)
print('PI profile validation tests passed')
