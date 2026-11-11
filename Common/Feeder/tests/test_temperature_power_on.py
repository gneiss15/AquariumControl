# test_temperature_power_on.py, Version: 1.01
"""Exercise the production ReadTemp body with deterministic completed scratchpad reads."""
from pathlib import Path
import subprocess,tempfile
source=Path(__file__).resolve().parents[1]/'FeederTemperature.cpp'
text=source.read_text();begin=text.index('void TFeederTemperature::ReadTemp(');end=text.index('\nvoid TFeederTemperature::Loop(',begin)
program=r'''
#include <cstdint>
#include <ctime>
#include <cassert>
struct TGnCoopTimerBase { bool Stopped=false;void Stop(){Stopped=true;} };
enum class TDiagnosticReason { None,ReadCrcFailed,TemperatureInvalid };
enum class TTemperatureError { None,ReadFailed };
class TFeederTemperature {
public:
 bool FHasTemperature=false,FPendingTemperature=false;float FPendingTemperatureCelsius=0;
 void *FOneWire=this;bool FDiagnosticPaused=false,FValid=false;
 unsigned FDiagnosticReadStatus[21]={},FSampleRevision=0,Nok=0;
 float FTemperatureCelsius=27;std::time_t FUpdatedAt=0;
 TTemperatureError FError=TTemperatureError::None;
 TDiagnosticReason LastReason=TDiagnosticReason::None;
 int16_t NextRaw=432;int8_t NextAttempt=1;
 int8_t ReadScratchpad(uint8_t *out,TDiagnosticReason &reason){
  reason=TDiagnosticReason::ReadCrcFailed;out[0]=uint16_t(NextRaw)&255;out[1]=uint16_t(NextRaw)>>8;return NextAttempt;
 }
 void RecordDiagnosticPoll(bool ok,TDiagnosticReason reason){if(!ok)Nok++;LastReason=reason;}
 void ReadTemp(TGnCoopTimerBase *,uint16_t);
};
'''+text[begin:end]+r'''
int main(){
 TFeederTemperature sensor;TGnCoopTimerBase timer;
 sensor.NextRaw=0x0550;sensor.ReadTemp(&timer,1);
 assert(timer.Stopped && !sensor.FValid && sensor.FSampleRevision==0 && sensor.FUpdatedAt==0);
 assert(sensor.LastReason==TDiagnosticReason::TemperatureInvalid && sensor.Nok==1);
 sensor.NextRaw=432;sensor.ReadTemp(&timer,1);
 assert(sensor.FValid && sensor.FTemperatureCelsius==27 && sensor.FSampleRevision==1);
 auto revision=sensor.FSampleRevision;auto updated=sensor.FUpdatedAt;
 sensor.NextRaw=0x0550;sensor.ReadTemp(&timer,1);
 assert(!sensor.FValid && sensor.FTemperatureCelsius==27 && sensor.FSampleRevision==revision && sensor.FUpdatedAt==updated);
 sensor.NextRaw=433;sensor.ReadTemp(&timer,1);
 assert(sensor.FValid && sensor.FTemperatureCelsius==27.0625 && sensor.FSampleRevision==revision+1);
 for(int16_t raw:{int16_t(-55*16),int16_t(125*16),int16_t(0x054f),int16_t(0x0551)}){
  sensor.FHasTemperature=false;sensor.NextRaw=raw;sensor.ReadTemp(&timer,1);assert(sensor.FValid);
 }
 for(int16_t raw:{int16_t(-55*16-1),int16_t(125*16+1)}){
  sensor.NextRaw=raw;sensor.ReadTemp(&timer,1);assert(!sensor.FValid);
 }
 sensor.FHasTemperature=false;sensor.NextRaw=432;sensor.ReadTemp(&timer,1);
 sensor.NextRaw=480;sensor.ReadTemp(&timer,1);assert(!sensor.FValid && sensor.FTemperatureCelsius==27);
 sensor.NextRaw=433;sensor.ReadTemp(&timer,1);assert(sensor.FValid && !sensor.FPendingTemperature);
 sensor.NextRaw=480;sensor.ReadTemp(&timer,1);assert(!sensor.FValid);
 sensor.NextRaw=481;sensor.ReadTemp(&timer,1);assert(sensor.FValid && sensor.FTemperatureCelsius==30.0625);
 sensor.NextRaw=432;sensor.ReadTemp(&timer,1);assert(!sensor.FValid);
 sensor.NextRaw=0x0550;sensor.ReadTemp(&timer,1);assert(!sensor.FValid && !sensor.FPendingTemperature);
 sensor.NextRaw=432;sensor.ReadTemp(&timer,1);assert(!sensor.FValid);
 sensor.NextRaw=433;sensor.ReadTemp(&timer,1);assert(sensor.FValid);
 sensor.NextRaw=432;sensor.NextAttempt=0;revision=sensor.FSampleRevision;sensor.ReadTemp(&timer,1);
 assert(!sensor.FValid && sensor.FSampleRevision==revision && sensor.LastReason==TDiagnosticReason::ReadCrcFailed);
 sensor.NextAttempt=20;sensor.ReadTemp(&timer,1);assert(sensor.FValid && sensor.FDiagnosticReadStatus[20]==1);
}
'''
program=program.replace('#include <cassert>','#include <cassert>\n#include <initializer_list>')
with tempfile.TemporaryDirectory() as tmp:
 root=Path(tmp);(root/'test.cpp').write_text(program)
 subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(root/'test.cpp'),'-o',str(root/'test')],check=True)
 subprocess.run([str(root/'test')],check=True)
print('Production temperature read: 85C rejection, timestamp/revision preservation, recovery, bounds and retry/error paths passed')
