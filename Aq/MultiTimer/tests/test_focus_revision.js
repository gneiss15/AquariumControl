// test_focus_revision.js, Version: 1.00
const fs = require('fs');
const vm = require('vm');
const assert = require('assert');
const path = require('path');
const source = fs.readFileSync(path.join(__dirname, '../WebSrc/Index.js'), 'utf8');
const logic = source.slice(source.indexOf('let ConfigurationRevision = null;'));
let now = 1000, revision = 'old', requests = 0, loads = 0;
const elements = new Map(['SwitchData', 'IntervalData', 'ChannelsText'].map(Id => [Id, {
  innerHTML: '', value: '', querySelectorAll: () => []
}]));
const context = vm.createContext({
  console, Date: {now: () => now},
  document: {hidden: false, getElementById: Id => elements.get(Id), addEventListener: () => {},
    createElement: () => ({remove: () => elements.delete('ConfigurationChangeNotice')})},
  window: {addEventListener: () => {}},
  fetch: async () => { ++requests; return {ok: true, json: async () => ({Revision: revision})}; },
  LoadChannels: async () => { ++loads; elements.get('ChannelsText').value = revision; },
  LoadSwitchTimes: async () => {}, LoadIntervals: async () => {}, Tr: (_, fallback) => fallback
});
elements.set('MainDiv', {prepend: Notice => elements.set(Notice.id, Notice)});
vm.runInContext(logic, context);
(async () => {
  await vm.runInContext('RememberConfigurationRevision()', context);
  assert.equal(requests, 1);
  revision = 'new'; now += 1000;
  await Promise.all([vm.runInContext('CheckConfigurationRevision()', context), vm.runInContext('CheckConfigurationRevision()', context)]);
  assert.equal(requests, 2); assert.equal(loads, 1);
  assert.equal(elements.get('ChannelsText').value, 'new');
  now += 1000;
  await vm.runInContext('CheckConfigurationRevision()', context);
  assert.equal(loads, 1);
  elements.get('ChannelsText').value = 'UNSAVED'; revision = 'newer'; now += 1000;
  await vm.runInContext('CheckConfigurationRevision()', context);
  assert.equal(elements.get('ChannelsText').value, 'UNSAVED'); assert.equal(loads, 1);
  assert(elements.get('ConfigurationChangeNotice').textContent.includes('Unsaved'));
  console.log('Passed focus revision check: coalesced requests, changed-only refresh, unchanged revisions, preserved unsaved edits; no polling timer.');
})().catch(Error => { console.error(Error); process.exitCode = 1; });
