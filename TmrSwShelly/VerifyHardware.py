# VerifyHardware.py, Version: 1.04
"""Exercise the BL0942 driver and factory calibration writer with mocks."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp)
    (p / 'GnBasics.h').write_text('#pragma once\n')
    (p / 'Arduino.h').write_text('''#pragma once
#include <cstdint>
#include <cstddef>
inline uint32_t Now = 0;
inline uint32_t millis(void) { return Now; }
inline void delay(unsigned Count) { Now += Count; }
#define SERIAL_8N1 1
''')
    (p / 'HardwareSerial.h').write_text('''#pragma once
#include "Arduino.h"
#include <vector>
#include <deque>
#include <cassert>
inline std::deque<uint8_t> Rx;
inline std::vector<uint8_t> Tx;
inline unsigned Begins = 0;
class HardwareSerial {
 public:
 explicit HardwareSerial(int Port) { assert(Port == 1); }
 void begin(unsigned Baud, int Format, int RxPin, int TxPin) {
  assert(Baud == 9600 && Format == SERIAL_8N1 && RxPin == 5 && TxPin == 4); ++Begins;
 }
 size_t write(const uint8_t * Data, size_t Size) { Tx.insert(Tx.end(), Data, Data + Size); return Size; }
 void flush(void) {}
 int available(void) { return int(Rx.size()); }
 int read(void) { if(Rx.empty()) return -1; const int Value = Rx.front(); Rx.pop_front(); return Value; }
};
''')
    (p / 'esp_partition.h').write_text('''#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <cassert>
using esp_partition_subtype_t = int;
#define ESP_PARTITION_TYPE_DATA 1
#define ESP_OK 0
struct esp_partition_t { uint32_t address, size; bool readonly; };
inline esp_partition_t Factory = {0x7F0000, 0x10000, true};
inline bool Missing = false, ReadError = false;
inline unsigned FailErase = 0, FailWrite = 0;
inline std::string Record;
inline const esp_partition_t * esp_partition_find_first(int Type, int Subtype, const char * Label) {
 assert(Type == 1 && Subtype == 0x88 && !strcmp(Label, "shelly")); return Missing ? nullptr : &Factory;
}
inline int esp_partition_read(const esp_partition_t *, unsigned Offset, void * Data, size_t Size) {
 assert(Offset == 0x1000 && Size == 4096); if(ReadError) return -1;
 memset(Data, 0xFF, Size); memcpy(Data, Record.data(), Record.size()); return 0;
}
inline int esp_partition_erase_range(const esp_partition_t *, unsigned Offset, size_t Size) {
 assert(Offset == 0x1000 && Size == 4096); if(FailErase){--FailErase;return -1;} Record.clear(); return 0;
}
inline int esp_partition_write(const esp_partition_t *, unsigned Offset, const void * Data, size_t Size) {
 assert(Offset == 0x1000 && Size == 4096); if(FailWrite){--FailWrite;return -1;}
 const auto * Bytes=static_cast<const char *>(Data); size_t Length=0; while(Length<Size && uint8_t(Bytes[Length])!=0xFF)++Length;
 Record.assign(Bytes,Length); return 0;
}
''')
    test = r'''
#include "ShellyPowerMeter.h"
#include "esp_partition.h"
#include <cmath>
#include <limits>
#include <array>
static std::array<uint8_t,23> packet(uint32_t I, uint32_t V, int32_t W) {
 std::array<uint8_t,23> P = {}; P[0] = 0x55;
 auto put = [&](unsigned Pos, uint32_t Value) { for(unsigned N=0;N<3;++N) P[Pos+N]=uint8_t(Value>>(8*N)); };
 put(1,I); put(4,V); put(10,uint32_t(W));
 uint8_t Sum=0x58; for(unsigned N=0;N<22;++N) Sum+=P[N]; P[22]=uint8_t(~Sum); return P;
}
int main(void) {
 auto P=packet(200000,2300000,50000); TShellyMeterSample S={1,2,3};
 assert(ShellyDecodeMeterPacket(P.data(),P.size(),1e-4,5e-6,.002,S));
 assert(std::abs(S.Voltage-230)<.001 && std::abs(S.Current-1)<.001 && std::abs(S.Power-100)<.001);
 P=packet(0,2300000,-50000); assert(ShellyDecodeMeterPacket(P.data(),P.size(),1e-4,5e-6,.002,S)); assert(S.Power==-100);
 const auto Old=S; P[7]^=1; assert(!ShellyDecodeMeterPacket(P.data(),P.size(),1e-4,5e-6,.002,S)); assert(S.Power==Old.Power);
 P=packet(0,0,0); assert(!ShellyDecodeMeterPacket(nullptr,23,1,1,1,S));
 assert(!ShellyDecodeMeterPacket(P.data(),22,1,1,1,S));
 assert(!ShellyDecodeMeterPacket(P.data(),23,0,1,1,S));
 assert(!ShellyDecodeMeterPacket(P.data(),23,std::numeric_limits<double>::quiet_NaN(),1,1,S));
 P[0]=0; assert(!ShellyDecodeMeterPacket(P.data(),23,1,1,1,S));
 Record=R"({"factory":{"calib":{"done":true,"scales0":{"voltage_scale":10000000,"current_scale":500000,"apower_scale":200000000,"current_gain":0}}}})";
 TShellyPowerMeter Meter;
 Missing=true; assert(!Meter.Setup() && Begins==0 && Tx.empty()); Missing=false;
 Factory.readonly=false; assert(Meter.Setup() && Begins==1); Factory.readonly=true;
 Begins=0; Tx.clear();
 Factory.address++; assert(!Meter.Setup() && Begins==0); Factory.address--;
 ReadError=true; assert(!Meter.Setup() && Begins==0); ReadError=false;
 const auto GoodRecord=Record; Record="{}"; assert(!Meter.Setup() && Begins==0); Record=GoodRecord;
 Record.replace(Record.find("\"current_gain\":0"),16,"\"current_gain\":1"); assert(!Meter.Setup() && Begins==0); Record=GoodRecord;
 assert(Meter.Setup() && Begins==1 && Tx.size()==36 && !Meter.Valid() && std::isnan(Meter.Voltage()));
 const uint8_t Regs[]={0x1D,0x1C,0x1D,0x19,0x14,0x1D};
 for(unsigned N=0;N<6;++N) { uint8_t Sum=0; for(unsigned I=0;I<5;++I)Sum+=Tx[N*6+I]; assert(Tx[N*6]==0xA8 && Tx[N*6+1]==Regs[N] && Tx[N*6+5]==uint8_t(~Sum)); }
 P=packet(200000,2300000,50000); Rx.push_back(0); Rx.insert(Rx.end(),P.begin(),P.begin()+10); Meter.Loop(); assert(!Meter.Valid());
 Rx.insert(Rx.end(),P.begin()+10,P.end()); Meter.Loop(); assert(Meter.Valid() && std::abs(Meter.Voltage()-230)<.001);
 P[22]^=1; Rx.insert(Rx.end(),P.begin(),P.end()); Meter.Loop(); assert(Meter.Valid());
 Now+=5000; Meter.Loop(); assert(!Meter.Valid() && std::isnan(Meter.Power()));
 P=packet(0,2300000,-50000); Now=0xFFFFFF00; Rx.insert(Rx.end(),P.begin(),P.end()); Meter.Loop(); assert(Meter.Valid() && Meter.Power()==-100);
 Now=0x100; assert(Meter.Valid()); Now=0x2000; assert(!Meter.Valid());
 Rx.insert(Rx.end(),P.begin(),P.begin()+4); Meter.Loop(); Now+=201; Meter.Loop();
 Rx.insert(Rx.end(),P.begin(),P.end()); Meter.Loop(); assert(Meter.Valid());
 assert(!Meter.CalibrationWritable());
 const auto OriginalCalibration=Record;
 assert(!Meter.SetCalibration(0,500000,200000000) && Record==OriginalCalibration);
 Factory.readonly=false;
 assert(Meter.CalibrationWritable());
 assert(Meter.SetCalibration(7100000,400000,180000000));
 assert(Record.find("\"done\":true")!=std::string::npos && Record.find("\"current_gain\":0")!=std::string::npos);
 assert(Record.find("\"voltage_scale\":7100000")!=std::string::npos);
 assert(Meter.VoltageCalibration()==7100000 && Meter.CurrentCalibration()==400000 && Meter.PowerCalibration()==180000000);
 const auto UpdatedCalibration=Record;
 FailWrite=1;
 assert(!Meter.SetCalibration(7200000,410000,181000000) && Record==UpdatedCalibration);
 Factory.readonly=true;
}
'''
    (p / 'test.cpp').write_text(test)
    subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-I', str(p), '-I', str(root / 'Sw'), '-I', str(root / 'generated/build/main/MainDependencies/libraries/ArduinoJson/src'),
                    str(p / 'test.cpp'), str(root / 'Sw/ShellyPowerMeter.cpp'), str(root / 'Sw/ShellyMeterPacket.cpp'),
                    '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
print('Actual BL0942 driver: factory rejection, UART initialization, checksums, signed power, fragmented RX, timeout and clock wrap passed; no hardware test')
