# test_room_temperature.py, Version: 1.04
"""Real asynchronous reader against fake sysfs; no GPIO or network access."""
from pathlib import Path
import subprocess,tempfile
project=Path(__file__).resolve().parents[1];lib=project.parent/'Libs/GnBasicLib'
with tempfile.TemporaryDirectory(prefix='room-temperature-') as name:
    tmp=Path(name)
    (tmp/'RoomTemperature.h').write_text((project/'include/RoomTemperature.h').read_text().replace(' private:',' public:').replace(' protected:',' public:'))
    (tmp/'Reader.cpp').write_text((project/'src/RoomTemperature.cpp').read_text())
    (tmp/'StatusApi.h').write_text((project/'include/StatusApi.h').read_text())
    (tmp/'Status.cpp').write_text((project/'src/StatusApi.cpp').read_text())
    # Use a fixed test MAC; this sensor test does not depend on host routing.
    identity=(lib.parent/'GnAqPrjsLib/GnDeviceIdentity.cpp').read_text()
    start=identity.index('TGNString LocalMac(void)')
    end=identity.index('TGNString StateJson(',start)
    identity=identity[:start]+'TGNString LocalMac(void) { return "02:11:22:33:44:55"; }\n'+identity[end:]
    (tmp/'Identity.cpp').write_text(identity)
    (tmp/'test.cpp').write_text(r'''
#include <GnBasics.h>
#include "RoomTemperature.h"
#include "StatusApi.h"
#include <GnDeviceIdentity.h>
#include <json-c/json.h>
#include <cassert>
#include <fstream>
#include <thread>
#include <sys/stat.h>
int main(int argc,char ** argv)
 {
  assert(argc==2); const auto Root=std::filesystem::path(argv[1])/"sysfs";
  std::filesystem::create_directories(Root/"28-0123456789ab");
  const auto File=Root/"28-0123456789ab/w1_slave";
  auto write=[&](char const * Text){std::ofstream(File)<<Text;};
  write("aa bb cc : crc=aa YES\naa bb cc t=23437\n");
  auto Value=TRoomTemperature::Read(Root,"");assert(Value.Valid && Value.Celsius>23.43F && Value.Celsius<23.44F && Value.UpdatedAt>0);
  write("aa bb : crc=aa NO\naa bb t=23437\n");assert(!TRoomTemperature::Read(Root,"").Valid);
  write("aa bb : crc=aa YES\naa bb t=23437junk\n");assert(!TRoomTemperature::Read(Root,"").Valid);
  write("aa bb : crc=aa YES\naa bb t=125001\n");assert(!TRoomTemperature::Read(Root,"").Valid);
  write("aa bb : crc=aa YES\naa bb t=-55000\n");assert(TRoomTemperature::Read(Root,"").Valid);
  assert(!TRoomTemperature::Read(Root,"../escape").Valid);
  std::filesystem::create_directories(Root/"28-abcdef012345");assert(!TRoomTemperature::Read(Root,"").Valid);
  assert(TRoomTemperature::Read(Root,"28-0123456789ab").Valid);
  std::filesystem::remove(Root/"28-abcdef012345");
  const auto Config=std::filesystem::path(argv[1])/"Config.json";
  std::ofstream(Config)<<R"({"RoomTemperature":{"Enabled":true}})";
  TGNString Error;
  TRoomTemperature Reader(Root,std::chrono::hours(24));assert(Reader.Setup(Config,Error));
  std::filesystem::remove(File);assert(mkfifo(File.c_str(),0600)==0);
  std::thread Writer([&](){std::this_thread::sleep_for(std::chrono::milliseconds(150));write("aa bb : crc=aa YES\naa bb t=24000\n");});
  const auto Start=std::chrono::steady_clock::now(); Reader.Loop();
  assert(std::chrono::steady_clock::now()-Start<std::chrono::milliseconds(100));
  assert(!Reader.Sample().Valid);
  for(unsigned I=0;I<500 && !Reader.Sample().Valid;++I){Reader.Loop();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
  Writer.join();assert(Reader.Sample().Valid && Reader.Sample().Celsius==24);
  const auto First=Reader.UdpTemperature();assert(First.Available && First.Valid && First.SampleRevision==1);
  TChannelConfigs Channels; THeaterProfiles Profiles; TScheduler Scheduler(IHardware,Channels,Profiles);
  GnDeviceIdentity.Setup("Test",1,(std::filesystem::path(argv[1])/"Identity.json").string());
  TStatusApi Api(IHardware,Scheduler,Channels,"Test","Test",8081,{},&Reader);
  auto Json=Api.StatusJson();auto Document=json_tokener_parse(Json.c_str());assert(Document);
  auto Measurements=json_object_object_get(Document,"Measurements");assert(json_object_array_length(Measurements)==1);
  auto Room=json_object_array_get_idx(Measurements,0);
  assert(std::string(json_object_get_string(json_object_object_get(Room,"Id")))=="RoomTemperature");
  assert(json_object_get_double(json_object_object_get(Room,"Value"))==24);
  assert(json_object_get_boolean(json_object_object_get(Room,"Valid")));json_object_put(Document);
  Reader.FCompletedAt-=std::chrono::seconds(21);assert(!Reader.UdpTemperature().Valid);
  Document=json_tokener_parse(Api.StatusJson().c_str());assert(Document);
  Room=json_object_array_get_idx(json_object_object_get(Document,"Measurements"),0);
  assert(!json_object_get_boolean(json_object_object_get(Room,"Valid")));
  assert(json_object_is_type(json_object_object_get(Room,"Value"),json_type_null));json_object_put(Document);
  // Once bound, a removed sensor cannot be silently replaced by another one.
  assert(Reader.FSensorId=="28-0123456789ab");
  std::ofstream(Config)<<R"({"RoomTemperature":{"Enabled":"yes"}})";
  TRoomTemperature Bad(Root);assert(!Bad.Setup(Config,Error));
  std::ofstream(Config)<<"{}";TRoomTemperature Optional(Root);assert(Optional.Setup(Config,Error) && !Optional.Enabled());
 }
''')
    command=['g++','-std=c++20','-Wall','-Wextra','-Werror','-DSYSTEM_DUMMY','-I'+str(tmp),'-I'+str(project/'include'),'-I'+str(lib),'-I'+str(lib.parent/'GnAqPrjsLib'),str(tmp/'test.cpp'),str(tmp/'Reader.cpp'),str(tmp/'Status.cpp')]
    command += [str(tmp/'Identity.cpp') if n=='GnDeviceIdentity' else str((lib.parent/'GnAqPrjsLib' if n in ('GnHeaterControl','GnDeviceIdentity','GnUdpStatus') else lib)/(n+'.cpp')) for n in ['GnBasics','GnTime','GnCoopTimer','GnHeaterControl','GnDeviceIdentity','GnUdpStatus']]
    command += [str(project/'src'/(n+'.cpp')) for n in ['Scheduler','Logger','TimeSync','HardwareDummy']]
    subprocess.run(command+['-ljson-c','-lpthread','-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test'),str(tmp)],check=True,timeout=10)
print('Room temperature: CRC/value/range validation, optional configuration, sensor binding, stale rejection, real status JSON and nonblocking FIFO conversion passed.')
