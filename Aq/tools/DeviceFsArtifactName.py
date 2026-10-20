# DeviceFsArtifactName.py, Version: 1.00
"""Filesystem upload names follow the verified Config hostname, not a reserve slot."""
import json,pathlib,re,sys

def artifact_name(config):
  name=json.loads(pathlib.Path(config).read_text()).get('Hostname')
  if not isinstance(name,str) or len(name)>63 or not re.fullmatch(r'[A-Za-z0-9](?:[A-Za-z0-9-]*[A-Za-z0-9])?',name):
    raise ValueError('Invalid Hostname for filesystem artifact')
  return name

def validate_directory(directory):
  names=set()
  for state in pathlib.Path(directory).iterdir():
    if not state.is_dir():continue
    name=artifact_name(state/'Config.json')
    if name in names:raise ValueError('Duplicate filesystem Hostname: '+name)
    names.add(name)
  if not names:raise ValueError('No active filesystem hostnames')

if __name__=='__main__':
  if sys.argv[1]=='--check-directory':validate_directory(sys.argv[2])
  else:print(artifact_name(sys.argv[1]))
