// test_status_push.mjs, Version: 1.00
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const Code = await readFile(new URL('../StatusClient.js', import.meta.url), 'utf8');
const {TDeviceStatusClient} = await import(`data:text/javascript;base64,${Buffer.from(Code).toString('base64')}`);
globalThis.window = {location: {href: 'http://dashboard/'}};
const Timers = new Map(); let Timer = 0;
globalThis.setInterval = (Function, Period) => {Timers.set(++Timer, {Function, Period}); return Timer;};
globalThis.clearInterval = Id => Timers.delete(Id);
const Streams = [];
globalThis.EventSource = class {
  constructor(Url) {this.Url=String(Url); Streams.push(this);}
  close() {this.Closed=true;}
};
let Requests=0, ResolvePoll;
globalThis.fetch = async () => {++Requests; return {ok:true,json:async()=>({Online:false,Revision:0,Status:null})};};
const Seen=[], Connections=[];
const Battery=new TDeviceStatusClient('/',Data=>Seen.push(Data),Value=>Connections.push(Value),'MAC@3');
const Other=new TDeviceStatusClient('/',()=>assert.fail('wrong subscriber'),'OTHER@4');
await Battery.Start(); await Other.Start();
assert.equal(Streams.length,1,'one stream must serve all cards');
assert.equal(Streams[0].Url,'http://dashboard/api/events');
const Send=Batch=>Streams[0].onmessage({data:JSON.stringify(Batch)});
Send([{ID:'MAC@3',Online:true,Revision:1,Status:{BatteryPercent:19}}]);
assert.deepEqual(Seen,[{BatteryPercent:19}]);
assert.equal([...Timers.values()].filter(T=>T.Period===1000).length,0,'healthy push must stop polling');
const Before=Requests; await Battery.PollStatus(); assert.equal(Requests,Before);
Send([{ID:'MAC@3',Online:false,Revision:1,Status:{BatteryPercent:19}}]);
assert.deepEqual(Connections,[true,false]); assert.equal(Seen.length,1);
Send([{ID:'MAC@3',Online:true,Revision:1,Status:{BatteryPercent:19}}]);
assert.equal(Seen.length,2,'recovery must republish same revision');
Streams[0].onerror(); assert.equal([...Timers.values()].filter(T=>T.Period===1000).length,2);
globalThis.fetch = () => new Promise(Resolve => {ResolvePoll=Resolve;});
const Pending=Battery.PollStatus();
Send([{ID:'MAC@3',Online:true,Revision:2,Status:{BatteryPercent:20}}]);
ResolvePoll({ok:true,json:async()=>({Online:true,Revision:1,Status:{BatteryPercent:18}})});
await Pending; assert.equal(Seen.at(-1).BatteryPercent,20,'late fallback must not overwrite pushed status');
// A restarted server may reuse the same revision for changed content.
Streams[0].onerror();
Send([{ID:'MAC@3',Online:true,Revision:2,Status:{BatteryPercent:21}}]);
assert.equal(Seen.at(-1).BatteryPercent,21);
const RealNow=Date.now; Date.now=()=>RealNow()+16000;
[...Timers.values()].find(T=>T.Period===5000).Function();
Date.now=RealNow;
assert(Streams[0].Closed);assert.equal(Streams.length,2,'silent stream must reconnect');
assert.equal([...Timers.values()].filter(T=>T.Period===1000).length,2);
Send([{ID:'MAC@3',Online:true,Revision:99,Status:{BatteryPercent:99}}]);
assert.equal(Seen.at(-1).BatteryPercent,21,'closed streams must not deliver old queued events');
Battery.Stop();assert(!Streams[1].Closed);Other.Stop();assert(Streams[1].Closed);assert.equal(Timers.size,0);
console.log('Push client: shared stream, routing, no polling while healthy, offline/recovery, fallback, late response and cleanup passed.');
