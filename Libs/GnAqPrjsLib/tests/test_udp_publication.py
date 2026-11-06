# test_udp_publication.py, Version: 1.03
"""Exercise actual publication loop without sockets, plus staged metering cycles."""
import pathlib, subprocess, tempfile
lib = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='aq-udp-publication-') as directory:
    tmp = pathlib.Path(directory)
    source = (lib/'GnUdpStatus.cpp').read_text().replace(' private:', ' public:')
    start = source.index('bool TGnUdpStatus::TImplementation::SendRaw(')
    end = source.index('\nvoid TGnUdpStatus::TImplementation::RememberTemperature', start)
    # Keep real parsing, serialization, signatures and Loop; replace only the send.
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
  const TGNString A = R"({"CurrentTime":1,"Channels":[{"State":false}],"PowerMeter":{"Voltage":230,"Current":1,"ActivePower":200,"Valid":true,"Diagnostics":{"CfEdges":1}},"Measurements":[{"Value":24,"UpdatedAt":1,"Valid":true}]})";
  const TGNString B = R"({"CurrentTime":2,"Channels":[{"State":false}],"PowerMeter":{"Voltage":231,"Current":2,"ActivePower":400,"Valid":true,"Diagnostics":{"CfEdges":999}},"Measurements":[{"Value":25,"UpdatedAt":2,"Valid":true}]})";
  assert(StatusEventSignature(A) == StatusEventSignature(B));
  TGnWireDocument Event(A);Event.Root().NewObject("ChargeLearning").UInt("Sequence",1);
  assert(StatusEventSignature(A)==StatusEventSignature(Event.Root().Json()));
  Event.Root().NewObject("HeatControl").Bool("FloorRequest",true);
  assert(StatusEventSignature(A)!=StatusEventSignature(Event.Root().Json()));
  auto changed = A; changed.replace(changed.find("false"),5,"true");
  assert(StatusEventSignature(A) != StatusEventSignature(changed));
  changed = A; changed.replace(changed.find("\"Valid\":true"),12,"\"Valid\":false");
  assert(StatusEventSignature(A) != StatusEventSignature(changed));
  auto Wire = BuildStatusJson("02:11:22:33:44:55@4", A, {});
  assert(Wire.find("Diagnostics") == TGNString::npos);
  assert(Wire.find("ActivePower") != TGNString::npos);
  assert(A.find("Diagnostics") != TGNString::npos);
  TGnUdpStatus::TImplementation U;
  U.FControlReady = true; // Socket-free publication harness.
  U.FReady = true; U.FNetworkCheckKnown = true; U.FNetworkConnected = true; U.FNetworkWasConnected = true;
  TGNString Current = A;
  U.FStatusProvider = [&](uint64_t & Signature) { Signature = 123; return Current; };
  auto check = [&]() { U.FStatusCheckDue = true; U.Loop(); };
  check(); assert(Sends == 1);
  Current = B; check(); assert(Sends == 1); // Measurement-only change stays quiet.
  U.FPublishDue = true; U.Loop(); assert(Sends == 2);
  Current = changed; check(); assert(Sends == 3); // Invalidity is immediate.
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
  std::cout << "Passed: 10s publication, measurement suppression, immediate state/validity, temperature completion, UDP-only diagnostic removal, complete/stale/invalid/wrapped meter cycles. No sockets or hardware.\n";
 }
''')
    command = ['g++','-std=c++20','-Wall','-Wextra','-Werror','-DSYSTEM_DUMMY',
               '-I'+str(tmp),'-I'+str(lib),'-I'+str(lib.parent/'GnBasicLib'),str(tmp/'test.cpp')]
    command += [str((lib.parent/'GnBasicLib' if name in ('GnBasics','GnTime','GnCoopTimer') else lib)/(name+'.cpp')) for name in ['GnBasics','GnTime','GnCoopTimer','GnDeviceIdentity']]
    subprocess.run(command+['-ljson-c','-lpthread','-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
