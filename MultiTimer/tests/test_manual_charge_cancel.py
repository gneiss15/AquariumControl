#!/usr/bin/env python3
# test_manual_charge_cancel.py, Version: 1.02
"""Exercise actual native manual switching with injected RAM state and fake GPIO."""
import pathlib
import subprocess
import tempfile
root = pathlib.Path(__file__).resolve().parents[1]
lib = root.parent/'Libs/GnBasicLib'
with tempfile.TemporaryDirectory() as temporary:
    tmp = pathlib.Path(temporary)
    (tmp/'Scheduler.h').write_text((root/'include/Scheduler.h').read_text().replace(' private:', ' public:'))
    (tmp/'IHardware.h').write_text('''#pragma once
#include <GnBasics.h>
class TIHardware { public: bool Fail=false; bool SetPin(unsigned,bool,TGNString &,bool=false) {return !Fail;} };
''')
    (tmp/'test.cpp').write_text(r'''
#include "Scheduler.h"
#include <cassert>
void Arm(TGnChargeRunOn &c, bool awaiting) {
 c.Reset();
 assert(c.Update("Laptop",80,60,0,true,false,70,0,true,true,true));
 assert(c.Update("Laptop",80,60,21000,false,true,0,21000,true,false,true));
 if(awaiting) assert(!c.Update("Laptop",80,60,600000,false,true,0,600000,true,false,true));
}
void CheckCancelled(TGnChargeRunOn &c) {
 // No intervening automatic evaluation: the manual entry point itself must cancel.
 assert(!c.Update("Laptop",80,60,601000,false,true,0,601000,true,false,true));
 c.Update("Laptop",80,60,700000,true,false,75,0,false,false,true,true,1);
 uint32_t factor; assert(!c.TakeLearned(factor));
}
int main() {
 TIHardware hw; TChannelConfigs channels(1); channels[0].Id="Laptop";channels[0].OutputPin=1;
 THeaterProfiles profiles;
 TScheduler scheduler(hw,channels,profiles); TGNString error;
 for(bool awaiting : {false,true}) {
  for(bool automatic : {false,true}) {
   auto &state=scheduler.FChannelStati[0];state.FState=true;state.FAutomaticState=automatic;
   Arm(state.FChargeRunOn,awaiting);
   assert(scheduler.ToggleChannel(0,error));CheckCancelled(state.FChargeRunOn);
   state.FState=true;Arm(state.FChargeRunOn,awaiting);
   assert(scheduler.ToggleChannel(0,std::chrono::minutes(2),error));CheckCancelled(state.FChargeRunOn);
  }
 }
 // Two manual toggles between regulator ticks must not revive the old baseline.
 auto &state=scheduler.FChannelStati[0]; state.FState=true;state.FAutomaticState=true;
 Arm(state.FChargeRunOn,false);
 assert(scheduler.ToggleChannel(0,error));assert(scheduler.ToggleChannel(0,error));
 CheckCancelled(state.FChargeRunOn);
 // Even a failed physical manual write invalidates the measurement cycle.
 Arm(state.FChargeRunOn,false);hw.Fail=true;
 assert(!scheduler.ToggleChannel(0,error));CheckCancelled(state.FChargeRunOn);
}
''')
    sources = [root/'src/Scheduler.cpp', root/'src/Logger.cpp']
    sources += [(lib.parent/'GnAqPrjsLib' if n in ('GnHeaterControl','GnUdpStatus','GnDeviceIdentity') else lib)/(n+'.cpp') for n in ['GnBasics','GnTime','GnCoopTimer','GnHeaterControl','GnUdpStatus','GnDeviceIdentity']]
    subprocess.run(['g++','-std=c++20','-O0','-Wall','-Wextra','-Werror','-DSYSTEM_DUMMY','-I'+str(tmp),'-I'+str(root/'include'),'-I'+str(lib),'-I'+str(lib.parent/'GnAqPrjsLib'),str(tmp/'test.cpp'),*[str(p) for p in sources],'-ljson-c','-lpthread','-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
print('Native manual switching: normal/timed toggle, return to automatic, rapid double toggle, running/awaiting cycle and failed GPIO write cancel learning immediately.')
