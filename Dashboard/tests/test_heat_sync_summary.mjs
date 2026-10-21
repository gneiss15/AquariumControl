// test_heat_sync_summary.mjs, Version: 1.01
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
const Source = await readFile(new URL('../Dashboard.js', import.meta.url), 'utf8');
const FunctionSource = Source.slice(Source.indexOf('function RenderHeatProfileStatus('), Source.indexOf('\nasync function LoadCentralHeatProfiles'));
const Element = (Tag, Class = '', Text = '') => ({Tag, Class, Text, Children: [],
  append(...Items) { this.Children.push(...Items); },
  replaceChildren(...Items) { this.Children = Items; }, classList: {Values: new Set(), add(...Classes) {Classes.forEach(Class => this.Values.add(Class));},
    remove(...Classes) {Classes.forEach(Class => this.Values.delete(Class));}}});
const Message = Element('div');
const Context = vm.createContext({CentralHeatProfileStatus: null,
  Devices: [1, 2, 3].map(Number => ({TechnicalName: `Heater${Number}`, Configuration: {Group: `Aq${Number}`}})),
  document: {getElementById: () => Message}, CreateElement: Element,
  DeviceDisplayName: Name => Name.startsWith('Heater') ? 'Heizung Haupt' : 'Zeitschaltuhr Aq1 & Aq2',
  Tr: (_Key, Default) => Default});
vm.runInContext(FunctionSource, Context);
const Render = Store => {Context.Store = Store; vm.runInContext('RenderHeatProfileStatus(Store)', Context);};
const Text = Node => typeof Node === 'string' ? Node : Node.Text + Node.Children.map(Text).join('');
Render({Revision: 'R', Sets: [{Device: 'Heater1', Synced: true}, {Device: 'Heater2', Synced: true},
  {Device: 'Heater3', Synced: false, Error: 'offline'}]});
assert.equal(Text(Message), 'Revision: R · synchron: Heizung Haupt Aq1, Heizung Haupt Aq2nicht synchron: Heizung Haupt Aq3: offline');
assert.equal(Message.Children.at(-1).Tag, 'div');
assert.equal(Message.Children.at(-1).Class, 'HeatSyncPending');
assert(Message.classList.Values.has('HeatSyncSummary'), 'revision must use the neutral summary style');
const Css = await readFile(new URL('../Index.css', import.meta.url), 'utf8');
assert.match(Css, /\.DashboardMessage\.HeatSyncSummary\s*\{[^}]*color:\s*var\(\s*--Text\s*\)/);
assert.match(Css, /\.HeatSyncPending\s*\{[^}]*color:\s*var\(\s*--Danger\s*\)/);
Context.Devices.push({TechnicalName: 'MT', Configuration: {GroupViews: [{Group: 'Aq1'}, {Group: 'Aq2'}]}});
Render({Sets: [{Device: 'MT', Synced: true}]});
assert.equal(Text(Message), 'Revision: ? · synchron: Zeitschaltuhr Aq1 & Aq2');
Render({Sets: []});
assert.match(Text(Message), /Noch keine Geräte synchronisiert/);
const LoadSource = Source.slice(Source.indexOf('async function LoadCentralHeatProfiles('), Source.indexOf('\nasync function SaveCentralHeatProfiles('));
Context.CentralChannelDrafts = {clear() {}};
Context.FetchDashboardJson = async () => {throw new Error('offline');};
vm.runInContext(LoadSource, Context);
assert.equal(await vm.runInContext('LoadCentralHeatProfiles()', Context), false);
assert(!Message.classList.Values.has('HeatSyncSummary'), 'load failures must retain the error style');
console.log('Heat sync summary: distinct aquariums, separate errors, no duplicate suffix and empty participants passed.');
