import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';
class Element {
 constructor(){this.children=[];this.style={};this.dataset={};this.value='';this.open=false;}
 append(...children){this.children.push(...children);}
 replaceChildren(){this.children=[];}
 setAttribute(key,value){this[key]=value;}
 setPointerCapture(){}
}
const elements=Object.fromEntries(['choices','choice-rows','choice-status','choice-use','choice-clear'].map(key=>[key,new Element()]));
const writes=[],timers=[],events={};let reads=0,snapshot='mc1,3,-1,0,0,0;0,0.2;1,0.3;2,0.5',operation='Velocity';
const api={component:'midi_fx1',setParam:(...args)=>writes.push(args),getParam:async key=>{reads++;return key.endsWith('motion_operation')?operation:snapshot;}};
const context={window:{schwungRemote:api,addEventListener:(event,handler)=>events[event]=handler},document:{getElementById:key=>elements[key],createElement:()=>new Element(),hidden:false,addEventListener:(event,handler)=>events[event]=handler},setInterval:handler=>timers.push(handler),Map,Set,Number,String,Error};
vm.runInNewContext(readFileSync(new URL('../modules/harmonybus/choice-web.js',import.meta.url),'utf8'),context);
const settle=()=>new Promise(resolve=>setImmediate(resolve));
timers[0]();await settle();assert.equal(reads,0,'closed disclosure does not poll');
elements.choices.open=true;elements.choices.ontoggle();await settle();
assert.equal(elements['choice-rows'].children.length,3);
const sliders=elements['choice-rows'].children.map(row=>row.children[1]);
sliders[0].value='6000';sliders[0].oninput();sliders[1].value='8000';sliders[1].oninput();
assert.deepEqual(writes.slice(-2),[['midi_fx1:motif_choice_edit','0,6000'],['midi_fx1:motif_choice_edit','1,8000']]);
snapshot='mc1,3,2,1,3,0;0,0.6;1,0.4;2,0';timers[0]();await settle();assert.equal(sliders[1].value,'4000','show accepted clamp');
sliders[0].onpointerup();assert.deepEqual(writes.at(-1),['midi_fx1:motif_choice_release','0,1']);
events.blur();assert.deepEqual(writes.at(-1),['midi_fx1:motif_choice_release','1,0']);
const previous=writes.length;await elements['choice-use'].onclick();assert.equal(writes.length,previous,'wrong operation is not changed');
operation='Play Motif';await elements['choice-use'].onclick();assert.deepEqual(writes.at(-1),['midi_fx1:motion_field_target','3']);
elements['choice-clear'].onclick();assert.deepEqual(writes.at(-1),['midi_fx1:motif_choice_clear','16']);
snapshot='mc1,3,2,1,0,0;0,NaN;1,0.4;2,0.6';timers[0]();await settle();assert.match(elements['choice-status'].textContent,/Invalid/);
console.log('Remote probability controls: exact edits, clamp feedback, release, disconnect, mapping guard and idle polling passed');
