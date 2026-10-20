# VerifyBuild.py, Version: 1.06
import pathlib
import json
Root=pathlib.Path(__file__).resolve().parent
Prjs=Root.parent.parent.parent
Out=Prjs/'TmrSwShelly/generated/build/rescue'
Versions={Key:Value for Key,Value in (Line.strip().split('=',1) for Line in (Prjs/'TmrSwShelly/Versions.txt').read_text().splitlines() if Line.strip() and not Line.lstrip().startswith('#'))}
RescueVersion=Versions['Rescue']
Image=(Out/'Rescue.bin').read_bytes()
assert Image[0]==0xE9 and Image[12:14]==b'\x05\x00' and len(Image)<=0x180000
assert Image[2]==2 and Image[3]==0x3F, 'Image must be DIO/80-MHz/8-MiB'
assert Image[48:80].split(b'\0')[0]==RescueVersion.encode()
assert Image[80:112].split(b'\0')[0]==b'ShellyRescue'
Config=(Out/'esp32/idf/sdkconfig').read_text()
for Setting in ['CONFIG_PARTITION_TABLE_OFFSET=0x10000','CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y','CONFIG_ESPTOOLPY_FLASHMODE_DIO=y','CONFIG_ESPTOOLPY_FLASHFREQ_80M=y']:
 assert Setting in Config,Setting
for Setting in ['CONFIG_ESP_WIFI_NVS_ENABLED=y','CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE=y','CONFIG_SECURE_BOOT=y','CONFIG_SECURE_FLASH_ENC_ENABLED=y']:
 assert Setting not in Config,Setting
Table=(Out/'esp32/idf/partition_table/partition-table.bin').read_bytes()
Expected=(Out/'NewTable.bin').read_bytes()
assert Table.ljust(4096,b"\xff")==Expected,'Rescue and installed dual-loader target table differ'
Result={'Application':'ShellyRescue','Version':RescueVersion,'Size':len(Image),'FactorySlotSize':0x180000,'TableOffset':0x10000,'ExactDualLoaderTargetTable':True,'DeviceType':(Root/'DeviceType.txt').read_text().strip(),'TargetMainProject':'TmrSwShelly','HardwareTested':False}
(Out/'build-verification.json').write_text(json.dumps(Result,indent=2)+'\n');print(json.dumps(Result,indent=2))
