# test_heat_pi_interpolation.py, Version: 1.01
"""Verify production PI energy/model corrections without relay hardware."""
import pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
program=r'''#include <GnHeatPi.h>
#include <cassert>
#include <cmath>
int main(){
 TGnHeatPiConfig c;c.Enabled=true;c.Setpoint=28;c.MainMaxWatts=100;c.Kp=1;
 c.TiSeconds=3600;c.CapacityWhPerK=100;c.LossWattsPerK=25;c.WindowSeconds=60;
 TGnHeatPi p;p.Configure(c);TGnHeatPiInput x;
 x.TemperatureValid=x.PowerValid=x.RoomValid=x.FloorValid=true;
 x.Temperature=27;x.RoomTemperature=25;x.MainWatts=100;
 x.Now=100000;p.Update(x);const double budget=p.MainRemaining;
 x.TemperatureValid=x.PowerValid=false;
 x.Now=110000;p.Update(x);x.Now=120000;p.Update(x);
 assert(std::abs(p.MainRemaining-(budget-2000))<1e-8);
 x.Now=130000;x.TemperatureValid=x.PowerValid=true;x.Temperature=28;x.MainWatts=0;p.Update(x);
 assert(std::abs(p.MainRemaining-(budget-1500))<1e-8);
 assert(std::abs(p.Energy-1500./3600)<1e-8);
 assert(std::abs(p.DifferenceHours-75./3600)<1e-8);
 // The delayed learner must match a complete, linearly interpolated history.
 for(unsigned lag : {0U,10U,20U,600U}) {
  c.DelaySeconds=lag;TGnHeatPi held,complete;held.Configure(c);complete.Configure(c);
  TGnHeatPiInput a;a.TemperatureValid=a.PowerValid=a.RoomValid=a.FloorValid=true;
  a.Temperature=28;a.RoomTemperature=25;a.MainWatts=100;
  for(unsigned t=0;t<=900;t+=10) {
   a.Now=100000+t*1000ULL;a.MainWatts=t<30 ? 100.*(30-t)/30 : 0;
   complete.Update(a);a.PowerValid=t!=10 && t!=20;
   if(!a.PowerValid)a.MainWatts=100;
   held.Update(a);a.PowerValid=true;
  }
  assert(std::abs(held.Energy-complete.Energy)<1e-8);
  assert(std::abs(held.DifferenceHours-complete.DifferenceHours)<1e-8);
 }
 c.DelaySeconds=0;
 // The true 60-second producer limit is inclusive; a later endpoint is not used.
 p.Configure(c);x.Now=100000;x.Temperature=27;x.MainWatts=100;p.Update(x);
 x.TemperatureValid=x.PowerValid=false;
 for(int j=1;j<=6;j++){x.Now=100000+j*10000;p.Update(x);assert(p.Status().Valid);}
 x.Now=170000;p.Update(x);assert(!p.Status().Valid);
 x.Now=180000;x.TemperatureValid=x.PowerValid=true;x.MainWatts=0;p.Update(x);
 assert(!p.HaveWindow && p.PowerCache.HoldCorrection==0);
 // Hold an ON floor sample, then correct the 30-second ON-to-OFF gap.
 c.FloorWatts=40;p.Configure(c);x={};
 x.TemperatureValid=x.PowerValid=x.RoomValid=x.FloorValid=true;
 x.Temperature=28;x.RoomTemperature=25;x.FloorOn=true;x.PeerBoot=12;
 x.Now=100000;p.Update(x);const double floorBudget=p.FloorRemaining;
 x.FloorValid=false;x.Now=110000;p.Update(x);x.Now=120000;p.Update(x);
 assert(p.Status().FloorAvailable && p.Status().ActualFloorOn);
 assert(std::abs(p.FloorRemaining-(floorBudget-800))<1e-8);
 x.FloorValid=true;x.FloorOn=false;x.Now=130000;p.Update(x);
 assert(std::abs(p.FloorRemaining-(floorBudget-600))<1e-8);
 assert(std::abs(p.Energy-600./3600)<1e-8);
 x.FloorValid=false;x.FloorOverride=true;x.Now=140000;p.Update(x);
 assert(!p.Status().FloorAvailable);
 x.FloorOverride=false;x.FloorValid=true;x.Now=150000;p.Update(x);
 x.FloorValid=false;x.PeerBoot=13;x.Now=160000;p.Update(x);
 assert(!p.Status().FloorAvailable);

}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp)
 (p/'GnBasics.h').write_text('#pragma once\n#include <cstdint>\n#include <string>\nusing TGNString=std::string;\n')
 # Expose state for numeric assertions, keeping the actual production algorithm.
 (p/'GnHeatPi.h').write_text((root/'GnHeatPi.h').read_text().replace(' private:',' public:'))
 (p/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+tmp,str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PI held energy budget, temperature/model interpolation and 60s expiry passed')
