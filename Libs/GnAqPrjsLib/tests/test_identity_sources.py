# test_identity_sources.py, Version: 1.16
"""Compile actual native identity, UDP cache, charge hysteresis and time sync code and test without networking.
Temporary copies expose the UDP receiver and provide a fixed local MAC.
Production validation, persistence and cache functions run without sockets.
"""
import pathlib, tempfile, subprocess
lib = pathlib.Path(__file__).resolve().parents[1]
multitimer = lib.parents[1] / 'MultiTimer'
with tempfile.TemporaryDirectory(prefix='aq-identity-test-') as temporary:
    temp = pathlib.Path(temporary)
    header = (lib/'GnUdpStatus.h').read_text().replace(' private:', ' public:')
    (temp/'GnUdpStatus.h').write_text(header)
    implementation = (lib/'GnUdpStatus.cpp').read_text().replace(' private:', ' public:')
    (temp/'Udp.cpp').write_text(implementation)
    identity = (lib/'GnDeviceIdentity.cpp').read_text()
    mac_start = identity.index('TGNString LocalMac(')
    mac_end = identity.index('void FillState(', mac_start)
    (temp/'Identity.cpp').write_text(
        identity[:mac_start] + 'TGNString LocalMac(void) { return "02:00:00:00:00:01"; }\n\n'
        + identity[mac_end:])

    (temp/'WebApplication.h').write_text('#pragma once\n#include <GnBasics.h>\n#include <TimeConfigSync.h>\nclass TWebApplication { public: bool ApplyRemoteTimes(TGNString const &, TGNString const &, TGNString &); };\n')
    (temp/'test.cpp').write_text(r'''
#include <GnBasics.h>
#include <GnDeviceIdentity.h>
#include <GnHeaterControl.h>
#include "Udp.cpp"
#include "WebApplication.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <iostream>
unsigned AppliedContent = 0;
bool TWebApplication::ApplyRemoteTimes( const TGNString &, const TGNString &, TGNString & )
 { ++AppliedContent; return true; }
TGNString Envelope( TGNString Id, TGNString Sources, TGNString Revision,
                    TGNString Function = "TestConsumer", unsigned Version = 1 )
 {
  return "{\"Identity\":{\"Ver\":" + std::to_string(Version) + ",\"ServiceId\":\"" + Id +
    "\",\"Func\":\"" + Function + "\",\"Rev\":\"" + Revision + "\",\"Srcs\":" + Sources + "}}";
 }
TGNString Packet( TGNString const & Id, TGNString const & Fields, unsigned Version = GnUdpStatusVersion )
 {
  return "{\"Ver\":" + std::to_string(Version) + ",\"Type\":" +
    std::to_string(static_cast<unsigned>(TMsgType::Status)) + ",\"ID\":\"" + Id +
    "\",\"Status\":{" + Fields + "}}";
 }
int main( int argc, char ** argv )
 {
  assert( argc == 2 );
  for( unsigned Version : {0U, GnUdpStatusVersion - 1U, GnUdpStatusVersion + 1U} )
   {
    TParsedPacket Unsupported;
    assert( !ParsePacket(Packet("02:11:22:33:44:55@3","\"Temp\":40",Version),Unsupported) );
   }
  TParsedPacket Parsed;
  assert( ParsePacket(BuildRequestJson({}),Parsed) && Parsed.Type == TPacketType::Request );

  const std::filesystem::path Directory = argv[1];
  const TGNString File = ( Directory / "Identity.json" ).string();
  const TGNString First = "02:11:22:33:44:55@3", Second = "02:66:77:88:99:AA@3";
  const TGNString Sources = "{\"FeederAq1\":\"" + First + "\"}", Other = "{\"FeederAq1\":\"" + Second + "\"}";
  const TGNString Revision = "2026-10-13T00:00:00.000Z", Next = "2026-10-13T00:00:01.000Z";
  GnDeviceIdentity.Setup( "TestConsumer", 1, File );
  const auto Id = GnDeviceIdentity.DeviceId();
  assert( Id.length() == 19 && GnDeviceIdentity.WireId() == Id );
  assert( GnDeviceIdentity.ResolveTemperatureSource( "FeederAq1" ).empty() );
  assert( !GnDeviceIdentity.ApplyEnvelope( "{\"Action\":\"Synced\"}" ) );
  assert( std::filesystem::exists( File ) );
  const auto Prepare = Envelope( Id, Sources, Revision );
  assert( GnDeviceIdentity.ApplyEnvelope( Prepare ) );
  assert( GnDeviceIdentity.CommitAssignment() );
  const auto Modified = std::filesystem::last_write_time( File );
  assert( GnDeviceIdentity.ApplyEnvelope( Prepare ) );
  assert( Modified == std::filesystem::last_write_time( File ) );
  assert( GnDeviceIdentity.ResolveTemperatureSource( "FeederAq1" ) == First );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( Id, Sources, Revision, "TestConsumer", 2 ) ) );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( "02:AA:BB:CC:DD:EE@1", Sources, Next ) ) );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( Id, "{\"FeederAq1\":\"00:00:00:00:00:00@3\"}", Next ) ) );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( Id, "{\"FeederAq1\":\"02:11:22:33:44:55@4\"}", Next ) ) );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( Id, Other, Revision ) ) );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( Id, Sources, Revision, "ConflictingFunction" ) ) );
  assert( !GnDeviceIdentity.SetSources( Other, true ) );
  assert( !GnDeviceIdentity.SetSources( "{\"x\":\"00:00:00:00:00:00@3\"}", false ) );
  assert( GnDeviceIdentity.SetSources( Other, false ) );
  assert( GnDeviceIdentity.ResolveTemperatureSource( "FeederAq1" ) == Second );
  const auto Boot = GnDeviceIdentity.BootCounter();
  TGnDeviceIdentity Reloaded;
  Reloaded.Setup( "RenamedHost", 1, File );
  assert( Reloaded.BootCounter() > Boot && Reloaded.ResolveTemperatureSource( "FeederAq1" ) == Second );
  assert( Reloaded.ApplyEnvelope( Prepare ) );
  assert( Reloaded.ResolveTemperatureSource( "FeederAq1" ) == First );
  assert( Reloaded.ReportJson().find( "\"LocalSrcs\":false" ) != TGNString::npos );
  assert( GnDeviceIdentity.ApplyEnvelope( Prepare ) );
  TGnDeviceIdentity Failed;
  Failed.Setup( "TestConsumer", 1, ( Directory / "missing" / "Identity.json" ).string() );
  assert( !Failed.ApplyEnvelope( Envelope( Failed.DeviceId(), Sources, Revision ) ) );
  assert( Failed.ResolveTemperatureSource( "FeederAq1" ).empty() );
  assert( Failed.ReportJson().find( "\"Prepared\":false" ) != TGNString::npos );
  auto & Cache = *GnUdpStatus.FImplementation;
  float Temperature = 0;
  Cache.HandlePacket( Packet("FeederAq1","\"Temp\":27.5") );
  assert( !GnUdpStatus.GetTemperature( First, Temperature ) );
  assert( !GnUdpStatus.GetTemperature( "FeederAq1", Temperature ) );
  Cache.HandlePacket( Packet(First,"\"Temp\":27.5") );
  assert( GnUdpStatus.GetTemperature( First, Temperature ) && Temperature == 27.5F );
  assert( !GnUdpStatus.GetTemperature( "FeederAq1", Temperature ) );
  Cache.HandlePacket( Packet(First,"\"Temp\":null") );
  Cache.HandlePacket( Packet(Second,"\"Temp\":21") );
  assert( !GnUdpStatus.GetTemperature( First, Temperature ) );
  assert( GnUdpStatus.GetTemperature( Second, Temperature ) && Temperature == 21 );

  THeaterProfile Profile;
  Profile.Id = "Charge";
  Profile.TempId = "FeederAq1";
  Profile.Measurement = "BatteryPercent";
  Profile.ChargeStartPercent = 40;
  Profile.ChargeStopPercent = 80;
  TGnHeaterControl Charge;
  {
    const THeaterProfiles Profiles{ Profile };
    Charge.Profiles( Profiles );
  } // The controller must retain its snapshot after the caller's vector expires.
  bool ChargeKnown = false, ChargeState = false;
  auto Charging = [&]()
   {
    return Charge.AutomaticState(THeaterRole::HeaterMain,"Charge",false,false,ChargeKnown,ChargeState);
   };
  auto Laptop = [&](TGNString Fields, unsigned Version = GnUdpStatusVersion)
   {
    Cache.HandlePacket( Packet(First,Fields,Version) );
   };
  Laptop("\"Kind\":\"LaptopPower\",\"BatteryPct\":35,\"Temp\":99,\"AcOnline\":true");
  assert( Charging() );
  assert( GnUdpStatus.GetMeasurement(First,"BatteryPercent",Temperature) && Temperature == 35 );
  assert( GnUdpStatus.GetTemperature(First,Temperature) && Temperature == 99 );
  for( auto Fields : {"\"BatteryPct\":null", "\"Temp\":35"} )
   {
    Laptop(Fields);
    assert( !Charging() && !GnUdpStatus.GetMeasurement(First,"BatteryPercent",Temperature) );
   }
  for( auto Fields : {"\"BatteryPct\":true", "\"BatteryPct\":\"35\"", "\"BatteryPct\":101"} )
   {
    Laptop(Fields);
    assert( !GnUdpStatus.GetMeasurement(First,"BatteryPercent",Temperature) );
   }
  Laptop("\"BatteryPct\":85");
  assert( !Charging() );
  Laptop("\"BatteryPct\":35",GnUdpStatusVersion - 1);
  Laptop("\"BatteryPct\":35",GnUdpStatusVersion + 1);
  assert( !Charging() );
  Laptop("\"BatteryPct\":35,\"ChargeLearnAllowed\":true,\"BootCnt\":16777217");
  assert( Charging() );
  auto Learning = GnUdpStatus.ChargeLearningPolicy(First);
  assert( Learning.Allowed && Learning.BootCounter == 16777217 ); // Keep exact 32-bit counters.
  assert( !GnUdpStatus.ChargeLearningPolicy(Second).Allowed );
  Laptop("\"BatteryPct\":35,\"ChargeLearnAllowed\":false,\"BootCnt\":2");
  Learning = GnUdpStatus.ChargeLearningPolicy(First);
  assert( Learning.Available && !Learning.Allowed && Learning.BootCounter == 2 );
  for( auto Fields : {"\"ChargeLearnAllowed\":true",
                      "\"ChargeLearnAllowed\":true,\"BootCnt\":4294967296"} )
   {
    Laptop(TGNString("\"BatteryPct\":35,") + Fields);
    assert( !GnUdpStatus.ChargeLearningPolicy(First).Available );
    assert( GnUdpStatus.GetMeasurement(First,"BatteryPercent",Temperature) && Temperature == 35 );
   }
  Laptop("\"BatteryPct\":35,\"ChargeLearnAllowed\":true,\"BootCnt\":0");
  Learning = GnUdpStatus.ChargeLearningPolicy(First);
  assert( Learning.Available && Learning.Allowed && Learning.BootCounter == 0 );
  Laptop("\"BatteryPct\":35");
  assert( !GnUdpStatus.ChargeLearningPolicy(First).Available );
  std::ofstream( Directory / "SwitchTimes.sync.json" ) << "{\"TimeStamp\":\"2026-10-13T12:00:00.000Z\",\"Synced\":true}";
  TTimeConfigSync Sync( "TestConsumer", Directory.string() );
  TWebApplication Application;
  Sync.Application( &Application );
  const TGNString Stamp = "2026-10-13T12:00:00.000Z";
  const TGNString Push = "{\"Dev\":\"" + Id + "\",\"Type\":\"SwitchTimes\",\"Format\":\"MyTimer\",\"TimeStamp\":\"" + Stamp + "\",\"Content\":{\"SwitchTimes\":[]}," + Envelope( Id, Other, Next ).substr( 1 );
  TGNString Reply;
  assert( Sync.HandleIncoming( Push, Reply ) == 200 );
  assert( AppliedContent == 1 ); // A pending assignment applies the complete bundle even at equal time.
  TGNString Confirmation=Push;
  const auto ContentStart=Confirmation.find("{\"SwitchTimes\":[]}");
  assert(ContentStart!=TGNString::npos);
  Confirmation.replace(ContentStart,TGNString("{\"SwitchTimes\":[]}").size(),"{}");
  assert(Sync.HandleIncoming(Confirmation,Reply)==200);
  assert(AppliedContent==1); // Once committed, same revision only confirms identity.

  assert( GnDeviceIdentity.ResolveTemperatureSource( "FeederAq1" ) == Second );
  const auto Ack = GnDeviceIdentity.ReportJson();
  assert( Ack.find( "\"AppliedRev\":\"" + Next + "\"" ) != TGNString::npos );
  TGNString WrongPush = Push;
  WrongPush.replace( WrongPush.find( Id ), Id.size(), "TestConsumer" );
  assert( Sync.HandleIncoming( WrongPush, Reply ) == 400 );
  assert( Sync.HandleIncoming( "{\"TimeStamp\":\"" + Stamp + "\",\"Content\":{}}", Reply ) == 400 );
  assert( !GnDeviceIdentity.ApplyEnvelope( Prepare ) );
  std::cout << "Passed: MAC-only IDs/no legacy cache, persistence/reboot generation, local source lock/validation/reload, Dashboard restoration at same revision, no source fallback, charge hysteresis and learning telemetry, equal-timestamp assignment/ack and target validation. No device network/hardware used.\n";
 }
''')
    command = ['g++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror', '-DSYSTEM_DUMMY',
               '-I'+str(temp), '-I'+str(lib),'-I'+str(lib.parent/'GnBasicLib'), '-I'+str(multitimer/'include'), str(temp/'test.cpp')]
    command += [str((lib.parent/'GnBasicLib' if name in ('GnBasics','GnTime','GnCoopTimer') else lib)/(name+'.cpp')) for name in ['GnBasics','GnTime','GnCoopTimer','GnHeaterControl']]
    command += [str(temp/'Identity.cpp'), str(multitimer/'src/TimeConfigSync.cpp'), '-ljson-c', '-lpthread', '-o', str(temp/'test')]
    subprocess.run(command, check=True)
    subprocess.run([str(temp/'test'), str(temp)], check=True)
