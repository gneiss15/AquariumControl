# test_gpio_startup.py, Version: 1.02
"""Execute actual switch setup/output methods against a GPIO latch model."""
from pathlib import Path
import subprocess
import tempfile
import unittest

AQ = Path(__file__).resolve().parents[2]


def method(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class StartupTest(unittest.TestCase):
    def check_variant(self, project, led_active):
        source = (AQ / project / 'Sw/TmrSwHw.cpp').read_text()
        methods = '\n'.join(method(source, f'void TTmrSwHw::{name}(void)')
                            for name in ('Setup', 'RelayOn', 'RelayOff'))
        hardware = 'TmrSwA8THardware' if project == 'TmrSwA8T' else 'TmrSwShellyHardware'
        program = r'''
#include <cassert>
#include <cstdint>
#include <stdexcept>
using gpio_num_t = int;
constexpr int GPIO_MODE_OUTPUT=1, GPIO_MODE_INPUT=2, GPIO_PULLUP_ENABLE=1;
struct gpio_config_t { uint64_t pin_bit_mask=0; int mode=0, pull_up_en=0; };
int latch[40], configured[40], expected[40], writes[40];
int fail_pin=-1;
int gpio_set_level(int pin, int value) {
  if(pin==fail_pin) return -1;
  latch[pin]=value; ++writes[pin]; return 0;
}
int gpio_config(gpio_config_t const * config) {
  for(int pin=0;pin<40;++pin) if(config->pin_bit_mask & (1ULL<<pin)) {
    if(pin==fail_pin) return -1;
    if(config->mode==GPIO_MODE_OUTPUT) {
      // The first enabled level must already be inactive, even if the old
      // output latch was active. No intermediate active pulse is permitted.
      assert(writes[pin]>0 && latch[pin]==expected[pin]);
    } else assert(config->pull_up_en==GPIO_PULLUP_ENABLE);
    configured[pin]=config->mode;
  }
  return 0;
}
int gpio_get_level(int pin) { assert(configured[pin]); return latch[pin]; }
#define ESP_ERROR_CHECK(call) do { if((call)!=0) throw std::runtime_error("GPIO"); } while(0)
void SetPin(int pin) { latch[pin]=1; ++writes[pin]; }
void ClrPin(int pin) { latch[pin]=0; ++writes[pin]; }
bool GetInPin(int pin) { return gpio_get_level(pin); }
void PinMode_Output(int pin) {
 gpio_config_t config; config.pin_bit_mask=1ULL<<pin; config.mode=GPIO_MODE_OUTPUT;
 ESP_ERROR_CHECK(gpio_config(&config));
}
void PinMode_InputPullUp(int pin) {
 gpio_config_t config; config.pin_bit_mask=1ULL<<pin; config.mode=GPIO_MODE_INPUT;
 config.pull_up_en=GPIO_PULLUP_ENABLE; ESP_ERROR_CHECK(gpio_config(&config));
}
struct Meter { bool enabled=true; void Setup() {} void LoadEnabled(bool on) { enabled=on; } };
Meter TmrSwA8TPowerMeter, ShellyPowerMeter;
bool ledcAttach(int pin,int,int) { assert(configured[pin]==GPIO_MODE_OUTPUT); return true; }
void ledcWrite(int,int) {}
namespace HARDWARE {
constexpr int RelayPin=13, StatusLedPin=2, KeyPin=4, RedLedPin=18, BlueLedPin=19;
constexpr int RelayActiveLevel=1, StatusLedActiveLevel=LED_ACTIVE, StatusLedOffDuty=255;
constexpr bool RelayConfirmed=true, StatusLedConfirmed=true;
}
class TTmrSwHw {
public:
 bool FRelayStatus=true, FRelayStatusKnown=false, FWiFiLedStatus=true;
 bool FInpStatus=false, FStatusLedReady=false;
 void Setup(); void RelayOn(); void RelayOff();
};
METHODS
int main() {
  TTmrSwHw hw;
  latch[13]=1; expected[13]=0;
  latch[2]=LED_ACTIVE; expected[2]=!LED_ACTIVE;
  latch[18]=latch[19]=0; expected[18]=expected[19]=1;
  latch[4]=1;
  hw.Setup();
  assert(!hw.FRelayStatus && hw.FRelayStatusKnown && latch[13]==0);
  assert(latch[2]==!LED_ACTIVE && hw.FInpStatus);
  assert(configured[4]==GPIO_MODE_INPUT);
  hw.RelayOn(); assert(hw.FRelayStatus && hw.FRelayStatusKnown && latch[13]==1);
  hw.RelayOff(); assert(!hw.FRelayStatus && hw.FRelayStatusKnown && latch[13]==0);
  // A repeat initialization must also discard the old known state before
  // any failing GPIO configuration; failure may not advertise a known OFF.
  hw.RelayOn(); fail_pin=13;
  try { hw.Setup(); assert(false); } catch(std::runtime_error const &) {}
  assert(!hw.FRelayStatusKnown);
}
'''.replace('HARDWARE', hardware).replace('LED_ACTIVE', str(led_active)).replace('METHODS', methods)
        with tempfile.TemporaryDirectory(prefix='gpio-startup-') as tmp:
            path = Path(tmp)
            (path / 'test.cpp').write_text(program)
            subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                            str(path / 'test.cpp'), '-o', str(path / 'test')], check=True)
            subprocess.run([str(path / 'test')], check=True)

    def test_nous_active_low_led(self):
        self.check_variant('TmrSwA8T', 0)

    def test_nous_testnode_active_high_led(self):
        self.check_variant('TmrSwA8T', 1)

    def test_shelly(self):
        self.check_variant('TmrSwShelly', 0)


if __name__ == '__main__':
    unittest.main()
