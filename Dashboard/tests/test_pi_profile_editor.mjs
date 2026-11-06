// test_pi_profile_editor.mjs, Version: 1.02
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
const schema=readFileSync(new URL('../Messages.js',import.meta.url),'utf8');
const source=readFileSync(new URL('../Dashboard.js',import.meta.url),'utf8');
const code=source.slice(source.indexOf('function CollectHeatProfiles()'),source.indexOf('\nfunction ActivateTab(',source.indexOf('function CollectHeatProfiles()')));
const profile={ProfName:'Heat',MeasSrc:'02:11:22:33:44:57@3',Meas:'Temperature',OnIfNoMeas:false,Heat:{Active:true,Setpoint:28,Kp:270,MainEndpoint:'02:11:22:33:44:55@4/0',FloorEndpoint:'02:11:22:33:44:56@1/1',FloorW:41.052,MainMaxW:350,CapacityWhPerK:590,LossWPerK:27,TiSec:31200,WindowSec:600,DelaySec:600,Future:{keep:true}}};
function collect(active,setpoint){
 const inputs=[{dataset:{piKey:'Active'},checked:active,type:'checkbox'},{dataset:{piKey:'Setpoint'},value:setpoint,type:'number'}];
 const card={querySelector:()=>inputs[0],querySelectorAll:s=>s==='[data-pi-key]'?inputs:[]};
 const context=vm.createContext({TextEncoder,CentralHeatProfiles:{Profs:[profile]},ProfileReferences:()=>[],Tr:(_k,v)=>v,document:{querySelectorAll:()=>[card]}});
 vm.runInContext(schema,context);
 vm.runInContext(code,context);return JSON.parse(JSON.stringify(context.CollectHeatProfiles()))[0];
}
const changed=collect(true,'28.1');assert.equal(changed.Heat.Setpoint,28.1);assert.deepEqual(changed.Heat.Future,profile.Heat.Future);assert.equal(profile.Heat.Setpoint,28);
const off=collect(false,'28');assert.equal(off.Heat.Active,false);assert.equal(off.Heat.Setpoint,28);
assert.throws(()=>collect(true,''));
console.log('PI editor: nested settings, unknown-field preservation, immutable draft and Off-mode preservation passed');
