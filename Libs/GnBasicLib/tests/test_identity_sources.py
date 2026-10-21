# test_identity_sources.py, Version: 1.09
"""Compile actual native identity/cache/heater/sync code and test without networking.
Only private access is opened in temporary copies to inject packets directly;
production parsing and cache functions are unchanged. No sockets are opened.
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
TGNString Envelope( TGNString Id, TGNString Sources, TGNString Revision, TGNString Mode = "MAC", TGNString Function = "TestConsumer" )
 { return "{\"Identity\":{\"Version\":1,\"DeviceId\":\"" + Id + "\",\"Mode\":\"" + Mode + "\",\"Function\":\"" + Function + "\",\"Revision\":\"" + Revision + "\",\"Sources\":" + Sources + "}}"; }
TGNString Packet( TGNString Id, TGNString Temperature, TGNString Inner = "" )
 { return "{\"Version\":2,\"Type\":\"Status\",\"ID\":\"" + Id + "\",\"Status\":{\"Temperature\":" + Temperature + ",\"Identity\":{\"DeviceId\":\"" + Inner + "\"}}}"; }
int main( int argc, char ** argv )
 {
  assert( argc == 2 );
  for(int Version : {0,1,3}) {
    TParsedPacket Unsupported;
    assert(!ParsePacket("{\"Version\":" + std::to_string(Version) + ",\"Type\":\"Status\",\"ID\":\"02:11:22:33:44:55@3\",\"Value\":40,\"Status\":{\"Temperature\":40}}",Unsupported));
    assert(!ParsePacket("{\"Version\":" + std::to_string(Version) + ",\"Type\":\"Request\",\"ID\":[]}",Unsupported));
  }
  auto CheckPublished = [](TGnUdpTemperature Measurement, bool Present, bool Valid) {
    auto Text = BuildStatusJson("02:11:22:33:44:55@3", "{\"Kind\":\"Feeder\",\"Marker\":123}", Measurement);
    auto Root = json_tokener_parse(Text.c_str()); json_object * Field = nullptr, * Status = nullptr;
    assert(json_object_get_int(json_object_object_get(Root,"Version"))==2);
    assert(!json_object_object_get_ex(Root,"Value",&Field));
    assert(!json_object_object_get_ex(Root,"Temperature",&Field));
    assert(json_object_object_get_ex(Root,"Status",&Status));
    assert(json_object_get_int(json_object_object_get(Status,"Marker"))==123);
    assert(TGNString(json_object_get_string(json_object_object_get(Status,"Kind")))=="Feeder");
    assert(json_object_object_get_ex(Status,"Temperature",&Field)==Present);
    if(Present) {
      if(Valid) assert(json_object_get_double(Field)==28.5);
      else assert(Field==nullptr);
    }
    TParsedPacket Parsed; assert(ParsePacket(Text,Parsed) && Parsed.Version==2);
    if(Present) assert(std::any_of(Parsed.Measurements.begin(),Parsed.Measurements.end(),[&](auto const & M){return M.Field=="Temperature" && M.Valid==Valid;}));
    json_object_put(Root);
  };
  CheckPublished({true,true,28.5F},true,true);
  CheckPublished({true,false,0},true,false);
  CheckPublished({},false,false);
  assert(BuildStatusJson("02:11:22:33:44:55@3","[]",{}).empty());
  assert(BuildStatusJson("02:11:22:33:44:55@3","bad",{}).empty());
  auto Request = json_tokener_parse(BuildRequestJson({}).c_str());
  assert(json_object_get_int(json_object_object_get(Request,"Version"))==2);
  json_object_put(Request);

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
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( Id, Sources, Revision, "Legacy" ) ) );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( "02:AA:BB:CC:DD:EE@1", Sources, Next ) ) );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( Id, "{\"FeederAq1\":\"00:00:00:00:00:00@3\"}", Next ) ) );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( Id, "{\"FeederAq1\":\"02:11:22:33:44:55@4\"}", Next ) ) );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( Id, Other, Revision ) ) );
  assert( !GnDeviceIdentity.ApplyEnvelope( Envelope( Id, Sources, Revision, "MAC", "ConflictingFunction" ) ) );
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
  assert( Reloaded.ReportJson().find( "\"LocalSources\":false" ) != TGNString::npos );
  assert( GnDeviceIdentity.ApplyEnvelope( Prepare ) );
  TGnDeviceIdentity Failed;
  Failed.Setup( "TestConsumer", 1, ( Directory / "missing" / "Identity.json" ).string() );
  assert( !Failed.ApplyEnvelope( Envelope( Failed.DeviceId(), Sources, Revision ) ) );
  assert( Failed.ResolveTemperatureSource( "FeederAq1" ).empty() );
  assert( Failed.ReportJson().find( "\"Prepared\":false" ) != TGNString::npos );
  auto & Cache = *GnUdpStatus.FImplementation;
  float Temperature = 0;
  Cache.HandlePacket( Packet( "FeederAq1", "27.5", First ) );
  assert( !GnUdpStatus.GetTemperature( First, Temperature ) );
  assert( !GnUdpStatus.GetTemperature( "FeederAq1", Temperature ) );
  Cache.HandlePacket( Packet( First, "27.5" ) );
  assert( GnUdpStatus.GetTemperature( First, Temperature ) && Temperature == 27.5F );
  Cache.HandlePacket("{\"Version\":2,\"Type\":\"Status\",\"ID\":\"" + First + "\",\"Value\":99,\"Temperature\":99,\"Status\":{\"Temperature\":40}}");
  assert(GnUdpStatus.GetTemperature(First, Temperature) && Temperature==40);
  Cache.HandlePacket("{\"Version\":2,\"Type\":\"Status\",\"ID\":\"" + First + "\",\"Value\":99,\"Temperature\":99,\"Status\":{\"Temperature\":null}}");
  assert(!GnUdpStatus.GetTemperature(First, Temperature));
  Cache.HandlePacket(Packet(First, "27.5"));
  THeaterProfiles Profiles{ { "HeatAq1", "FeederAq1", 24, 25, 24, 25, true } };
  TGnHeaterControl Heater( Profiles ); bool Known = false, State = true;
  assert( !Heater.AutomaticState( THeaterRole::HeaterMain, "HeatAq1", false, false, Known, State ) );
  Cache.HandlePacket( Packet( First, "23" ) );
  assert( Heater.AutomaticState( THeaterRole::HeaterMain, "HeatAq1", false, false, Known, State ) );
  Cache.HandlePacket( Packet( First, "null" ) );
  assert( !GnUdpStatus.GetTemperature( First, Temperature ) );
  assert( Heater.AutomaticState( THeaterRole::HeaterMain, "HeatAq1", false, false, Known, State ) );
  Cache.HandlePacket( Packet( Second, "21", First ) );
  assert( !GnUdpStatus.GetTemperature( First, Temperature ) );
  THeaterProfiles ChargeProfiles{ { "Charge", "FeederAq1", 40, 80, 40, 80, false } };
  TGnHeaterControl Charge( ChargeProfiles ); bool ChargeKnown = false, ChargeState = false;
  ChargeProfiles[0].Measurement = "BatteryPercent";
  auto LaptopV2 = [&](TGNString Fields, int Version=2) {
    Cache.HandlePacket("{\"Version\":" + std::to_string(Version) + ",\"Type\":\"Status\",\"ID\":\"" + First + "\",\"Value\":99,\"Status\":{" + Fields + "}}");
  };
  LaptopV2("\"Kind\":\"LaptopPower\",\"BatteryPercent\":35,\"Temperature\":99,\"AcOnline\":true");
  assert(Charge.AutomaticState(THeaterRole::HeaterMain,"Charge",false,false,ChargeKnown,ChargeState));
  assert(GnUdpStatus.GetMeasurement(First,"BatteryPercent",Temperature) && Temperature==35);
  assert(GnUdpStatus.GetTemperature(First,Temperature) && Temperature==99);
  for(auto Fields : {"\"BatteryPercent\":null", "\"BatteryPercent\":true", "\"BatteryPercent\":\"35\"", "\"Temperature\":35"}) {
    LaptopV2(Fields);
    assert(!Charge.AutomaticState(THeaterRole::HeaterMain,"Charge",false,false,ChargeKnown,ChargeState));
    assert(!GnUdpStatus.GetMeasurement(First,"BatteryPercent",Temperature));
  }
  LaptopV2("\"BatteryPercent\":85");
  assert(!Charge.AutomaticState(THeaterRole::HeaterMain,"Charge",false,false,ChargeKnown,ChargeState));
  LaptopV2("\"BatteryPercent\":35",1);
  assert(!Charge.AutomaticState(THeaterRole::HeaterMain,"Charge",false,false,ChargeKnown,ChargeState));
  LaptopV2("\"BatteryPercent\":35",3);
  assert(!Charge.AutomaticState(THeaterRole::HeaterMain,"Charge",false,false,ChargeKnown,ChargeState));
  LaptopV2("\"BatteryPercent\":35");
  assert(Charge.AutomaticState(THeaterRole::HeaterMain,"Charge",false,false,ChargeKnown,ChargeState));
  std::ofstream( Directory / "SwitchTimes.sync.json" ) << "{\"TimeStamp\":\"2026-10-13T12:00:00.000Z\",\"Synced\":true}";
  TTimeConfigSync Sync( "TestConsumer", Directory.string() );
  TWebApplication Application;
  Sync.Application( &Application );
  const TGNString Stamp = "2026-10-13T12:00:00.000Z";
  const TGNString Push = "{\"Device\":\"" + Id + "\",\"Type\":\"SwitchTimes\",\"Format\":\"MyTimer\",\"TimeStamp\":\"" + Stamp + "\",\"Content\":{}," + Envelope( Id, Other, Next ).substr( 1 );
  TGNString Reply;
  assert( Sync.HandleIncoming( Push, Reply ) == 200 );
  assert( GnDeviceIdentity.ResolveTemperatureSource( "FeederAq1" ) == Second );
  const auto Ack = GnDeviceIdentity.ReportJson();
  assert( Ack.find( "\"AppliedRevision\":\"" + Next + "\"" ) != TGNString::npos );
  TGNString WrongPush = Push;
  WrongPush.replace( WrongPush.find( Id ), Id.size(), "TestConsumer" );
  assert( Sync.HandleIncoming( WrongPush, Reply ) == 400 );
  assert( Sync.HandleIncoming( "{\"TimeStamp\":\"" + Stamp + "\",\"Content\":{}}", Reply ) == 400 );
  assert( !GnDeviceIdentity.ApplyEnvelope( Prepare ) );
  std::cout << "Passed: MAC-only IDs/no legacy cache, persistence/reboot generation, local source lock/validation/reload, Dashboard restoration at same revision, no source fallback, heater behavior, equal-timestamp assignment/ack and target validation. No device network/hardware used.\n";
 }
''')
    command = ['g++', '-std=c++20', '-O1', '-Wall', '-Wextra', '-Werror', '-DSYSTEM_DUMMY',
               '-I'+str(temp), '-I'+str(lib), '-I'+str(multitimer/'include'), str(temp/'test.cpp')]
    command += [str(lib/(name+'.cpp')) for name in ['GnBasics','GnTime','GnCoopTimer','GnDeviceIdentity','GnHeaterControl']]
    command += [str(multitimer/'src/TimeConfigSync.cpp'), '-ljson-c', '-lpthread', '-o', str(temp/'test')]
    subprocess.run(command, check=True)
    subprocess.run([str(temp/'test'), str(temp)], check=True)
