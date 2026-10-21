# VerifyActivation.py, Version: 1.05
"""Check actual Main validation and common handlers without migration receipts."""
from pathlib import Path
import subprocess
import tempfile
Root=Path(__file__).resolve().parent
Updater=(Root.parent.parent.parent/'TmrSwShelly/generated/build/rescue/EspRescueUpdater.cpp').read_text()
assert 'ShellyRecordVerifiedUpdate' not in Updater
assert 'EspSignedArtifactVerifyPrefix' in Updater
Handlers=Updater[Updater.index('void TEspRescueUpdater::HandleCommit('):Updater.index('void TEspRescueUpdater::Loop(')]
Header=r'''#pragma once
#include <cstdint>
#include <cstring>
#include <cassert>
#define ESP_OK 0
#define ESP_PARTITION_TYPE_APP 0
#define ESP_PARTITION_SUBTYPE_APP_OTA_0 16
#define ESP_IMAGE_VERIFY 0
#define pdMS_TO_TICKS(x) (x)
#define SHUT_RDWR 2
struct esp_partition_t {uint32_t address,size;};
struct esp_partition_pos_t {uint32_t offset,size;};
struct esp_image_metadata_t {};
struct esp_app_desc_t {char project_name[32];};
static esp_partition_t Main{0x3A0000,0x2A0000};
static bool Present=true,DescriptionOk=true,ImageOk=true,Locked=false;
static const char * Project="ShellyTakeover";
static int BootSelections=0,Restarts=0,Status=0;
static const esp_partition_t * esp_partition_find_first(int,int,const char*){return Present?&Main:nullptr;}
static int esp_ota_get_partition_description(const esp_partition_t*,esp_app_desc_t*d){memset(d,0,sizeof(*d));strncpy(d->project_name,Project,31);return DescriptionOk?0:-1;}
static int esp_image_verify(int,const esp_partition_pos_t*,esp_image_metadata_t*){return ImageOk?0:-1;}
static bool ShellyRollbackBlockUpdates(void){return Locked;}
static void vTaskDelay(int){}
static int esp_ota_set_boot_partition(const esp_partition_t*){BootSelections++;return 0;}
static int shutdown(int,int){return 0;}
static int close(int){return 0;}
static void esp_restart(void){Restarts++;}
class TEspRescueUpdater {
 public:
 const esp_partition_t * FMainPartition=&Main;
 bool FUpdateCompleted=false;
 void HandleCommit(int);
 void HandleMain(int);
 void SendText(int,int Code,const char*){Status=Code;}
 void RestoreRescueBoot(void){}
};
'''
Tests=r'''int main(void){
 TEspRescueUpdater Updater;
 Updater.HandleMain(0);assert(Status==400&&BootSelections==0&&Restarts==0);
 Updater.HandleCommit(0);assert(Status==400&&BootSelections==0);
 Project="ShellyReadout";assert(ShellyMainActivationError());
 Project="TmrSwShelly";assert(!ShellyMainActivationError());
 Present=false;assert(ShellyMainActivationError());Present=true;
 Main.address++;assert(ShellyMainActivationError());Main.address--;
 Main.size--;assert(ShellyMainActivationError());Main.size++;
 DescriptionOk=false;assert(ShellyMainActivationError());DescriptionOk=true;
 ImageOk=false;assert(ShellyMainActivationError());ImageOk=true;
 Locked=true;Updater.HandleMain(0);assert(Status==400&&BootSelections==0);Locked=false;
 Updater.FUpdateCompleted=true;Updater.HandleCommit(0);
 assert(Status==200&&BootSelections==1&&Restarts==1);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as Temp:
 P=Path(Temp);(P/'common.h').write_text(Header)
 for Name in ['esp_ota_ops.h','esp_partition.h','esp_image_format.h']:
  (P/Name).write_text('#include "common.h"\n')
 (P/'Geometry.h').write_text((Root/'Runtime/Geometry.h').resolve().read_text())
 (P/'test.cpp').write_text('#include "common.h"\n#include "'+str(Root/'main/ActivationGate.cpp')+'"\n'+Handlers+Tests)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-I',str(P),str(P/'test.cpp'),'-o',str(P/'test')],check=True)
 subprocess.run([str(P/'test')],check=True)
print('Actual Main validation: empty/legacy/invalid images rejected, geometry checked, ordinary Main/Commit allowed without migration receipts (host mocks)')
