import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';
class Element {
 constructor(){this.children=[];this.style={};this.dataset={};this.className='';this.value='';this.listeners={};this.classList={toggle:()=>{}};this.capture=new Set();}
 append(...children){for(const child of children){child.parentElement=this;this.children.push(child);}}
 replaceChildren(){this.children=[];}
 setAttribute(key,value){this[key]=value;}
 get firstChild(){return this.children[0];}get lastChild(){return this.children.at(-1);}
 querySelectorAll(selector){return this.children.flatMap(child=>[...(child.className===selector.slice(1)?[child]:[]),...child.querySelectorAll(selector)]);}
 querySelector(selector){return this.querySelectorAll(selector)[0];}
 getBoundingClientRect(){return {left:0,top:0,width:100,height:100};}
 setPointerCapture(id){this.capture.add(id);}hasPointerCapture(id){return this.capture.has(id);}
}
const elements=Object.fromEntries(['outputs','orientation','bank','count','status','release','override','field_distribution','field_spread','field_randomness','field_placement_seed'].map(key=>[key,new Element()]));elements.count.value='8';elements.orientation.value=elements.bank.value='0';
const writes=[],timers=[],events={};
const api={component:'midi_fx1',setParam:(key,value)=>writes.push([key,value]),getParam:async(key)=> key.endsWith('field_output_labels')?'fl1;V10%|Velocity amount 10%'+(';--|Unmapped').repeat(15):key.endsWith('field_placement_view')?'nfp1,2,250,75,9':'fo2,1,0,0,'+Array.from({length:16},()=>[5000,3500,1]).flat().join(',')+',1,1,0,0'};
const context={window:{schwungRemote:api,addEventListener:(event,handler)=>events[event]=handler},document:{getElementById:key=>elements[key],createElement:()=>new Element(),hidden:false,addEventListener:(event,handler)=>events[event]=handler},setInterval:handler=>timers.push(handler),Date,Math,Number,String,Map,Array,Error};
vm.runInNewContext(readFileSync(new URL('../modules/harmonybus/field-web.js',import.meta.url),'utf8'),context);
await new Promise(resolve=>setImmediate(resolve));
const sliders=elements.outputs.querySelectorAll('.slider');assert.equal(sliders.length,8);assert.equal(sliders[0]['aria-orientation'],'horizontal');
sliders[0].onpointerdown({button:0,pointerId:1,clientX:25,clientY:0});sliders[1].onpointerdown({button:0,pointerId:2,clientX:75,clientY:0});
let frame=()=>writes.at(-1)[1].split(',').slice(1).map(Number);
assert.equal(frame()[0],2500);assert.equal(frame()[1],7500);
const beforeRelease=writes.length;sliders[0].onpointerup();assert(writes.slice(beforeRelease).some(([key,value])=>key==='midi_fx1:field_output_finish'&&value==='1,0,2500'));assert.equal(frame()[0],-1);assert.equal(frame()[1],7500);
const beforeBlur=writes.length;events.blur();assert(!writes.slice(beforeBlur).some(([key])=>key==='midi_fx1:field_output_finish'));assert(frame().every(value=>value===-1));
assert.equal(sliders[0].querySelectorAll('.cell')[2].style.background,`rgb(0 ${Math.round(.55**2.2*255)} 0)`);
elements.release.onclick();assert(writes.some(([key,value])=>key==='midi_fx1:field_output_clear'&&value==='16'));
assert(sliders[0].parentElement.firstChild.textContent.includes('L'));
sliders[0].onpointerdown({button:0,pointerId:7,clientX:25,clientY:0});elements.release.onclick();
sliders[0].onpointermove({pointerId:7,clientX:75,clientY:0});timers[0]();assert(frame().every(value=>value===-1),'cleared pointer cannot rearm until new press');
console.log('Remote field sliders: bridge, simultaneous holds, independent release, disconnect cleanup, horizontal default and blended brightness pass');

assert.equal(elements.field_distribution.value,'2');assert.equal(elements.field_spread.value,'250');
elements.field_randomness.value='42';elements.field_randomness.onchange();assert.deepEqual(writes.at(-1),['midi_fx1:field_randomness','42']);

assert.equal(sliders[0]['aria-valuetext'],'Velocity amount 10%');assert.equal(sliders[0].parentElement.querySelector('.readout').textContent,'V10%');
