# test_udp_publication.py, Version: 1.05
"""Exercise actual publication loop without sockets, plus staged metering cycles."""
import pathlib, subprocess, tempfile
lib = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='aq-udp-publication-') as directory:
    tmp = pathlib.Path(directory)
    source = (lib/'GnUdpStatus.cpp').read_text().replace(' private:', ' public:')
    start = source.index('bool TGnUdpStatus::TImplementation::SendRaw(')
    end = source.index('\nvoid TGnUdpStatus::TImplementation::RememberTemperature', start)
    # Keep the real receive and publication loop; replace only the socket send.
    source = source[:start] + '''unsigned Sends = 0;
TGNString LastPacket;
bool TGnUdpStatus::TImplementation::SendRaw( TGNString const & Payload )
 { ++Sends; LastPacket = Payload; return true; }
''' + source[end:]
    (tmp/'GnUdpStatus.h').write_text((lib/'GnUdpStatus.h').read_text().replace(' private:', ' public:'))
    (tmp/'Udp.cpp').write_text(source)
    (tmp/'test.cpp').write_text(r'''
#include <GnBasics.h>
#include <GnPowerMeterCycle.h>
#include "Udp.cpp"
#include <cassert>
#include <iostream>
int main(void)
 {
  static_assert(GnUdpStatusPublishIntervalMs == 10000);
  const TGNString A = R"({"Ver":3,"Type":2,"ID":"02:11:22:33:44:55@4","Compact":true,"InfoRev":"0123456789abcdef","Status":{}})";
  const TGNString B = R"({"Ver":3,"Type":2,"ID":"02:11:22:33:44:55@4","Compact":true,"InfoRev":"0123456789abcdef","Status":{"Temp":25}})";
  TGnUdpStatus::TImplementation U;
  U.FControlReady = true; // Socket-free publication harness.
  U.FReady = true; U.FNetworkCheckKnown = true; U.FNetworkConnected = true; U.FNetworkWasConnected = true;
  TGNString Current = A;
  uint64_t CurrentSignature = 1;
  U.FCompactStatusProvider = [&](uint64_t & Signature,TGnUdpInfoParts * Parts,TGNString & Packet)
   {
    Signature = CurrentSignature;
    if(Parts){Parts->clear();Parts->emplace("Root","{}");}
    Packet = Current;
    return true;
   };
  auto check = [&]() { U.FStatusCheckDue = true; U.Loop(); };
  check(); assert(Sends == 1);
  Current = B; check(); assert(Sends == 1); // Same operational signature stays quiet.
  U.FPublishDue = true; U.Loop(); assert(Sends == 2);
  ++CurrentSignature;Current = B; check(); assert(Sends == 3); // State/validity signature changes publish immediately.
  TGnUdpTemperature Temp{true,true,24,1};
  U.FTemperatureProvider = [&]() { return Temp; };
  check(); assert(Sends == 4);
  Temp.Celsius = 26; check(); assert(Sends == 4); // No value threshold.
  U.FPublishDue = true; U.Loop(); assert(Sends == 4); // No stale sample just before completion.
  ++Temp.SampleRevision; check(); assert(Sends == 5); // Completed read, even same value.
  ++Temp.SampleRevision; check(); assert(Sends == 6);
  Temp.Valid = false; check(); assert(Sends == 7);
  TGnPowerMeterCycle C;
  using M = TGnPowerMeterCycle;
  C.Add(M::Voltage,230,true,0); C.Add(M::Power,100,true,1000);
  assert(!C.Valid(1000) && C.Sample().Voltage == 0);
  C.Add(M::Current,1,true,2500); assert(C.Valid(2500));
  assert(C.Sample().Voltage == 230 && C.Sample().Current == 1 && C.Sample().Power == 100);
  C.Add(M::Voltage,240,true,5000); assert(C.Sample().Voltage == 230);
  C.Add(M::Power,200,true,6000); C.Add(M::Current,2,false,7500);
  assert(!C.Valid(7500) && C.Sample().Voltage == 230); // Invalid cycle never overwrites snapshot.
  C.Add(M::Current,0,true,8000); C.Add(M::Voltage,235,true,9000);
  C.Add(M::Power,0,true,10000); assert(C.Valid(10000) && C.Sample().Current == 0);
  assert(!C.Valid(25000));
  C.Reset(); C.Add(M::Voltage,220,true,0); C.Add(M::Current,1,true,8000);
  C.Add(M::Power,100,true,9000); assert(!C.Valid(9000)); // Stale partial cycle discarded.
  C.Add(M::Voltage,225,true,10000); assert(C.Valid(10000));
  C.Reset(); C.Add(M::Voltage,230,true,UINT32_MAX-2000);
  C.Add(M::Current,1,true,UINT32_MAX-1000); C.Add(M::Power,100,true,100);
  assert(C.Valid(200));
  std::cout << "Passed: 10s publication, signature suppression, immediate signature changes, temperature completion, complete/stale/invalid/wrapped meter cycles. No sockets or hardware.\n";
 }
''')
    command = ['g++','-std=c++20','-Wall','-Wextra','-Werror','-DSYSTEM_DUMMY',
               '-I'+str(tmp),'-I'+str(lib),'-I'+str(lib.parent/'GnBasicLib'),str(tmp/'test.cpp')]
    command += [str((lib.parent/'GnBasicLib' if name in ('GnBasics','GnTime','GnCoopTimer') else lib)/(name+'.cpp')) for name in ['GnBasics','GnTime','GnCoopTimer','GnDeviceIdentity']]
    subprocess.run(command+['-ljson-c','-lpthread','-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
