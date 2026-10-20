# test_json_editor_format.py, Version: 1.02
"""Check actual JSON editor formatters preserve records and unknown data."""
import pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[2]
for name in ['Libs/Common/TmrSw/WebSrc/Index.js','MultiTimer/WebSrc/Index.js']:
 source=(root/name).read_text();start=source.index('function FormatRecordArray(')
 end=source.find('function FormatChannelsJson(',start)
 if end<0:end=source.find('let ConfigurationRevision',start)
 formatter=source[start:end if end>=0 else len(source)]
 checks=r'''
const samples=[{Profiles:[{Id:'A',On:2},{Id:'Long',On:3}],Channels:[{Id:'Main',Role:'Heater',Extra:true},{Id:'Light',Role:'Timer'}],Unknown:{Keep:[1,2]}},{FeederAq1:'AA@3',FeederAq2:'BB@3'},[],[1,2],{Channels:[],Profiles:[]},[{A:1},{B:2}],{Escaped:'quote" and\n newline',Flag:false,Empty:null}];
for(const data of samples){
 const text=FormatEditorJson(data);
 if(JSON.stringify(JSON.parse(text))!==JSON.stringify(data))throw Error('Data lost');
 if(data.Profiles?.length){
  const rows=text.split('\n').filter(line=>line.includes('"On"'));
  if(rows.length!==2||rows[0].indexOf('"On"')!==rows[1].indexOf('"On"'))throw Error('Not aligned');
 }
}
'''
 subprocess.run(['node','-e',formatter+checks],check=True)
print('Passed: both actual editor formatters, one aligned row per record, unknown fields, empty/mixed arrays, escaped strings and unchanged JSON values.')
