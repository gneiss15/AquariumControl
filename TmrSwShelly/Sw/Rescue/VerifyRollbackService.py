# VerifyRollbackService.py, Version: 1.02
"""Compile the actual socket service and exercise authorization/restart locks."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'freertos').mkdir();(p/'lwip').mkdir()
 (p/'esp_random.h').write_text('#include <stddef.h>\nvoid esp_fill_random(void*,size_t);\n')
 (p/'esp_system.h').write_text('void esp_restart(void);\n')
 (p/'freertos/FreeRTOS.h').write_text('#define pdMS_TO_TICKS(x) (x)\n')
 (p/'freertos/task.h').write_text('static inline void vTaskDelay(unsigned n){(void)n;}\n')
 (p/'lwip/sockets.h').write_text('#include <sys/socket.h>\n#include <unistd.h>\n')
 header=r'''
#include <cassert>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include "Migration.h"
static bool Ready=true,RestartReady=true,Present=true,Uncertain;
static unsigned Writes,Restarts;
extern const uint8_t NewTable[4096] asm("_binary_NewTable_bin_start") = {};
void esp_fill_random(void*p,size_t n){memset(p,0x42,n);}
void esp_restart(void){Restarts++;}
bool MigrationUncertain(void){return Uncertain;}
bool MigrationCheckpointPresent(void){return Present;}
const char *MigrationError(void){return "blocked";}
const char *MigrationStep(void){return "verified";}
bool MigrationRollbackReady(const uint8_t*){return Ready;}
bool MigrationRollbackRestartReady(const uint8_t*){return RestartReady;}
bool MigrationRollback(const uint8_t*){Writes++;return Ready;}
'''
 tests=r'''
static std::string request(const char*path,bool post,const char*body,unsigned len){int s[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,s)==0);if(body)assert(send(s[1],body,len,0)==int(len));assert(ShellyRollbackHandle(s[0],path,post,len));shutdown(s[0],SHUT_WR);char b[4096];std::string response;int n;while((n=recv(s[1],b,sizeof(b),0))>0)response.append(b,n);close(s[0]);close(s[1]);return response;}
int main(void){
 ShellyRollbackConfigure(false);assert(ShellyRollbackBlockUpdates());
 for(const char * Path:{"/Firmware","/Filesystem","/Main","/Commit"}) assert(request(Path,true,"bad",3).find("400")!=std::string::npos);
 assert(request("/rollback-info",false,nullptr,0).find("\"MigrationComplete\":false")!=std::string::npos);
 ShellyRollbackConfigure(true);assert(!ShellyRollbackBlockUpdates());
 for(const char * Path:{"/Firmware","/Filesystem","/Main","/Commit"}) assert(!ShellyRollbackHandle(-1,Path,true,0));
 Present=true;Ready=false;assert(!ShellyRollbackBlockUpdates());
 assert(request("/rollback-info",false,nullptr,0).find("\"MigrationComplete\":true")!=std::string::npos);
 assert(request("/rollback-info",false,nullptr,0).find("\"DestructiveUpdatesLocked\":false")!=std::string::npos);
 for(const char * Path:{"/rollback-write","/rollback-restart"}) {
  assert(request(Path,true,Token,32).find("400")!=std::string::npos);
  assert(request(Path,true,"bad",3).find("400")!=std::string::npos);
  assert(request(Path,false,nullptr,0).find("400")!=std::string::npos);
 }
 assert(Writes==0&&Restarts==0);
 Uncertain=true;assert(ShellyRollbackBlockUpdates());
 for(const char * Path:{"/Firmware","/Filesystem","/Main","/Commit"}) assert(request(Path,true,"bad",3).find("400")!=std::string::npos);
 assert(request("/rollback-info",false,nullptr,0).find("\"DoNotRestart\":true")!=std::string::npos);
 Uncertain=false;Present=false;assert(!ShellyRollbackBlockUpdates());
 assert(request("/rollback",false,nullptr,0).find("Rescue: Migration abgeschlossen")!=std::string::npos);
 assert(Writes==0&&Restarts==0);
 return 0;}

'''
 (p/'test.cpp').write_text(header+'\n#include "'+str(root/'main/RollbackService.cpp')+'"\n'+tests)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(p),'-I',str(root/'Runtime'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Actual production Rescue socket service: completion ignores legacy checkpoints, rollback rejected, geometry/uncertainty locks and diagnostics passed')
