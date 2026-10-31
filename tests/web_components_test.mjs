import assert from 'node:assert/strict';
import vm from 'node:vm';
import {readFileSync} from 'node:fs';

class Element {
 constructor(){this.children=[];this.style={};this.dataset={};this.className='';this.value='';this.open=true;this.classList={toggle(){}};this.capture=new Set();}
 append(...children){for(const child of children){child.parentElement=this;this.children.push(child);}}
 replaceChildren(...children){this.children=[];this.append(...children);}
 setAttribute(key,value){this[key]=value;}
 get firstChild(){return this.children[0];}
 querySelectorAll(selector){return this.children.flatMap(child=>[...(child.className===selector.slice(1)?[child]:[]),...child.querySelectorAll(selector)]);}
 querySelector(selector){return this.querySelectorAll(selector)[0];}
 getBoundingClientRect(){return {left:0,top:0,width:100,height:100};}
 setPointerCapture(id){this.capture.add(id);}hasPointerCapture(id){return this.capture.has(id);}
}
const listeners=new Set(),timers=new Map();let serial=0;
const target={addEventListener:(event,fn)=>listeners.add(fn),removeEventListener:(event,fn)=>listeners.delete(fn)};
const context={window:{...target},document:{...target,hidden:false,createElement:()=>new Element(),getElementById:()=>null},setInterval:fn=>{timers.set(++serial,fn);return serial;},clearInterval:id=>timers.delete(id),Date,Math,Number,String,Map,Set,Array,Error};
const tick=async()=>{for(const fn of [...timers.values()])fn();await new Promise(resolve=>setImmediate(resolve));};
for(const file of ['field','choice'])vm.runInNewContext(readFileSync(new URL(`../modules/harmonybus/${file}-web.js`,import.meta.url),'utf8'),context);
function mount(kind){
 const ids=kind==='Field'?['outputs','orientation','bank','count','status','release','override','field_distribution','field_spread','field_randomness','field_placement_seed']:['choices','choice-rows','choice-status','choice-use','choice-clear'];
 const elements=Object.fromEntries(ids.map(id=>[id,new Element()]));if(kind==='Field'){elements.count.value='8';elements.orientation.value=elements.bank.value='0';}
 const writes=[];let reads=0,pending=null;
 const api={component:'midi_fx1',setParam:(...args)=>writes.push(args),getParam:async key=>{
  reads++;if(pending)return pending;
  if(key.endsWith('motif_choice_view'))return 'mc1,1,-1,0,0,0;0,1';
  if(key.endsWith('motion_operation'))return 'Play Motif';
  if(key.endsWith('field_placement_view'))return 'nfp1,2,250,75,9';
  if(key.endsWith('field_output_labels'))return 'fl1'+(';V|Velocity').repeat(16);
  return 'fo2,1,0,0,'+Array.from({length:16},()=>[5000,3500,1]).flat().join(',')+',1,1,0,0';
 }};
 const component=context.window['mountHB'+kind]({root:{getElementById:id=>elements[id]},api});
 return {component,elements,writes,get reads(){return reads;},set pending(value){pending=value;}};
}
for(const kind of ['Field','Choices']){
 const a=mount(kind),b=mount(kind);await tick();
 const slider=kind==='Field'?a.elements.outputs.querySelector('.slider'):a.elements['choice-rows'].children[0].children[1];
 if(kind==='Field')slider.onpointerdown({button:0,pointerId:1,clientX:25,clientY:0});else{slider.value='2500';slider.oninput();}
 assert(a.component.isHeld());assert(!b.component.isHeld());assert.equal(b.writes.length,0,'independent mounts do not send each other edits');
 a.component.suspend();assert(!a.component.isHeld());const cancelled=a.writes.at(-1);
 assert(kind==='Field'?cancelled[1].split(',').slice(1).every(value=>value==='-1'):cancelled[1]==='0,0','suspension cancels without latching');
 const reads=a.reads,writes=a.writes.length;await tick();assert.equal(a.reads,reads);assert.equal(a.writes.length,writes);
 a.component.resume();await tick();assert(a.reads>reads);
 if(kind==='Choices'){
  let resolve;a.pending=new Promise(done=>resolve=done);
  const edit=a.elements['choice-use'].onclick();a.component.suspend();const before=a.writes.length;
  resolve('Play Motif');await edit;assert.equal(a.writes.length,before,'late mapping read cannot write into a changed context');
 }
 a.component.dispose();b.component.dispose();assert.equal(timers.size,0);assert.equal(listeners.size,0);
 a.component.resume();await tick();assert.equal(timers.size,0,'disposed components cannot restart');
}
console.log('Shared web components: isolated roots, cancellation, suspension, stale replies and disposal pass');
