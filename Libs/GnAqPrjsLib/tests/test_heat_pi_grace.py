# test_heat_pi_grace.py, Version: 1.02
"""Bound held readings by producer age, preserve PWM, continue bounded integration and learning."""
import pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
program=r'''#include <GnHeatPi.h>
#include <cassert>
#include <limits>
#include <string>
int main(){
 TGnHeatPiConfig c;c.Enabled=c.Active=true;c.Setpoint=28;c.FloorWatts=40;c.MainMaxWatts=350;
 c.CapacityWhPerK=500;c.LossWattsPerK=20;c.Kp=200;c.TiSeconds=30000;
 TGnHeatPi p;p.Configure(c);TGnHeatPiInput x;x.Now=100000;
 x.TemperatureValid=x.PowerValid=x.RoomValid=x.FloorValid=true;
 x.Temperature=27;x.MainWatts=50;x.RoomTemperature=25;
 // Temperature already 20s old when first used; the grace must not restart at this read.
 x.TemperatureAgeMs=20000;p.Update(x);
 x.TemperatureValid=false;x.Temperature=std::numeric_limits<double>::quiet_NaN();
 for(int j=1;j<=4;j++){x.Now=100000+j*10000;p.Update(x);
  assert(p.Status().Valid&&p.Status().Learning);
  assert(std::string(p.Status().Reason)=="measurement-held");assert(p.Status().Temperature==27);
 }
 x.Now=150000;p.Update(x);assert(!p.Status().Valid);
 x.Now=160000;x.TemperatureValid=true;x.TemperatureAgeMs=0;x.Temperature=28;p.Update(x);
 assert(p.Status().Valid);
 // A power gap continues with explicitly held power, bounded by its true sample age.
 x.Now=170000;x.PowerValid=false;x.MainWatts=-1;p.Update(x);
 for(int j=1;j<=5;j++){x.Now=170000+j*10000;p.Update(x);
  assert(p.Status().Valid&&p.Status().Learning);
 }
 x.Now=230000;p.Update(x);assert(!p.Status().Valid);
 // Reconfiguration/boot must not inherit old readings or an artificial grace period.
 p.Configure(c);x.Now=240000;p.Update(x);assert(!p.Status().Valid);
 x.PowerValid=true;x.MainWatts=0;x.Now=250000;p.Update(x);assert(p.Status().Valid);
 x.Now=260000;x.Manual=true;x.TemperatureValid=false;p.Update(x);
 assert(!p.Status().Learning&&!p.Status().MainRequest&&!p.Status().FloorRequest);
 x.Manual=false;x.ScheduleAllowed=false;x.Now=270000;p.Update(x);
 assert(!p.Status().MainRequest&&!p.Status().FloorRequest);
 // Above target, even a large measured heating pulse cannot increase the integral.
 p.Configure(c);x={};x.TemperatureValid=x.PowerValid=x.FloorValid=x.RoomValid=true;
 x.Temperature=28.25;x.MainWatts=350;x.RoomTemperature=25;x.FloorOn=true;
 x.Now=100000;p.Update(x);x.Now+=10000;p.Update(x);assert(p.Status().I<0);
 double previous=p.Status().I;x.PowerValid=false;x.Now+=10000;p.Update(x);
 assert(p.Status().I<previous&&p.Status().Learning);
 assert(std::string(p.Status().LearningReason)=="collecting-held");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'GnBasics.h').write_text('#pragma once\n#include <cstdint>\n#include <string>\nusing TGNString=std::string;\n');(p/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+tmp,'-I'+str(root),'-I'+str(root.parent/'GnBasicLib'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PI measurement grace: producer age, 60s expiry, held PWM, continued error integral/learning, no positive power feedback and reset passed')
