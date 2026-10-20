# VerifyIdfBuild.py, Version: 1.00
"""Check the linked A8T image and SDK geometry before artifact signing."""
from pathlib import Path
import struct
import sys

root=Path(__file__).resolve().parent
sys.path.insert(0,str(root.parent/'tools'))
from UpdateVersionFiles import read_versions
build=Path(sys.argv[1])
image=(build/'TmrSwA8T.bin').read_bytes()
assert image[0]==0xE9 and struct.unpack_from('<H',image,12)[0]==0
assert image[2:4]==b'\x02\x20', 'Expected DIO, 4 MiB, 40 MHz image'
assert image[32:36]==b'\x32\x54\xcd\xab'
assert image[48:80].split(b'\0')[0]==read_versions(root/'Versions.txt')['Firmware'].encode()
assert image[80:112].split(b'\0')[0]==b'TmrSwA8T'
assert len(image)<=0x2D0000
config=(build/'sdkconfig').read_text()
for setting in ['CONFIG_IDF_TARGET="esp32"','CONFIG_PARTITION_TABLE_OFFSET=0x8000',
                'CONFIG_AUTOSTART_ARDUINO=y','CONFIG_ARDUINO_RUNNING_CORE=1',
                'CONFIG_ARDUINO_LOOP_STACK_SIZE=8192',
                'CONFIG_ESP_WIFI_NVS_ENABLED=y','CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE=y']:
    assert setting+'\n' in config, setting
for flag in ['CONFIG_SECURE_BOOT=y','CONFIG_SECURE_FLASH_ENC_ENABLED=y']:
    assert flag not in config, flag
expected={'nvs':(1,2,0x9000,0x5000),'otadata':(1,0,0xE000,0x2000),
          'safeboot':(0,0,0x10000,0xD0000),'app0':(0,16,0xE0000,0x2D0000),
          'spiffs':(1,130,0x3B0000,0x50000)}
found={}
table=(build/'partition_table/partition-table.bin').read_bytes()
for offset in range(0,len(table),32):
    if table[offset:offset+2]!=b'\xaa\x50':break
    _,kind,subtype,start,size,label,flags=struct.unpack_from('<HBBII16sI',table,offset)
    assert flags==0
    found[label.split(b'\0')[0].decode()]=(kind,subtype,start,size)
assert found==expected, 'SDK partition geometry differs from the installed A8T layout'
print('A8T ESP-IDF image, version, Arduino task and unchanged SDK geometry verified; no hardware test.')
