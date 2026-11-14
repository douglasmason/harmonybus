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
const elements=Object.fromEntries(['outputs','orientation','bank','count','status','release','override','field_distribution','field_spread','field_randomness','field_placement_seed','field-xy-pad','field-xy-x-channel','field-xy-y-channel','field-xy-readout','field-xy-guide-x','field-xy-guide-y','field-xy-point','field-xy-trace'].map(key=>[key,new Element()]));elements.count.value='8';elements.orientation.value=elements.bank.value=elements['field-xy-x-channel'].value='0';elements['field-xy-y-channel'].value='1';
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


// The same Field output bridge now owns a full 2D touch surface. Independent
// fingers share the X and Y channels without creating a second musical engine.
const xy=elements['field-xy-pad'],xyOutput=elements['field-xy-readout'];
const pointer=(id,x,y)=>({button:0,pointerId:id,clientX:x,clientY:y,preventDefault(){}});
const recentFinish=()=>writes.filter(([key])=>key==='midi_fx1:field_output_finish');
const startingFinishes=recentFinish().length;
xy.onpointerdown(pointer(21,25,75));
assert.equal(frame()[0],2500);assert.equal(frame()[1],2500);
assert(xyOutput.textContent.includes('X Field 1 25.0%'));
assert.equal(elements['field-xy-point'].style.left,'25%');
xy.onpointerdown(pointer(22,95,10));
assert.equal(frame()[0],2500,'first finger independently owns X');
assert.equal(frame()[1],9000,'second finger independently owns Y');
xy.onpointermove(pointer(21,75,75));
xy.onpointermove(pointer(22,20,20));
xy.onpointerup(pointer(22,20,20));
// Leaving the second finger switches back to the first finger's Y. The
// final release commits both coordinates once, never midway through a hold.
xy.onpointerup(pointer(21,75,75));
assert.deepEqual(frame().slice(0,2),[-1,-1]);
const committed=recentFinish().slice(startingFinishes).map(([,value])=>value);
assert.deepEqual(committed,['1,0,7500','1,1,2500']);
assert.equal(elements['field-xy-trace'].points.includes('75.00,75.00'),true,'SVG y-coordinate is inverted for vertical touch');
assert(xyOutput.textContent.includes('X Field 1'));
const beforeCancel=recentFinish().length;
xy.onpointerdown(pointer(30,60,40));
xy.onpointercancel(pointer(30,60,40));
assert.equal(recentFinish().length,beforeCancel,'a canceled XY contact never latches');
assert(frame().every(value=>value===-1));
xy.onpointerdown(pointer(31,40,50));
events.pagehide();
assert.equal(recentFinish().length,beforeCancel,'pagehide cancels without committing');
assert(frame().every(value=>value===-1));
xy.onpointermove(pointer(31,90,0));
assert(frame().every(value=>value===-1),'stale movement cannot restart a released touch');
// Keyboard XY editing reuses the same two-lane owner and Release behavior.
const keyboard={key:'ArrowRight',shiftKey:false,preventDefault(){}};
xy.onkeydown(keyboard);
assert.equal(frame()[0],3600);
assert.equal(frame()[1],3500);
xy.onkeyup(keyboard);
assert.equal(recentFinish().length,beforeCancel+2,'keyboard release commits both lanes');
xy.onpointerdown(pointer(33,0,0));
elements.release.onclick();
assert(frame().every(value=>value===-1),'Release all cancels both XY lanes');
const beforeStale=writes.length;
xy.onpointermove(pointer(33,100,100));
assert.equal(writes.length,beforeStale,'cleared XY pointer cannot recreate a hold');
elements['field-xy-y-channel'].value='0';
xy.onpointerdown(pointer(34,80,80));
assert(frame().every(value=>value===-1),'same-lane axis assignment is rejected');
elements['field-xy-y-channel'].value='1';
elements['field-xy-y-channel'].onchange();
console.log('XY head surface: two-finger X/Y ownership, normalized output, keyboard, latch finish, cancellation and same-lane guards pass');
