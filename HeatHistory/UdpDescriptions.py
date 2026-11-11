# UdpDescriptions.py, Version: 1.15
"""Decode compact status; descriptor traffic never counts as a fresh sample."""
import copy
import json
from Messages import ValidateMessage, ValidateType, TYPES, BITS, NewInfoRequest, TMsgType


class UdpDescriptions:
  def __init__(self):
    self.entries = {}
    self.pending = {}

  def OnUdpPacket(self, packet, now, wall_time, identities=None):
    """Validate once; return (timestamped status, description request).

    Info replies may release a held status, but never update its receive times.
    identities limits reception to the current Dashboard assignments.
    """
    if isinstance(packet, (bytes, str)):
      try:
        packet = json.loads(packet)
      except (ValueError, UnicodeDecodeError):
        return None, None
    if not ValidateMessage(packet):
      return None, None
    identity = packet.get('ID')
    if not isinstance(identity,str):
      return None, None
    if identities is not None and identity not in identities:
      return None, None
    if packet.get('Type') == TMsgType.Info:
      if not all(isinstance(packet.get(key),str) for key in ('Field','Data','InfoRev')):
        return None, None
      if not self.OnInfoPacket(packet) or identity not in self.pending:
        return None, None
      pending, received, wall = self.pending[identity]
      expanded = self.ExpandStatus(pending, self.entries[identity])
      if expanded is None:
        return None, None
      del self.pending[identity]
      return (expanded, received, wall), None
    if packet.get('Type') == TMsgType.Status:
      status=packet.get('Status')
      if not isinstance(status,dict):
        return None, None
      for key,field in (('Chs','Id'),('Meas','Kind')):
        rows=status.get(key)
        if rows is None:
          rows=[]
          status[key]=rows
        if not isinstance(rows,list) or any(not isinstance(row,dict) or not isinstance(row.get(field),str) for row in rows):
          return None, None
      expanded, request = self.OnStatusPacket(packet, now, wall_time)
      if expanded is not None:
        self.pending.pop(identity, None)
        return (expanded, now, wall_time), request
      return None, request
    return None, None

  def OnInfoPacket(self, packet):
    entry = self.entries.get(packet.get('ID'))
    field, data = packet['Field'], packet['Data']
    if not entry or packet['InfoRev'] != entry['revision'] or field not in entry['required']:
      return False
    try:
      value = json.loads(data)
    except (ValueError, TypeError):
      if not data.startswith('{{') or not data.endswith('}}'):
        return False
      try:
        value = json.loads(data[1:-1])
      except (ValueError, TypeError):
        return False
    if not isinstance(value, dict):
      return False
    schema = 'TUdpStatusData' if field == 'Root' else 'TJsonIdentity' if field == 'Identity' else 'TUdpChannel' if field.startswith('Channel:') else 'TUdpMeasurement'
    if not ValidateType(schema, value, Partial=True):
      return False
    if field == 'Root' and set(value) - {row['Name'] for row in TYPES[schema]['Fields'] if row.get('Part') == 'Info'}:
      return False
    entry['parts'][field] = value
    return True

  def OnStatusPacket(self, packet, now, wall_time):
    if packet.get('Compact') is not True:
      return packet, None
    identity, revision = packet.get('ID'), packet.get('InfoRev')
    if identity not in self.entries and len(self.entries) >= 32:
      return None, None
    entry = self.entries.get(identity)
    if not entry or entry['revision'] != revision:
      self.pending.pop(identity, None)
      entry = self.entries[identity] = dict(revision=revision, parts={}, required=set(), last=0, attempts=0, channel_bits={})
    status = packet['Status']
    channels, measurements = status.get('Chs') or [], status.get('Meas') or []
    status['Chs'], status['Meas'] = channels, measurements
    channel_key = lambda row: 'Channel:' + row['Id']
    required = {'Root', 'Identity'}
    required.update(channel_key(row) for row in channels)
    required.update('Measurement:' + row['Kind'] for row in measurements)
    if len(required) > 64:
      return None, None
    entry['required'] = required
    missing = sorted(required - entry['parts'].keys())
    request = None
    if missing and (not entry['attempts'] or now-entry['last'] >= (3 if entry['attempts'] < 3 else 30)):
      request = NewInfoRequest(ID=identity, InfoRev=revision, Fields=missing)
      entry['last'], entry['attempts'] = now, entry['attempts']+1
    if missing:
      self.pending[identity] = (packet, now, wall_time)
      return None, request
    return self.ExpandStatus(packet, entry), request

  def ExpandStatus(self, packet, entry):
    """Expand a validated status only when all its descriptions are complete."""
    if entry['required'] - entry['parts'].keys():
      return None
    identity = packet['ID']
    channel_key = lambda row: 'Channel:' + row['Id']
    result = copy.deepcopy(packet)
    status = result['Status']
    status.update(entry['parts']['Root'])
    status['Identity'] = dict(entry['parts']['Identity'])
    for row in status.get('Chs') or []:
      channel_id = row.get('Id')
      incoming = row.pop('Bits', 0)
      previous = entry['channel_bits'].get(channel_id, 0)
      replace = 0
      for definition in BITS.values():
        if incoming & definition['Mask']:
          replace |= definition['Mask'] | definition['Value']
      bits = entry['channel_bits'][channel_id] = (previous & ~replace) | (incoming & replace)
      row.update(entry['parts'][channel_key(row)])
      for name, definition in BITS.items():
        row[name] = bool(bits & definition['Value']) if bits & definition['Mask'] else None
    for row in status.get('Meas') or []:
      row.update(entry['parts']['Measurement:' + str(row.get('Kind', ''))])
    result.pop('Compact', None)
    result.pop('InfoRev', None)
    return result
