# GenerateDocumentation.py, Version: 1.03
"""Render the shared definition model as reference documentation."""
import html,re
from pathlib import Path

def GenerateMarkdown(Types,Messages,Bits,Notice):
  def Cell(Value):
    return html.escape(str(Value)).replace('|','&#124;').replace('\n','<br>')

  Lines=['<!-- '+Notice+' -->','','# Aquarium JSON contracts','','## UDP messages','','All UDP messages are JSON objects with named fields.','',
         '| Message type | Schema |','| :--- | :--- |']
  for Tag,Name in Messages.items():
    Lines.append(f'| {Cell(Tag)} | [{Name}](#{Name.lower()}) |')
  Lines+=['','## Device IDs','',
          'A device ID identifies a service instance as `MAC@ServiceId`, for example `02:11:22:33:44:55@1`. '
          'The MAC identifies the hardware; ServiceId identifies the service on it. '
          'Channel IDs identify channels within that service. Some older status formats also accept logical names.',
          '','## UDP part','',
          '| Part | Meaning |','| :--- | :--- |',
          '| Live | Included in the regular compact Status data. |',
          '| Info | Descriptive data sent separately on request for the matching InfoRevision. |',
          '| Omit | Absent from both the regular compact Status and its Info replies. Access depends on the field: power meter diagnostics via HTTP, source mappings via SourceInfo. |']
  if 'TMsgType' in Types:
    Introduction=(Path(__file__).parent/'Templates/ContractIntroduction.md').read_text()
    Lines=['<!-- '+Notice+' -->','','# Aquarium JSON contracts','',Introduction,'## UDP messages','',
           '| Message type | Schema |','| :--- | :--- |']
    for Tag,Name in Messages.items():Lines.append(f'| {Cell(Tag)} | [{Name}](#{Name.lower()}) |')
  Lines+=['','## Compact channel bits','','Each logical bool uses two adjacent bits: `<Name>` carries the value and `<Name>Msk` marks it as supplied. A clear mask leaves the receiver’s previous value unchanged; a set mask applies the value, including `false`. Bit positions follow `SharedTypes.inc.h` definition order.','',
          '| Name | Value bit | Mask bit | Meaning |','| :--- | ---: | ---: | :--- |']
  for Name,Definition in Bits.items():
    Lines.append(f"| {Cell(Name)} | {Definition['Value'].bit_length()-1} | {Definition['Mask'].bit_length()-1} | {Cell(Definition['Description'])} |")
  Scalars={Name:Type for Name,Type in Types.items() if Type.get('Alias') or 'Enum' in Type or re.fullmatch(r'uint(8|16|32|64)_t',Type.get('ValueType',''))}
  if Scalars:
    Lines+=['','## Shared scalar types']
    for Name,Type in Scalars.items():
      Lines+=['','### '+Name,'',Cell(Type['Description'])]
      if Type.get('Alias'):Lines+=['','Encoded as `'+Cell(Type['Alias'])+'`.']
      if 'Enum' in Type:
        Lines+=['','| Name | Value | Description |','| :--- | :--- | :--- |']
        for Item in Type['Enum']:Lines.append('| '+Cell(Item['Name'])+' | '+str(Item['Value'])+' | '+Cell(Item['Description'])+' |')
  Lines+=['','## JSON types']
  for Name,Type in Types.items():
    if Name in Scalars:continue
    Lines+=['','### '+Name,'',Cell(Type['Description']),'']
    if Type.get('Shape')=='Tuple':Lines+=['JSON array with positional values.','']
    Lines+=[
            '| Field | Type | Presence | Rule or constant | UDP part | Description |',
            '| :--- | :--- | :--- | :--- | :--- | :--- |']
    if Type.get('ValueType'):
      Lines.append('| Value | '+Cell(Type['ValueType'])+' | Required | '+Cell(Type.get('Rule',''))+' | | |')
    for Field in Type['Fields']:
      Cells=[Field['Name'],Field['Type'],Field.get('Presence','Required'),
             Field.get('Value',Field.get('Rule','')),Field.get('Part',''),Field.get('Description','')]
      Cells=list(map(Cell,Cells))
      if Field['Type'] in Scalars:Cells[1]=f"[{Cells[1]}](#{Field['Type'].lower()})"
      Lines.append('| '+' | '.join(Cells)+' |')
  return '\n'.join(Lines)+'\n'



def GenerateHtml(Types,Notice):
  Rows=['<!-- '+html.escape(Notice)+' -->','<!doctype html><meta charset="utf-8"><title>Aquarium messages</title>']
  for Name,Type in Types.items():
    Rows+=['<h2>'+html.escape(Name)+'</h2><p>'+html.escape(Type['Description'])+'</p><table>']
    for Field in Type['Fields']:
      Cells=[Field['Name'],Field['Type'],Field.get('Presence','Required'),str(Field.get('Value',Field.get('Rule',''))),Field.get('Description','')]
      Rows+=['<tr>'+''.join('<td>'+html.escape(Cell)+'</td>' for Cell in Cells)+'</tr>']
    Rows+=['</table>']
  return '\n'.join(Rows)+'\n'
