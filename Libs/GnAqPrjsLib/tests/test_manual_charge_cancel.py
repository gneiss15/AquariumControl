#!/usr/bin/env python3
# test_manual_charge_cancel.py, Version: 1.03
"""Run the actual ESP manual-entry bodies with fake relay/automatic callbacks."""
import pathlib
import subprocess
import tempfile
lib = pathlib.Path(__file__).resolve().parents[1]
source = (lib.parents[1]/'Common/TmrSw/SwitchTimes.cpp').read_text()
def body(name):
    start = source.index('bool TSwitchTimes::'+name+'(')
    opened = source.index('{',start); depth = 1; end = opened+1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}'); end += 1
    return source[start:end]
program = r'''
#include <GnChargeRunOn.h>
#include <GnHeatPi.h>
#include <cassert>
#include <vector>
enum class THeaterRole { Timer, HeaterMain };
struct {bool Known=true,On=true;bool RelayStatusKnown(){return Known;}bool RelayStatus(){return On;}} TmrSwHw;
struct Config {THeaterRole Role=THeaterRole::HeaterMain;};
struct {Config Item;Config &Channel(){return Item;}} TmrSwChannels;
class TSwitchTimes {
 public:
 TGnChargeRunOn FChargeRunOn;
 TGnHeatPi FHeatPi;
 bool FCalibrationOverrideActive=false,FCalibrationOverrideState=false,FOverrideActive=false,FOverrideState=false,FAutomaticState=false;
 std::vector<int> FEvents;
 void ApplyAutomaticState() {}
 void ApplyOutputState(bool on) {TmrSwHw.On=on;}
 bool ManualToggle(void);
 bool SetCalibrationOverride(bool Active,bool State);
};
'''+body('ManualToggle')+'\n'+body('SetCalibrationOverride')+r'''
void Arm(TGnChargeRunOn &c, bool awaiting) {
 c.Reset();
 assert(c.Update("Laptop",80,60,0,true,false,70,0,true,true,true));
 assert(c.Update("Laptop",80,60,21000,false,true,0,21000,true,false,true));
 if(awaiting) assert(!c.Update("Laptop",80,60,600000,false,true,0,600000,true,false,true));
}
void Check(TGnChargeRunOn &c) {
 assert(!c.Update("Laptop",80,60,601000,false,true,0,601000,true,false,true));
 c.Update("Laptop",80,60,700000,true,false,75,0,false,false,true,true,1);
 uint32_t learned;assert(!c.TakeLearned(learned));
}
int main() {
 for(bool awaiting : {false,true}) for(bool overridden : {false,true}) for(bool known : {false,true}) {
  TSwitchTimes s;TmrSwHw.Known=known;TmrSwHw.On=true;s.FOverrideActive=overridden;
  Arm(s.FChargeRunOn,awaiting);assert(s.ManualToggle());Check(s.FChargeRunOn);
 }
 for(bool active : {false,true}) {
  TSwitchTimes s;TmrSwHw.Known=true;TmrSwHw.On=true;
  Arm(s.FChargeRunOn,true);s.SetCalibrationOverride(active,true);Check(s.FChargeRunOn);
 }
 TSwitchTimes s;TmrSwHw.Known=true;TmrSwHw.On=true;
 Arm(s.FChargeRunOn,false);s.ManualToggle();s.ManualToggle();Check(s.FChargeRunOn);
}
'''
with tempfile.TemporaryDirectory() as temporary:
    tmp=pathlib.Path(temporary);(tmp/'test.cpp').write_text(program)
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-I'+str(lib),'-I'+str(lib.parent/'GnBasicLib'),str(tmp/'test.cpp'),'-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
print('ESP manual entry points: toggling, override release, unknown relay, calibration override and rapid double toggle cancel active/awaiting learning immediately.')
