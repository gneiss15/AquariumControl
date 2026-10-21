# VerifyMain.py, Version: 1.09
"""Verify the linked main image, SDK geometry and per-device filesystem artifacts."""
from pathlib import Path
import json
import struct
import sys
import os
root=Path(__file__).resolve().parent
versions={key:value for key,value in (line.strip().split('=',1) for line in (root/'Versions.txt').read_text().splitlines() if line.strip() and not line.lstrip().startswith('#'))}
firmware_version=versions['Firmware']
firmware_only='--firmware-only' in sys.argv
build=root/'generated/build/main/esp32/idf';device=root/'generated/Instances'/sys.argv[1]
binaries=root/'generated/bin/esp32c3_Arduino2'
image=(build/'TmrSwShelly.bin').read_bytes()
assert image[0]==0xE9 and image[12:14]==b'\x05\x00' and image[2:4]==b'\x02\x3f'
assert image[32:36]==b'\x32\x54\xcd\xab'
assert image[48:80].split(b'\0')[0]==firmware_version.encode() and image[80:112].split(b'\0')[0]==b'TmrSwShelly'
assert len(image)<=0x2A0000
config=(build/'sdkconfig').read_text()
assert 'CONFIG_PARTITION_TABLE_OFFSET=0x10000' in config
for flag in ['CONFIG_ESP_WIFI_NVS_ENABLED=y','CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE=y','CONFIG_SECURE_BOOT=y','CONFIG_SECURE_FLASH_ENC_ENABLED=y']:
    assert flag not in config
expected={'app0':(0x3A0000,0x2A0000),'safeboot':(0x640000,0x180000),'spiffs':(0x2C0000,0xE0000),'nvs':(0x7C0000,0x20000),'otadata':(0x7E0000,0x2000)}
table=(build/'partition_table/partition-table.bin').read_bytes()
found={}
for offset in range(0,len(table),32):
    if table[offset:offset+2]!=b'\xaa\x50':break
    _,kind,subtype,start,size,label,flags=struct.unpack_from('<HBBII16sI',table,offset)
    found[label.split(b'\0')[0].decode()]=(start,size)
for name,geometry in expected.items():assert found[name]==geometry
if not firmware_only:
    assert len((device/'LittleFS.NoId.bin').read_bytes())==0xE0000
    for p in (device/'LittleFS').glob('*.json'):json.loads(p.read_text())
sys.path.insert(0,str(root.parent/'tools'))
import DeviceTypeArtifacts as Artifacts
from DeviceFsArtifactName import artifact_name
public=Path(os.environ.get('PRJS_PRIVATE_DIR',str(root.parent.parent.parent/'PrjsPrivate')))/'public.key'
expected_device_type=(root/'generated/build/main/sketch/TmrSwShelly/DeviceType.txt').read_text().strip()
assert expected_device_type==(os.environ.get('TMR_SW_DEVICE_TYPE') or Artifacts.ReadDeviceType(Path(os.environ.get('TMR_SW_DEVICE_TYPE_FILE',str(root/'DeviceType.txt')))))
artifacts=[('firmware','TmrSwShelly.ino.bin_signed',image)]
if not firmware_only:artifacts.append(('filesystem',artifact_name(device/'LittleFS/Config.json')+'.ino.littlefs_signed',(device/'LittleFS.NoId.bin').read_bytes()))
for kind,name,payload in artifacts:
    path=binaries/name
    Artifacts.VerifySignedArtifact(path,public)
    blob=path.read_bytes()
    artifact_type,size,device_type=Artifacts.ReadArtifactHeader(blob)
    assert artifact_type==Artifacts.ARTIFACT_TYPES[kind] and size==len(payload) and device_type==expected_device_type
    signature_size=struct.unpack_from('<8sHBBI32s',blob,Artifacts.HEADER_SIZE)[4]
    offset=Artifacts.HEADER_SIZE+Artifacts.SIGNATURE_HEADER_SIZE+signature_size
    assert blob[offset:]==payload, 'Signed artifact differs from verified raw payload'
result={'Application':'TmrSwShelly','FirmwareVersion':firmware_version,'DeviceType':expected_device_type,'FirmwareSize':len(image),'SdkTableOffset':0x10000,'TargetPartitionVerified':True,'FilesystemSize':None if firmware_only else 0xE0000,'SignedFirmwareVerified':True,'SignedFirmwareAndFilesystemVerified':not firmware_only,'HardwareTested':False,'RelayPin':0,'RelayActiveLevel':1,'KeyPin':7,'LedHardwareEnabled':True,'LedColor':'BlueWaitingGreenRelay','LedPin':10,'PowerMeterEnabled':True,'PowerMeterChip':'BL0942','PowerMeterCalibration':'Device factory record; direct overwrite with confirmation','HardwareMeasurementsVerified':False,'Uploaded':False}
(device/'main-build-verification.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
