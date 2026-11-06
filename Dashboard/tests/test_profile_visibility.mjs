// test_profile_visibility.mjs, Version: 1.01
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import vm from 'node:vm';
const Source=await readFile(new URL('../Dashboard.js',import.meta.url),'utf8');
class Element {
 constructor(Tag){this.Tag=Tag;this.dataset={};this.Children=[];this.events={};this.value='';}
 appendChild(Child){Child.parentElement=this;this.Children.push(Child);return Child;}
 replaceChildren(...Items){this.Children=[];Items.forEach(Item=>this.appendChild(Item));}
 set value(Value){this.Value=String(Value);}
 get value(){return this.Value;}
 setAttribute(){}
 addEventListener(Event,Call){this.events[Event]=Call;}
 querySelector(Selector){const Key=Selector.match(/data-key="([^"]+)"/)?.[1];for(const Child of this.Children){if(Key&&Child.dataset.key===Key)return Child;const Match=Child.querySelector(Selector);if(Match)return Match;}return null;}
}
const Editor=new Element('div');
const Context=vm.createContext({CentralHeatProfiles:{Profiles:[{Id:'Heat',Measurement:'Temperature',FloorOn:27,FloorOff:28,MainOn:27,MainOff:28},
 {Id:'Charge',Measurement:'BatteryPercent',ChargeRunOn:true,ChargeSecondsPerPercent:15,FloorOn:40,FloorOff:80,MainOn:40,MainOff:80},
 {Id:'PI',Measurement:'Temperature',FloorOn:27,FloorOff:28,MainOn:27,MainOff:28,Pi:{Mode:'Active',Setpoint:28}}]},
 document:{createElement:Tag=>new Element(Tag),getElementById:()=>Editor},
 CreateElement:(Tag,Class,Text)=>Object.assign(new Element(Tag),{className:Class,textContent:Text}),Tr:(_Key,Text)=>Text,ProfileReferences:()=>[]});
vm.runInContext(Source.slice(Source.indexOf('function RenderHeatProfiles()'),Source.indexOf('\nfunction ChannelDraftKey')),Context);
Context.RenderHeatProfiles();
const Get=(Index,Key)=>Editor.Children[Index].querySelector(`[data-key="${Key}"]`);
assert.equal(Get(0,'ChargeRunOn').parentElement.hidden,true);
assert.equal(Get(0,'FloorOn').parentElement.hidden,false);
assert.equal(Get(1,'FloorOn').parentElement.hidden,true);
assert.equal(Get(1,'NoTempOn').parentElement.hidden,true);
assert.equal(Get(1,'ChargeSecondsPerPercent').parentElement.hidden,false);
Get(1,'ChargeRunOn').checked=false;Get(1,'ChargeRunOn').events.change();
assert.equal(Get(1,'ChargeSecondsPerPercent').parentElement.hidden,true);
assert.equal(Get(1,'ChargeSecondsPerPercent').value,'15');
Get(0,'Measurement').value='BatteryPercent';Get(0,'Measurement').events.input();
assert.equal(Get(0,'ChargeRunOn').parentElement.hidden,false);
assert.equal(Get(0,'FloorOn').value,'27');
const Css=await readFile(new URL('../Index.css',import.meta.url),'utf8');
assert.match(Css,/\.HeatProfileField\[hidden\]\s*\{\s*display:\s*none\s*!important/);
console.log('Profile visibility follows measurement/run-on and keeps hidden input values; CSS honors hidden.');

assert.equal(Get(2,'FloorOn').parentElement.hidden,true);
assert.equal(Get(2,'MainOn').parentElement.hidden,true);
assert.equal(Get(2,'NoTempOn').parentElement.hidden,false);
const Walk=Element=>[Element,...Element.Children.flatMap(Walk)];
assert.equal(Walk(Editor.Children[2]).find(Element=>Element.dataset.piKey==='Setpoint').value,'28');

const Mode=Walk(Editor.Children[2]).find(Element=>Element.dataset.piKey==='Mode');
Mode.value='Off';Mode.events.change();assert.equal(Get(2,'MainOn').parentElement.hidden,false);
Mode.value='Active';Mode.events.change();assert.equal(Get(2,'MainOn').parentElement.hidden,true);
