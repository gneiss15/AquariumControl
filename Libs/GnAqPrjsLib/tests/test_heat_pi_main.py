# test_heat_pi_main.py, Version: 1.02
"""Exercise the production main relay decision body with deterministic PI/relay inputs."""
import pathlib,subprocess,tempfile
lib=pathlib.Path(__file__).resolve().parents[1]
s=(lib.parents[1]/'Common/TmrSw/SwitchTimes.cpp').read_text();a=s.index('void TSwitchTimes::ApplyAutomaticState(void)');b=s.index('\nvoid TSwitchTimes::DoEvents',a)
program=r'''
#include <GnBasics.h>
#include <GnHeaterControl.h>
#include <cassert>
struct {std::string WireId(){return "main";}} GnDeviceIdentity;
struct {bool On=false;bool RelayStatusKnown(){return true;}bool RelayStatus(){return On;}} TmrSwHw;
struct TChannel{THeaterRole Role=THeaterRole::HeaterMain;std::string Profile="Aq1",Id="main-channel";};
struct {TChannel C;THeaterProfiles P;TChannel &Channel(){return C;}THeaterProfiles const&Profiles(){return P;}bool SaveLearnedChargeFactor(std::string,unsigned){return false;}} TmrSwChannels;
struct {void LocalChanged(){}} GnTimeSync;
#undef Debug
void Debug(const char*){}
class TSwitchTimes {public:
 bool FOverrideActive=false,FCalibrationOverrideActive=false,FOverrideState=false,FCalibrationOverrideState=false;
 bool FAutomaticState=false,FSchedulePresent=false,FScheduleState=false,FHysteresisStateKnown=false,FHysteresisState=false;
 TGnHeaterControl FHeaterControl;TGnChargeRunOn FChargeRunOn;
 struct {TGnHeatPiStatus S;auto const &Status(){return S;}} FHeatPi;
 void UpdateHeatPi(){}void RefreshProfiles(){}void ApplyOutputState(bool v){TmrSwHw.On=v;}
 void ApplyAutomaticState(void);
};
'''+s[a:b]+r'''
int main(){
 THeaterProfile p;p.Id="Aq1";p.Pi.Active=true;p.Pi.MainDevice="main";TmrSwChannels.P={p};
 TSwitchTimes t;t.FHeaterControl.Profiles({p});t.FHeatPi.S.Valid=true;t.FHeatPi.S.MainRequest=true;
 t.ApplyAutomaticState();assert(TmrSwHw.On);
 t.FHeatPi.S.MainRequest=false;t.ApplyAutomaticState();assert(!TmrSwHw.On);
 t.FHeatPi.S.MainRequest=true;t.FOverrideActive=true;t.FOverrideState=false;t.ApplyAutomaticState();assert(!TmrSwHw.On);
 t.FOverrideActive=false;t.FCalibrationOverrideActive=true;t.FCalibrationOverrideState=false;t.ApplyAutomaticState();assert(!TmrSwHw.On);
 t.FCalibrationOverrideActive=false;t.FSchedulePresent=true;t.FScheduleState=false;t.ApplyAutomaticState();assert(!TmrSwHw.On);
 t.FSchedulePresent=false;t.FHeatPi.S.Valid=false;t.ApplyAutomaticState();assert(!TmrSwHw.On);
 TmrSwChannels.P[0].NoTempOn=true;t.ApplyAutomaticState();assert(TmrSwHw.On);
 TmrSwChannels.P[0].NoTempOn=false;TmrSwChannels.P[0].Pi.MainDevice="other";t.FHeatPi.S.Valid=true;
 t.ApplyAutomaticState();assert(!TmrSwHw.On);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'test.cpp').write_text(program)
 # The unchanged hysteresis and charge transport path is discarded only after compilation.
 cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-DSYSTEM_DUMMY','-DGN_DEBUG_CHARGE_LEARNING=0','-DGN_DEBUG_HEAT_PI=0','-I'+str(lib),'-I'+str(lib.parent/'GnBasicLib'),str(p/'test.cpp')]
 cmd += [str((lib.parent/'GnBasicLib' if n in ('GnBasics','GnTime','GnCoopTimer') else lib)/(n+'.cpp')) for n in ['GnBasics','GnTime','GnCoopTimer','GnDeviceIdentity','GnHeaterControl','GnUdpStatus']]
 subprocess.run(cmd+['-Wl,--gc-sections','-ljson-c','-lpthread','-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('Actual main relay decision: PI requests, invalid data, schedule, overrides and main identity binding passed')
