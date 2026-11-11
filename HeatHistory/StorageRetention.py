# StorageRetention.py, Version: 1.03
"""Expire recording segments by age, then enforce the storage limit."""
import datetime as dt
import subprocess
from pathlib import Path

class StorageRetention:
  def __init__(self,root,config):
    self.root=Path(root)
    self.days=config.get('StorageRetentionDays',30)
    self.maximum=config.get('StorageMaxBytes',512*1024*1024)
    self.enabled=config.get('StorageDeleteExpired',False)
    if type(self.days) is not int or not 1<=self.days<=366:raise ValueError('StorageRetentionDays must be 1..366')
    if type(self.maximum) is not int or self.maximum<1024*1024:raise ValueError('StorageMaxBytes must be at least 1 MiB')
    if type(self.enabled) is not bool:raise ValueError('StorageDeleteExpired must be boolean')

  def size(self):
    # GNU du -b reports apparent bytes, including directory storage.
    try:
      return int(subprocess.check_output(['du','-sb','--',str(self.root)],text=True).split()[0])
    except (subprocess.CalledProcessError,ValueError,IndexError) as error:
      raise OSError('Cannot measure HeatHistory storage with du') from error

  def maintain(self,active=(),now=None):
    # Preconditions owned by HeatHistory/the installation:
    # - root contains only recording files, never foreign files or symlinks;
    # - daily rotation closes old segments (on the Pi, the daily system reboot);
    # - a CSV and its JSON metadata belong together; state.json is resume state.
    # Procedure: delete expired closed segments, measure the remainder with du,
    # then delete oldest closed segments until within the byte limit, subtracting
    # their file sizes from that single measurement.
    # Automatic cleanup runs only when StorageDeleteExpired is True in the
    # configuration. Active segments and resume state are retained even if
    # rotation fails; report an unresolvable limit.
    if not self.enabled:return
    now=now or dt.datetime.now(dt.timezone.utc)
    cutoff=(now-dt.timedelta(days=self.days)).timestamp()
    protected={Path(path).with_suffix('') for path in active}
    groups={}
    for path in self.root.rglob('*'):
      if path.is_file() and path.name!='state.json':
        groups.setdefault(path.with_suffix(''),[]).append(path)
    closed=sorted((paths for key,paths in groups.items() if key not in protected),
                  key=lambda paths:max(path.stat().st_mtime for path in paths))
    def remove(paths):
      for path in paths:
        path.unlink()
    expired=0
    for paths in closed:
      if max(path.stat().st_mtime for path in paths)>=cutoff:break
      remove(paths)
      expired+=1
    del closed[:expired]
    total=self.size()
    for paths in closed:
      if total<=self.maximum:break
      size=sum(path.stat().st_size for path in paths)
      remove(paths)
      total-=size
    if total>self.maximum:
      raise OSError('HeatHistory storage limit reached by active logs; recording stopped')
