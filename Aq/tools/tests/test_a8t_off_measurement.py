# test_a8t_off_measurement.py, Version: 1.01
"""Run actual meter processing/getters with load present while logical relay is OFF."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from test_gpio_startup import method

AQ = Path(__file__).resolve().parents[2]


class MeterTest(unittest.TestCase):
    def test_off_does_not_mask_measurements(self):
        source = (AQ / 'TmrSwA8T/Sw/TmrSwA8TPowerMeter.cpp').read_text()
        header = (AQ / 'TmrSwA8T/Sw/TmrSwA8TPowerMeter.h').read_text()
        methods = method(source, 'void TTmrSwA8TPowerMeter::LoadEnabled( bool Enabled )')
        methods += '\n' + method(source, 'void TTmrSwA8TPowerMeter::Loop(void)')
        getters = []
        for name in ('Current', 'ActivePower'):
            body = re.search(r'float\s+' + name + r'\(void\) const\s+(\{[^}]*\})', header)
            self.assertIsNotNone(body)
            getters.append(f'float {name}(void) const ' + body[1])
        program = r'''
#include <cassert>
#include <cstdint>
#define portENTER_CRITICAL(x)
#define portEXIT_CRITICAL(x)
constexpr uint32_t HjlPowerRatio=1362, HjlVoltageRatio=822, HjlCurrentRatio=3300;
constexpr double GateSeconds[]={2.0,4.0};
bool ChangedLoad[2]={}, Ready[2]={}, Good[2]={};
bool VoltageSample=false;
int Counts[2]={};
class TTmrSwA8TPowerMeter {
public:
 bool FHardwareReady=true, FLoadEnabled=true, FVoltageValid=false;
 float FCurrent=0.5f, FActivePower=100.0f, FVoltage=0.0f;
 uint32_t FPowerCal=9461, FCurrentCal=3255, FVoltageCal=1525;
 uint32_t FCurrentIntervals=0, FVoltageIntervals=0;
 bool Available(void) const { return true; }
 void LoadEnabled(bool Enabled); void Loop(void);
GETTERS
};
METHODS
int main() {
 TTmrSwA8TPowerMeter meter;
 meter.LoadEnabled(false);
 assert(ChangedLoad[0] && ChangedLoad[1]);
 assert(meter.Current()==0.5f && meter.ActivePower()==100.0f);
 // A complete stable window with load must report it even when relay is OFF.
 Counts[0]=100; Counts[1]=80; Good[0]=Good[1]=true; Ready[0]=Ready[1]=true;
 meter.Loop();
 assert(!meter.FLoadEnabled && meter.Current()>0.5f && meter.ActivePower()>25.0f);
 assert(meter.FCurrentIntervals==100);
 // An invalid transition window still yields no valid frequency.
 Good[0]=Good[1]=false; Ready[0]=Ready[1]=true;
 meter.Loop(); assert(meter.Current()==0.0f && meter.ActivePower()==0.0f);
}
'''.replace('GETTERS', '\n'.join(getters)).replace('METHODS', methods)
        with tempfile.TemporaryDirectory(prefix='a8t-off-measurement-') as tmp:
            path = Path(tmp)
            (path / 'test.cpp').write_text(program)
            subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True)


if __name__ == '__main__':
    unittest.main()
