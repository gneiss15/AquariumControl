// test_charge_profile_editor.mjs, Version: 1.03
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
const schema = await readFile(new URL('../Messages.js', import.meta.url), 'utf8');
const source = await readFile(new URL('../Dashboard.js', import.meta.url), 'utf8');
const code = source.slice(source.indexOf('function CollectHeatProfiles()'), source.indexOf('\nfunction ActivateTab(', source.indexOf('function CollectHeatProfiles()')));
const base = {ProfName:'LaptopCharge', MeasSrc:'02:11:22:33:44:55@3', Meas:'BatteryPercent', ChargeStartPct:40, ChargeStopPct:80, OnIfNoMeas:false, Future:{nested:[1,true,'keep']}};
function collect(profile, active, factor) {
 const inputs = Object.entries({...profile, ChargeRunOn:active, ChargeSecPerPct:factor}).filter(([key])=>key!=='Future').map(([key,value])=>({dataset:{key}, type: typeof value==='boolean'?'checkbox':(['ChargeStartPct','ChargeStopPct','ChargeSecPerPct'].includes(key)?'number':'text'), checked:value===true, value:value===undefined?'':String(value), disabled:key==='ChargeSecPerPct'&&!active}));
 const context = vm.createContext({TextEncoder,CentralHeatProfiles:{Profs:[profile]}, ProfileReferences:()=>[], Tr:(_key,text)=>text, document:{querySelectorAll:()=>[{querySelectorAll:selector=>selector==='[data-key]'?inputs:[]}]}});
 vm.runInContext(schema,context);
 vm.runInContext(code,context);
 return JSON.parse(JSON.stringify(context.CollectHeatProfiles()))[0];
}
const active=collect(base,true,15);
assert.equal(active.ChargeSecPerPct,15);assert.equal(active.ChargeRunOn,true);assert.deepEqual(active.Future,base.Future);
const inactive=collect(base,false,undefined);
assert.ok(!('ChargeSecPerPct' in inactive));assert.equal(inactive.ChargeRunOn,false);assert.deepEqual(inactive.Future,base.Future);
assert.throws(()=>collect(base,true,undefined));assert.throws(()=>collect(base,true,0));
assert.ok(!('ChargeRunOn' in collect({...base,Meas:'Temperature'},true,15)));
assert.throws(()=>collect(base,true,15.5));assert.throws(()=>collect(base,true,-1));assert.throws(()=>collect(base,true,4294967296));
console.log('Profile editor: optional inactive fields, required active factor, battery-only rule and unknown-field preservation passed.');
