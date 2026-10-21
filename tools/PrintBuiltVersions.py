# PrintBuiltVersions.py, Version: 1.00
"""Print versions of the artifact types completed by the calling build."""
import argparse
from pathlib import Path
from UpdateVersionFiles import read_versions

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('project',type=Path)
parser.add_argument('--filesystem',action='store_true')
parser.add_argument('--devices',type=int,default=0)
args=parser.parse_args()
versions=read_versions(args.project/'Versions.txt')
print(f'Erzeugte Versionen ({args.project.name}):')
print('  Firmware: '+versions.get('Firmware',versions.get('App','unbekannt')))
if args.filesystem:
  key='LittleFS' if 'LittleFS' in versions else 'Web'
  suffix=f' ({args.devices} Geräte)' if args.devices else ''
  print('  '+('LittleFS' if key=='LittleFS' else 'LittleFS/Web')+': '+versions[key]+suffix)
