// test_external_status_client.mjs, Version: 1.00
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const Code = await readFile(new URL('../StatusClient.js', import.meta.url), 'utf8');
const {TDeviceStatusClient} = await import(`data:text/javascript;base64,${Buffer.from(Code).toString('base64')}`);
globalThis.window = {location: {href: 'http://dashboard/'}};
const RealInterval = globalThis.setInterval;
globalThis.setInterval = () => null;
let Envelope = {Online: false, Revision: 0, Status: null};
const Requests = [], Statuses = [], Connections = [];
globalThis.fetch = async Url => {
  Requests.push(String(Url));
  return {ok: true, json: async () => Envelope};
};
const Client = new TDeviceStatusClient('/', Status => Statuses.push(Status),
  Connected => Connections.push(Connected), 'F4:06:69:2A:E9:CD@3');
try {
  await Client.Start();
  assert.equal(Statuses.length, 0);
  Envelope = {Online: true, Revision: 1, Status: {BatteryPercent: 67}};
  await Client.PollStatus();
  assert.deepEqual(Statuses, [{BatteryPercent: 67}]);
  Envelope = {...Envelope, Online: false};
  await Client.PollStatus();
  assert.equal(Statuses.length, 1, 'stale battery must not be republished as fresh');
  Envelope = {...Envelope, Online: true};
  await Client.PollStatus();
  assert.equal(Statuses.length, 2, 'recovery restores a cleared measurement');
  assert.deepEqual(Connections, [true, false, true]);
  assert(Requests.every(Url => Url.includes('/api/device-status/')),
    'external sender must not require a device/config API');
  console.log('External cache-only client: startup, updates, stale suppression and recovery passed.');
} finally {
  Client.Stop();
  globalThis.setInterval = RealInterval;
}

// Exercise the actual measurement rendering without booting the full page.
const Dashboard = await readFile(new URL('../Dashboard.js', import.meta.url), 'utf8');
function FunctionText(Name) {
  const Start = Dashboard.indexOf(`function ${Name}(`);
  let End = Dashboard.indexOf('{', Start), Depth = 1;
  const Opening = End++;
  while (Depth) { Depth += (Dashboard[End] === '{') - (Dashboard[End] === '}'); ++End; }
  assert(Opening >= 0);
  return Dashboard.slice(Start, End);
}
const {runInNewContext} = await import('node:vm');
const Container = {replaceChildren(...Rows) { this.Rows = Rows; }};
const Device = {Status: {}, Configuration: {ExternalSourceId: 'test'},
  DisplayName: 'Laptop', MeasurementIds: null, Card: {querySelector() { return Container; }}};
const Context = {Device, Tr: (Key, Fallback) => Fallback,
  CreateElement: (Tag, ClassName, Text) => ({dataset: {}, Text,
    appendChild(Child) { (this.Children ||= []).push(Child); }})};
runInNewContext(['FormatMeasurement', 'CreateMeasurementRow', 'UpdateMeasurements']
  .map(FunctionText).join('\n') + '\nthis.Render = () => UpdateMeasurements(Device);', Context);
for (const [Value, Expected] of [[67, '67 %'], [0, '0 %'], [100, '100 %'],
  [null, '— %'], ['67', '— %'], [-1, '— %'], [101, '— %']]) {
  Device.Status = {BatteryPercent: Value}; Context.Render();
  assert.equal(Container.Rows[0].Children[1].Text, Expected);
}
console.log('Battery rendering: percent, zero/full and invalid values passed.');
