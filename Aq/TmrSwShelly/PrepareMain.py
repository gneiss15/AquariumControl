# PrepareMain.py, Version: 1.03
"""Prepare source-only Arduino integration without touching unchanged inputs."""
from pathlib import Path
import argparse
import shutil
import sys
import tempfile
root=Path(__file__).resolve().parent
sys.path.insert(0,str(root.parent/'tools'))
from SyncGeneratedFiles import copy_file_if_changed, sync_tree, remove_generated_path, write_text_if_changed
import PrepareSimpleFtpServer as ftp
parser=argparse.ArgumentParser()
parser.add_argument('core',type=Path)
parser.add_argument('libraries',type=Path)
args=parser.parse_args()
out=root/'generated/build/main/MainDependencies'
component=out/'arduino';component.mkdir(parents=True,exist_ok=True)
for name in ['cores','variants','libraries']:
    target=args.core/name
    if not target.is_dir():raise ValueError('Missing Arduino source directory: '+name)
    link=component/name
    expected=target.resolve()
    if link.is_symlink():
        if link.resolve()==expected:continue
        remove_generated_path(link)
    elif link.exists():raise ValueError('Unexpected existing source path: '+str(link))
    link.symlink_to(expected,target_is_directory=True)
copy_file_if_changed(args.core/'Kconfig.projbuild',component/'Kconfig.projbuild')
# Render the final CMake variant before comparing it with its persistent output.
text=(args.core/'CMakeLists.txt').read_text()
needle='set(priv_requires fatfs nvs_flash app_update spiffs bootloader_support bt esp_hid esp_psram ${ARDUINO_LIBRARIES_REQUIRES})'
assert text.count(needle)==1
text=text.replace(needle,needle+'\nlist(APPEND requires espressif__mdns joltwallet__littlefs)')
write_text_if_changed(component/'CMakeLists.txt',text)
write_text_if_changed(component/'idf_component.yml','dependencies:\n  idf: ">=5.3,<6.0"\n  espressif/mdns: "1.11.2"\n  joltwallet/littlefs: "1.22.1"\n')
libs=out/'libraries';libs.mkdir(exist_ok=True)
for name in ['ArduinoJson','SimpleFTPServer']:
    source=args.libraries/name
    if not source.is_dir():raise ValueError('Missing external library: '+name)
    target=libs/name
    if name=='ArduinoJson':
        sync_tree(source,target)
        continue
    stage=Path(tempfile.mkdtemp(prefix='.ftp-stage-',dir=libs))
    try:
        shutil.copytree(source,stage,dirs_exist_ok=True)
        ftp.patch_esp32_storage(stage/'FtpServerKey.h')
        ftp.patch_timeout(stage/'FtpServerKey.h')
        sync_tree(stage,target)
    finally:
        remove_generated_path(stage)
print('Source Arduino component and LittleFS-only FTP prepared')
