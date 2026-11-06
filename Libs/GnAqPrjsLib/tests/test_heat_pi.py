# test_heat_pi.py, Version: 1.03
"""Compile and exercise the real shadow-only controller, never a relay mock."""
import pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
program=r'''
#include <GnHeatPi.h>
#include <cassert>
int main() {
 TGnHeatPiConfig c; c.Enabled=true;c.Setpoint=28;c.FloorWatts=41;c.MainMaxWatts=350;
 c.CapacityWhPerK=590;c.LossWattsPerK=27;c.Kp=270;c.TiSeconds=31200;c.WindowSeconds=600;
 TGnHeatPi p;p.Configure(c);TGnHeatPiInput x;
 x.TemperatureValid=x.PowerValid=x.FloorValid=x.RoomValid=true;
 x.Temperature=27.5;x.RoomTemperature=25;x.MainWatts=0;x.FloorOn=true;
 for(unsigned i=0;i<10000;i++){x.Now=i*10000ULL;p.Update(x);auto s=p.Status();
  assert(s.Valid && s.Demand>=0 && s.Demand<=391 && s.I>=-391 && s.I<=391);
  assert(s.FloorDemand<=41 && s.MainDemand<=350);
  if(s.MainDemand>0)assert(s.FloorDemand==41);
 }
 auto i=p.Status().I;x.TemperatureValid=false;x.Now+=10000;p.Update(x);
 assert(p.Status().Valid && p.Status().I>=i && p.Status().Learning);
 i=p.Status().I;
 x.Now+=60000;p.Update(x);assert(!p.Status().Valid && p.Status().I==i && !p.Status().MainRequest);
 x.TemperatureValid=true;x.Manual=true;x.Now+=10000;p.Update(x);
 assert(!p.Status().Learning && p.Status().I==i);
 x.Manual=false;x.FloorValid=false;x.Now+=10000;p.Update(x);
 assert(p.Status().FloorAvailable && std::string(p.Status().Reason)=="measurement-held");
 x.Now+=60000;p.Update(x);
 assert(!p.Status().FloorAvailable && p.Status().FloorDemand==0 && p.Status().MainDemand<=350);
 x.FloorValid=true;x.Now+=60000;p.Update(x);assert(!p.Status().Learning);
 x.Now+=10000;x.PeerBoot++;p.Update(x);assert(!p.Status().Learning);
 p.Configure(c);x={};x.TemperatureValid=x.PowerValid=x.FloorValid=x.RoomValid=true;
 x.Temperature=28;x.RoomTemperature=25;x.MainWatts=81;
 for(unsigned j=0;j<=362;j++){x.Now=j*10000ULL;p.Update(x);}
 assert(p.Status().LearnedWindows>=1);assert(std::abs(p.Status().Loss-27)<0.01);
 // Delayed learning must wait for a full lag buffer and a full valid hour.
 c.DelaySeconds=600;p.Configure(c);x={};x.TemperatureValid=x.PowerValid=x.FloorValid=x.RoomValid=true;
 x.Temperature=28;x.RoomTemperature=25;x.MainWatts=81;
 for(unsigned j=0;j<=400;j++){x.Now=j*10000ULL;p.Update(x);}
 assert(p.Status().LearnedWindows==0);
 for(unsigned j=401;j<=430;j++){x.Now=j*10000ULL;p.Update(x);}
 assert(p.Status().LearnedWindows>=1);
 // Excitation learns capacity from measured energy, and adjusts gain slowly.
 c.DelaySeconds=0;p.Configure(c);x={};x.TemperatureValid=x.PowerValid=x.FloorValid=x.RoomValid=true;
 x.MainWatts=321;
 for(unsigned j=0;j<=360;j++){x.Now=j*10000ULL;x.Temperature=28+0.3*j/360;x.RoomTemperature=x.Temperature-3;p.Update(x);}
 assert(p.Status().Capacity>600 && p.Status().Capacity<601);
 assert(p.Status().Kp>c.Kp && p.Status().Kp<c.Kp*1.05);
 x.ScheduleAllowed=false;x.Now+=10000;p.Update(x);assert(!p.Status().FloorRequest&&!p.Status().MainRequest);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'GnBasics.h').write_text('#pragma once\n#include <cstdint>\n#include <string>\nusing TGNString=std::string;\n')
 (p/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+tmp,'-I'+str(root),'-I'+str(root.parent/'GnBasicLib'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Heat PI shadow controller tests passed')
