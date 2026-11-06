# test_heat_pi_budget.py, Version: 1.01
"""Exercise energy cutoffs, deficit carry, overshoot compensation and cancellation."""
import pathlib,subprocess,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
program=r'''
#include <GnHeatPi.h>
#include <cassert>
int main() {
 TGnHeatPiConfig c;c.Enabled=true;c.Setpoint=28;c.MainMaxWatts=100;
 c.CapacityWhPerK=100;c.LossWattsPerK=50;c.Kp=1;c.TiSeconds=3600;c.WindowSeconds=60;
 TGnHeatPi p;p.Configure(c);TGnHeatPiInput x;
 x.TemperatureValid=x.PowerValid=x.RoomValid=true;x.Temperature=28;x.RoomTemperature=27;
 auto step=[&](unsigned t,double watts){x.Now=t*1000ULL;x.MainWatts=watts;p.Update(x);};
 step(0,100);step(10,100);step(20,100);assert(p.Status().MainRequest);
 step(30,0);assert(!p.Status().MainRequest); // 3000 Ws delivered, independent of nameplate PWM.
 step(40,0);step(50,0);step(60,25);assert(p.Status().MainRequest);
 p.Configure(c);step(0,25);
 for(unsigned t=10;t<60;t+=10)step(t,25);
 step(60,100); // 1500 Ws deficit enters the next period.
 for(unsigned t=70;t<=100;t+=10)step(t,100);
 assert(p.Status().MainRequest);
 step(110,0);assert(!p.Status().MainRequest);
 step(120,100);step(130,100);step(140,100);assert(p.Status().MainRequest);
 step(150,0);assert(!p.Status().MainRequest); // Previous overdelivery reduces the next budget.
 // Demand is frozen for the period, not changed by a fresh room sample.
 p.Configure(c);step(0,100);x.RoomTemperature=28;step(10,100);
 assert(p.Status().Demand==0 && p.Status().MainRequest);
 // A manual intervention or an unobserved gap discards the pending budget.
 x.Manual=true;step(20,100);assert(!p.Status().MainRequest);
 x.Manual=false;step(30,0);assert(!p.Status().MainRequest);
 x.RoomTemperature=27;step(60,0);assert(p.Status().MainRequest);
 // Floor priority and takeover when floor feedback disappears.
 c.FloorWatts=60;p.Configure(c);x.FloorValid=true;x.FloorOn=false;
 step(0,0);assert(p.Status().FloorRequest && !p.Status().MainRequest);
 x.FloorValid=false;step(10,0);assert(p.Status().FloorRequest && !p.Status().MainRequest);
 for(unsigned t=20;t<=70;t+=10)step(t,0);
 assert(!p.Status().FloorRequest && p.Status().MainRequest);
 // Delivery by either circuit counts toward the shared total cutoff.
 c.FloorWatts=60;p.Configure(c);x.FloorValid=true;x.FloorOn=true;
 step(0,100);step(10,100);assert(p.Status().FloorRequest);
 step(20,0);assert(!p.Status().FloorRequest && !p.Status().MainRequest);
 // A new zero-demand period consumes any signed overdelivery credit without switching.
 p.Configure(c);x.FloorValid=false;x.RoomTemperature=27;step(0,100);
 step(10,100);step(20,100);step(30,100);x.RoomTemperature=28;
 step(40,0);step(50,0);step(60,0);assert(!p.Status().MainRequest);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'GnBasics.h').write_text('#pragma once\n#include <cstdint>\n#include <string>\nusing TGNString=std::string;\n')
 (p/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror','-I'+tmp,'-I'+str(root),'-I'+str(root.parent/'GnBasicLib'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PI energy budget tests passed')
