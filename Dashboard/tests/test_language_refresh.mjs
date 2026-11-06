// test_language_refresh.mjs, Version: 1.00
import fs from 'node:fs';import vm from 'node:vm';import assert from 'node:assert/strict';
const source=fs.readFileSync(new URL('../Dashboard.js',import.meta.url),'utf8');
const slice=(a,b)=>source.slice(source.indexOf(a),source.indexOf(b,source.indexOf(a)));
const caption={dataset:{tr:'Controller'},textContent:'Control'};
const input={value:'28.125'};let stamp='';
const context=vm.createContext({LangNr:0,Devices:[],DeviceGroups:new Map(),CentralTimeStore:{Times:[],Sets:[]},CentralHeatProfileStatus:{Sets:[]},LastDashboardUpdateAt:new Date('2026-10-29T06:00:00Z'),
 Tr:key=>key==='Controller'?(context.LangNr?'Regelung':'Control'):key==='Updated'?(context.LangNr?'Aktualisiert':'Updated'):'No data',
 document:{documentElement:{},querySelectorAll:s=>s==='[data-tr]'?[caption]:[],querySelector:()=>input,getElementById:id=>id==='DetailOverlay'?{hidden:true}:id==='Assignments'?{classList:{contains:()=>false}}:{}},
 RenderChannelProfiles:()=>{},RenderHeatProfileStatus:()=>{},SetDashboardStatus:s=>stamp=s,FormatShortTime:date=>date.toISOString().slice(11,16)});
vm.runInContext(slice('function TrAll()', '\nfunction DeviceDisplayName(')+slice('function RefreshTranslatedContent()', '\nfunction LSelect(')+slice('function RenderDashboardTimestamp()', '\nfunction CheckOfflineDevices('),context);
context.LangNr=1;context.RefreshTranslatedContent();
assert.equal(caption.textContent,'Regelung');assert.equal(input.value,'28.125');assert.equal(stamp,'Aktualisiert 06:00');
assert.equal(context.LastDashboardUpdateAt.toISOString(),'2026-10-29T06:00:00.000Z');
context.LangNr=0;context.RefreshTranslatedContent();assert.equal(caption.textContent,'Control');assert.equal(stamp,'Updated 06:00');
console.log('Language refresh preserves unsaved input and actual last-update time while translating dynamic captions');
