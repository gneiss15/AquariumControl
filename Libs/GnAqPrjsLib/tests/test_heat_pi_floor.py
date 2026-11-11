# test_heat_pi_floor.py, Version: 1.03
"""Run actual floor control against the production status receiver, without GPIO."""
import pathlib,subprocess,tempfile
lib=pathlib.Path(__file__).resolve().parents[1]
program=r'''
#include <GnBasics.h>
#include "Udp.cpp"
#include <GnHeaterControl.h>
#include <cassert>
int main(int argc,char **argv){
 THeaterRole role;
 assert(TGnHeaterControl::ParseRole("MainSw",role) && role==THeaterRole::HeaterMain);
 assert(std::string(TGnHeaterControl::RoleName(role))=="MainSw");
 assert(TGnHeaterControl::ParseRole("HeaterMain",role) && role==THeaterRole::HeaterMain);
 assert(!TGnHeaterControl::ParseRole("invalid",role));
 assert(argc==2);GnDeviceIdentity.Setup("Floor",1,argv[1]);
 THeaterProfile p;p.Id="Aq1";p.Pi.Enabled=p.Pi.Active=true;p.Pi.MainDevice="2C:BC:BB:C8:B6:88@4";
 p.Pi.FloorDevice=GnDeviceIdentity.WireId();p.Pi.FloorChannel="floor-one";
 TGnHeaterControl h({p});bool known=false,state=false;
 auto run=[&](bool scheduled=true,TGNString channel="floor-one"){
  return h.AutomaticState(THeaterRole::HeaterFloor,"Aq1",!scheduled,false,known,state,nullptr,false,true,channel);
 };
 assert(run()); // No main received: low-power floor fallback ON.
 assert(!run(false));assert(!run(true,"other-floor"));
 TGnWireDocument packet;auto r=packet.Root();r.UInt("Version",2);r.Text("Type","Status");r.Text("ID",p.Pi.MainDevice);
 auto s=r.NewObject("Status");s.Bool("TimeSynchronized",true);s.UInt("CurrentTime",time(nullptr));
 auto pi=s.NewObject("HeatPi");pi.UInt("Schema",1);pi.Text("Mode","Active");pi.Text("Profile",p.Id);pi.Bool("FloorRequest",false);
 auto c=pi.NewObject("Config");c.Text("FloorDevice",p.Pi.FloorDevice);c.Text("FloorChannel",p.Pi.FloorChannel);
 auto & receiver=*TGnUdpStatus::Instance().FImplementation;
 receiver.HandlePacket(r.Json(),"127.0.0.1",44444);assert(!run());
 pi.Bool("FloorRequest",true);receiver.HandlePacket(r.Json(),"127.0.0.1",44444);assert(run());
 r.Text("ID","2C:BC:BB:C8:B9:A4@4");pi.Bool("FloorRequest",false);
 receiver.HandlePacket(r.Json(),"127.0.0.1",44444);assert(run()); // Wrong main cannot replace demand.
 r.Text("ID",p.Pi.MainDevice);pi.Text("Profile","Aq2");receiver.HandlePacket(r.Json(),"127.0.0.1",44444);
 assert(run()); // Invalid profile binding invokes fallback, never another tank's request.
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'GnUdpStatus.h').write_text((lib/'GnUdpStatus.h').read_text().replace(' private:',' public:'))
 (p/'Udp.cpp').write_text((lib/'GnUdpStatus.cpp').read_text().replace(' private:',' public:'));(p/'test.cpp').write_text(program)
 cmd=['g++','-std=c++20','-Wall','-Wextra','-Werror','-DSYSTEM_DUMMY','-I'+tmp,'-I'+str(lib),'-I'+str(lib.parent/'GnBasicLib'),str(p/'test.cpp')]
 cmd += [str((lib.parent/'GnBasicLib' if n in ('GnBasics','GnTime','GnCoopTimer') else lib)/(n+'.cpp')) for n in ['GnBasics','GnTime','GnCoopTimer','GnDeviceIdentity','GnHeaterControl']]
 subprocess.run(cmd+['-ljson-c','-lpthread','-o',str(p/'test')],check=True);subprocess.run([str(p/'test'),str(p/'Identity.json')],check=True)
print('Actual PI floor controller: main requests, schedule gating, identity/profile/channel binding and fallback passed')
