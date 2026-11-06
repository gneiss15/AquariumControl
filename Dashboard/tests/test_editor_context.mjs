// test_editor_context.mjs, Version: 1.00
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
const Source = await readFile(new URL('../Dashboard.js', import.meta.url), 'utf8');
const Slice = (Start,End) => Source.slice(Source.indexOf(Start),Source.indexOf(End,Source.indexOf(Start)));
const Context = vm.createContext({AssignmentView:null, Devices:[
  {TechnicalName:'LightTimerAq3',Configuration:{Group:'Aq3'}},
  {TechnicalName:'HeaterMainAq1',Configuration:{Group:'Aq1'}},
  {TechnicalName:'GN-Pi',Configuration:{GroupViews:[{Group:'Aq1',Channels:['HeizungBodenAq1']},{Group:'Aq2',Channels:['HeizungBodenAq2']}]}}
], Translations:{},Tr:(_Key,Fallback)=>Fallback,DeviceGroupDisplayName:Group=>Group});
vm.runInContext(Slice('function DeviceDisplayName(', '\nfunction RefreshTranslatedContent()')+
  Slice('function EndpointDisplayName(', '\nfunction AssignmentChoices('),Context);
assert.match(Context.EditorDeviceLabel('LightTimerAq3'),/Aq3/);
assert.match(Context.EditorDeviceLabel('HeaterMainAq1'),/Aq1/);
for (const Number of [1,2]) {
 const Label=Context.EditorChannelLabel({Id:`HeizungBodenAq${Number}`},'GN-Pi');
 assert.match(Label,new RegExp(`Aq${Number}`));assert.doesNotMatch(Label,new RegExp(`Aq${3-Number}`));
}
assert.notEqual(Context.EditorChannelLabel({Id:0,Name:'HeaterMainAq1'},'HeaterMainAq1'),'-');
console.log('Central editor labels preserve aquarium context for shared and single-channel devices.');
