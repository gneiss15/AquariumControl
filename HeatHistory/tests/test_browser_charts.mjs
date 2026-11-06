// test_browser_charts.mjs, Version: 1.03
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const nodes=new Map();
class Element {
  constructor(tag='div'){this.tag=tag;this.children=[];this.textContent='';this.events={};this.value='';this.dataset={};}
  append(...items){this.children.push(...items);for(const item of items)if(item.id)nodes.set(item.id,item);}
  replaceChildren(){this.children=[];}
  addEventListener(name,handler){this.events[name]=handler;}
}
for(const name of ['Range','Reload','LoadStatus','Charts','BrowserClock','chart-error'])nodes.set(name,new Element());
nodes.get('Range').value='1day';
const calls=[],traces=[];
const columns=['Time','TempC','TempValid','RoomTempC','RoomTempValid','FloorState','MainState','MainCorrectedW','MainPowerValid','FloorNominalW','MainIntervalQuality','FloorIntervalQuality','MainIntervalWh','FloorIntervalWh','MainIntervalSeconds','FloorIntervalSeconds'];
function bundle() {
 const boundary='test-boundary';const Files=[{Id:'0',Prof:'HeatProfileAq1',Kind:'Energy',Setpoint:28},{Id:'1',Prof:'HeatProfileAq1',Kind:'Energy',Setpoint:29}];
 const manifest={Ver:2,Files,Range:'1day',AvailableFrom:null,RetentionDays:30,From:'2026-10-28T12:00:00.000Z',To:'2026-10-28T12:02:00.000Z'};
 const row=(time,temp,floor,mi,fi)=>[time,temp,1,24,1,floor,0,0,1,41.052,'valid','valid',mi,fi,10,10].join(';');
 const parts=[columns.join(';')+'\r\n'+row('2026-10-28T12:00:00Z',27,1,0,0)+'\r\n'+row('2026-10-28T12:00:10Z',27.1,1,1,2)+'\r\n',columns.join(';')+'\r\n'+row('2026-10-28T12:01:00Z',27.2,0,1,0)+'\r\n'];
 let text='--'+boundary+'\r\nContent-Type: application/json\r\n\r\n'+JSON.stringify(manifest);
 for(let i=0;i<parts.length;i++)text+='\r\n--'+boundary+'\r\nContent-Type: text/csv\r\nContent-ID: '+i+'\r\n\r\n'+parts[i];
 return {text:text+'\r\n--'+boundary+'--\r\n',type:'multipart/mixed; boundary='+boundary};
}
const Plotly={newPlot:async(node,rows,layout)=>{traces.length=0;traces.push(...rows);assert.equal(layout.yaxis4.rangemode,'tozero');},addTraces:async(node,rows)=>traces.push(...rows),purge:()=>{},relayout:()=>{}};
const context=vm.createContext({TextEncoder,console,Date,JSON,Math,Object,Array,Plotly,setInterval:()=>{},window:{Plotly,matchMedia:()=>({matches:false,addEventListener:()=>{}})},
 document:{readyState:'complete',documentElement:{lang:'en'},getElementById:id=>nodes.get(id),querySelectorAll:()=>[],createElement:tag=>new Element(tag)},
 fetch:async url=>{calls.push(url);const data=bundle();return {ok:true,text:async()=>data.text,headers:{get:()=>data.type}};}});
vm.runInContext(fs.readFileSync(new URL('../Messages.js',import.meta.url),'utf8'),context);
vm.runInContext(fs.readFileSync(new URL('../HeatHistory.js',import.meta.url),'utf8'),context);
for(let i=0;i<20&&nodes.get('Reload').disabled;i++)await new Promise(resolve=>setImmediate(resolve));
assert.equal(calls[0],'/api/history.csv?range=1day');assert.equal(nodes.get('Reload').disabled,false);
assert(traces.some(trace=>trace.name==='Setpoint')); // Available with PI diagnostics disabled.
const setpoint=traces.find(trace=>trace.name==='Setpoint');assert.deepEqual(Array.from(setpoint.y),[28,28,null,29]);
const energy=traces.find(trace=>trace.name==='Main'&&trace.legendgroup==='Energy');assert.deepEqual(Array.from(energy.y),[0,1,null,2]);
nodes.get('Range').value='all';await nodes.get('Range').events.change();
assert.equal(calls.at(-1),'/api/history.csv?range=all');
console.log('Browser charts: 1day default, All request, stored targets without PI debug, measured energy intervals and explicit gaps passed');

const parsed=vm.runInContext(`parseCsvRows('Time;Reason\\r\\n"t";"quoted; value with ""quotes"" and\\nnewline"\\r\\n')`,context);
assert.equal(parsed[1][1],'quoted; value with "quotes" and\nnewline');
console.log('CSV parsing preserves semicolons, escaped quotes and embedded newlines');
