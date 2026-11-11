// test_room_temperature.mjs, Version: 1.01
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const Source=fs.readFileSync(new URL('../Dashboard.js',import.meta.url),'utf8');
const Code=Source.slice(Source.indexOf('function EnsureRoomTemperatureView('),Source.indexOf('function UpdateDeviceStatus('));
let Created=0;
const Context=vm.createContext({TDashboardDeviceView:class {constructor(Owner,Config){Object.assign(this,Config);this.RoomTemperatureOnly=Config.RoomTemperatureOnly;this.StatusRow={hidden:false};}},CreateDeviceCard:()=>{Created++;return {};},SetConnectionState:(Device,State)=>{Device.ConnectionState=State;}});
vm.runInContext(Code,Context);
const Device={ConnectionState:'Live',Configuration:{DeviceType:'MultiTimer'},Views:[{Group:'Aq1'},{Group:'Aq2'}]};
Context.EnsureRoomTemperatureView(Device,{Measurements:[]});assert.equal(Created,0);
Context.EnsureRoomTemperatureView(Device,{Measurements:[{Id:'RoomTemperature',Value:24,Valid:true}]});
assert.equal(Created,1);assert.equal(Device.Views.length,3);assert.equal(Device.Views[2].Group,'System');assert.equal(Device.Views[2].ConnectionState,'Live');
assert.deepEqual(Array.from(Device.Views[2].Channels),[]);assert.deepEqual(Array.from(Device.Views[2].Measurements),['RoomTemperature']);
Context.EnsureRoomTemperatureView(Device,{Measurements:[{Id:'RoomTemperature',Valid:false}]});assert.equal(Created,1);
Context.EnsureRoomTemperatureView(Device,{Measurements:[]});assert.equal(Device.Views[2].StatusRow.hidden,true);
Context.EnsureRoomTemperatureView(Device,{Measurements:[{Id:'RoomTemperature',Value:25}]});assert.equal(Device.Views[2].StatusRow.hidden,false);assert.equal(Created,1);
Context.EnsureRoomTemperatureView({Configuration:{DeviceType:'Feeder'},Views:[]},{Measurements:[{Id:'RoomTemperature'}]});assert.equal(Created,1);
console.log('System room-temperature view: optional, unique, no relay channels, retained aquarium views, disable/re-enable passed.');
