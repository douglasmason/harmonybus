/* Remote control uses Schwung's existing parameter bridge; DSP owns all values. */
/** Mount the same editor in Schwung Remote UI or a composed workspace. */
window.mountHBField=function({root=document,api=window.schwungRemote}={}){
const prefix=(api.component||'midi_fx1')+':';
let active=true,generation=0,disposed=false;
const write=(...args)=>{if(active&&!disposed)api.setParam(...args);};
const timers=[],listeners=[];
const interval=(callback,delay)=>timers.push(setInterval(callback,delay));
const listen=(target,event,callback)=>{target.addEventListener(event,callback);listeners.push([target,event,callback]);};
const container=root.getElementById('outputs'),orientation=root.getElementById('orientation'),bank=root.getElementById('bank'),count=root.getElementById('count'),status=root.getElementById('status'),override=root.getElementById('override');
let values=Array.from({length:16},()=>({raw:.5,value:.5,target:0})),holds=new Map(),vertical=false,bankIndex=0,dirty=false,lastSend=0,busy=false,latchMode=false;
const bound=value=>Math.max(0,Math.min(1,value));

/* One shared XY gesture writes two existing Field output overrides. The DSP
 * remains the sole owner of latch, lease, interpolation and musical timing.
 * A second finger controls Y independently; first finger remains X.
 * Nothing creates MIDI events when the pointer moves. */
const xyPad=root.getElementById('field-xy-pad');
const xyX=root.getElementById('field-xy-x-channel'),xyY=root.getElementById('field-xy-y-channel');
const xyReadout=root.getElementById('field-xy-readout');
const xyCrossX=root.getElementById('field-xy-guide-x'),xyCrossY=root.getElementById('field-xy-guide-y');
const xyCursor=root.getElementById('field-xy-point'),xyTrace=root.getElementById('field-xy-trace');
const xyContacts=new Map(),xyOwned=new Set();
let xyHeld=false,xyTrail=[];
const xyChannels=()=>[Number(xyX?.value??0),Number(xyY?.value??1)];
function paintXY(){
 if(!xyPad)return;
 const [laneX,laneY]=xyChannels();
 const x=holds.get(laneX)??values[laneX]?.value??.5;
 const y=holds.get(laneY)??values[laneY]?.value??.5;
 if(xyCrossX)xyCrossX.style.left=x*100+'%';
 if(xyCrossY)xyCrossY.style.bottom=y*100+'%';
 if(xyCursor){xyCursor.style.left=x*100+'%';xyCursor.style.bottom=y*100+'%';}
 if(xyTrace)xyTrace.setAttribute('points',xyTrail.map(point=>(point.x*100).toFixed(2)+','+((1-point.y)*100).toFixed(2)).join(' '));
 if(xyReadout)xyReadout.textContent='X Field '+(laneX+1)+' '+(x*100).toFixed(1)+'% · Y Field '+(laneY+1)+' '+(y*100).toFixed(1)+'%'+(xyHeld?' · touch':'');
}
function cancelXY(commit=false){
 if(!xyHeld)return;
 const lanes=[...xyOwned],levels=lanes.map(lane=>holds.get(lane));
 xyHeld=false;xyContacts.clear();xyOwned.clear();xyPad?.classList.toggle('xy-held',false);
 if(commit&&active&&!disposed)lanes.forEach((lane,index)=>{
  if(levels[index]!==undefined)write(prefix+'field_output_finish','1,'+lane+','+Math.round(levels[index]*10000));
 });
 for(const lane of lanes)holds.delete(lane);
 send();paintXY();
}
function xyCoords(event){
 const rect=xyPad.getBoundingClientRect();
 if(!(rect.width>0&&rect.height>0))return null;
 return {x:bound((event.clientX-rect.left)/rect.width),y:bound(1-(event.clientY-rect.top)/rect.height)};
}
function xyTracePoint(x,y){
 const last=xyTrail[xyTrail.length-1];
 if(!last||Math.abs(last.x-x)+Math.abs(last.y-y)>.008){
  xyTrail.push({x,y});if(xyTrail.length>24)xyTrail.shift();
 }
}
function xyApply(){
 if(!xyContacts.size||!xyHeld)return;
 const [laneX,laneY]=xyChannels(),points=[...xyContacts.values()];
 const x=points[0].x,y=(points[1]??points[0]).y;
 holds.set(laneX,x);holds.set(laneY,y);
 xyTracePoint(x,y);dirty=true;paintXY();
}
if(xyPad){
 xyPad.onpointerdown=event=>{
  if(!active||disposed||document.hidden||event.button!==0||xyContacts.size>=2)return;
  const channels=xyChannels(),point=xyCoords(event);
  if(!point||channels.some(lane=>!Number.isInteger(lane)||lane<0||lane>=16)||channels[0]===channels[1])return;
  if(!xyHeld&&channels.some(lane=>holds.has(lane))){
   if(xyReadout)xyReadout.textContent='Release the matching Field slider first';
   return;
  }
  event.preventDefault?.();
  if(!xyHeld){xyHeld=true;xyOwned.add(channels[0]);xyOwned.add(channels[1]);xyTrail=[];xyPad.classList.toggle('xy-held',true);}
  xyContacts.set(event.pointerId,point);
  xyPad.setPointerCapture(event.pointerId);xyApply();send();
 };
 xyPad.onpointermove=event=>{
  if(!xyContacts.has(event.pointerId))return;
  const point=xyCoords(event);if(point){xyContacts.set(event.pointerId,point);xyApply();}
 };
 const xyFinish=(event,commit)=>{
  if(!xyContacts.has(event.pointerId))return;
  const point=xyCoords(event);if(point){xyContacts.set(event.pointerId,point);xyApply();}
  xyContacts.delete(event.pointerId);
  if(!xyContacts.size)cancelXY(commit);else xyApply();
 };
 xyPad.onpointerup=event=>xyFinish(event,true);
 xyPad.onpointercancel=event=>xyFinish(event,false);
 xyPad.onlostpointercapture=event=>xyFinish(event,false);
 xyPad.onkeydown=event=>{
  if(event.key==='Escape'){event.preventDefault();cancelXY(false);return;}
  const delta={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowDown:[0,-1],ArrowUp:[0,1]}[event.key];
  if(!delta||!active||disposed||xyContacts.size)return;
  const channels=xyChannels();
  if(channels[0]===channels[1]||!xyHeld&&channels.some(lane=>holds.has(lane)))return;
  event.preventDefault();
  if(!xyHeld){xyHeld=true;xyOwned.add(channels[0]);xyOwned.add(channels[1]);xyTrail=[];xyPad.classList.toggle('xy-held',true);}
  const step=event.shiftKey?0.001:0.01;
  holds.set(channels[0],bound((holds.get(channels[0])??values[channels[0]].value)+delta[0]*step));
  holds.set(channels[1],bound((holds.get(channels[1])??values[channels[1]].value)+delta[1]*step));
  xyTracePoint(holds.get(channels[0]),holds.get(channels[1]));
  dirty=true;paintXY();send();
 };
 xyPad.onkeyup=event=>{if(event.key.startsWith('Arrow')&&xyHeld&&!xyContacts.size)cancelXY(true);};
 xyPad.onblur=()=>cancelXY(false);
 for(const selector of [xyX,xyY])selector.onchange=()=>{cancelXY(false);paintXY();};
}

const placementKeys=['field_distribution','field_spread','field_randomness','field_placement_seed'];
const placementControls=placementKeys.map(key=>root.getElementById(key));
placementControls.forEach((control,index)=>{control.onchange=()=>{write(prefix+placementKeys[index],control.value);};});
let placementBusy=false;
async function pollPlacement(){
 if(placementBusy||!active||document.hidden)return;placementBusy=true;const ticket=generation;
 try{const parts=String(await api.getParam(prefix+'field_placement_view')).split(',');
 if(!active||ticket!==generation)return;
 if(parts[0]==='nfp1'&&parts.length===5&&parts.slice(1).every(value=>Number.isFinite(Number(value))))placementControls.forEach((control,index)=>{if(root.activeElement!==control)control.value=parts[index+1];});
 }finally{placementBusy=false;}
}
const send=()=>{write(prefix+'field_outputs_remote','fo1,'+values.map((_,lane)=>holds.has(lane)?Math.round(holds.get(lane)*10000):-1).join(','));lastSend=Date.now();dirty=false;};
// Cancellation (blur/disconnect/layout) never creates a latch.
const release=()=>{cancelXY(false);if(holds.size){holds.clear();send();}dirty=false;};
const clear=lane=>{if(xyHeld&&(lane===16||xyOwned.has(lane)))cancelXY(false);if(lane===16)holds.clear();else holds.delete(lane);write(prefix+'field_output_clear',String(lane));send();};
function render(){
 container.classList.toggle('vertical',vertical);
 container.style.gridTemplateColumns=vertical?`repeat(${count.value},minmax(50px,1fr))`:'';
 container.replaceChildren();
 for(let strip=0;strip<Number(count.value);strip++){
  const lane=(bankIndex*4+strip)%16,row=document.createElement('div'),label=document.createElement('span'),slider=document.createElement('div'),readout=document.createElement('output'),clearButton=document.createElement('button');
  readout.className='readout';clearButton.className='clear';clearButton.textContent='Release';clearButton.setAttribute('aria-label',`Release lane ${lane+1}`);clearButton.onclick=()=>clear(lane);
  row.className='strip';label.className='label';label.textContent=`Lane ${lane+1}`;slider.className='slider';slider.tabIndex=0;slider.setAttribute('role','slider');slider.setAttribute('aria-label',`Lane ${lane+1} field output`);slider.setAttribute('aria-valuemin','0');slider.setAttribute('aria-valuemax','100');slider.setAttribute('aria-orientation',vertical?'vertical':'horizontal');slider.dataset.lane=lane;
  for(let position=0;position<8;position++){const cell=document.createElement('span');cell.className='cell';slider.append(cell);}
  const marker=document.createElement('span');marker.className='marker';slider.append(marker);
  const move=event=>{const rect=slider.getBoundingClientRect();holds.set(lane,bound(vertical?1-(event.clientY-rect.top)/rect.height:(event.clientX-rect.left)/rect.width));dirty=true;};
  slider.onpointerdown=event=>{if(!active||event.button!==0)return;if(xyOwned.has(lane))cancelXY(false);slider.setPointerCapture(event.pointerId);move(event);send();};
  slider.onpointermove=event=>{if(slider.hasPointerCapture(event.pointerId)&&holds.has(lane))move(event);};
  const end=(commit=false)=>{
   if(commit&&holds.has(lane))write(prefix+'field_output_finish',`1,${lane},${Math.round(holds.get(lane)*10000)}`);
   holds.delete(lane);send();
  };
  slider.onpointerup=()=>end(true);slider.onpointercancel=()=>end();slider.onlostpointercapture=()=>end();
  slider.onkeydown=event=>{if(!active)return;if(['ArrowLeft','ArrowDown','ArrowRight','ArrowUp'].includes(event.key)){event.preventDefault();if(event.repeat&&!holds.has(lane))return;holds.set(lane,bound((holds.get(lane)??values[lane].value)+(['ArrowLeft','ArrowDown'].includes(event.key)?-1:1)*(event.shiftKey?.001:.01)));send();}if(event.key==='Escape')clear(lane);};
  slider.onkeyup=event=>{if(event.key.startsWith('Arrow'))end(true);};slider.onblur=()=>end();
  row.append(label,slider,readout,clearButton);container.append(row);
 }paint();
}
function paint(){
 for(const slider of container.querySelectorAll('.slider')){
  const output=values[Number(slider.dataset.lane)];slider.setAttribute('aria-valuenow',String(Math.round(output.value*1000)/10));
  slider.parentElement.firstChild.textContent=`Lane ${Number(slider.dataset.lane)+1}${output.latched?' · L':''}${output.description?' · '+output.description:output.target?'':' · unmapped'}`;
  slider.parentElement.firstChild.title=output.description||'';
  slider.parentElement.querySelector('.readout').textContent=output.label&&output.target?output.label:(output.value*100).toFixed(1)+'%';
  slider.setAttribute('aria-valuetext',output.description||`${(output.value*100).toFixed(1)}%`);
  [...slider.querySelectorAll('.cell')].forEach((cell,position)=>{const coverage=Math.max(0,1-Math.abs(output.value*7-position));cell.style.background=`rgb(0 ${Math.round(Math.pow(coverage,2.2)*255)} 0)`;});
  const marker=slider.querySelector('.marker');marker.style.opacity=Math.abs(output.raw-output.value)>.005?'.6':'0';
  marker.style.left=vertical?'3px':`${output.raw*100}%`;marker.style.bottom=vertical?`${output.raw*100}%`:'3px';
 }
 paintXY();
}
orientation.onchange=()=>{release();vertical=orientation.value==='1';write(prefix+'field_orientation',orientation.value);render();};
bank.onchange=()=>{release();bankIndex=Number(bank.value);write(prefix+'field_bank',bank.value);render();};count.onchange=()=>{release();render();};root.getElementById('release').onclick=()=>clear(16);
override.onchange=()=>write(prefix+'field_override',override.value);
listen(window,'blur',release);listen(window,'pagehide',release);listen(document,'visibilitychange',()=>{if(document.hidden)release();});
interval(()=>{if((active&&!document.hidden)&&((dirty&&Date.now()-lastSend>=33)||(holds.size&&Date.now()-lastSend>=400)))send();},33);
async function poll(){
 if(busy||!active||document.hidden)return;busy=true;const ticket=generation;
 try{const text=await api.getParam(prefix+'field_outputs_view2');const snapshot=String(text).split(',');if(!active||ticket!==generation)return;
 if(snapshot[0]!=='fo2'||snapshot.length!==56)throw Error('Waiting for a compatible HarmonyBus instance');
 const numbers=snapshot.slice(1).map(Number);if(!numbers.every(Number.isFinite))throw Error('Invalid field snapshot');
 latchMode=numbers[51]===1;override.value=String(Number(latchMode));
 const blocked=numbers[54];if(xyHeld&&[...xyOwned].some(lane=>blocked&(1<<lane)))cancelXY(false);let canceled=false;for(const lane of holds.keys())if(blocked&(1<<lane)){holds.delete(lane);canceled=true;}if(canceled)send();
 const nextVertical=numbers[1]===1,nextBank=numbers[2];const rebuild=nextVertical!==vertical||nextBank!==bankIndex;
 if(rebuild){release();vertical=nextVertical;bankIndex=nextBank;orientation.value=String(Number(vertical));bank.value=String(bankIndex);}
 const labelRows=String(await api.getParam(prefix+'field_output_labels')).split(';');if(!active||ticket!==generation)return;
 const labels=labelRows[0]==='fl1'&&labelRows.length===17?labelRows.slice(1).map(row=>row.split('|')):[];
 values=values.map((_,lane)=>({raw:numbers[3+lane*3]/10000,value:numbers[4+lane*3]/10000,target:numbers[5+lane*3],latched:!!(numbers[52]&(1<<lane)),label:labels[lane]?.[0],description:labels[lane]?.[1]}));
 if(rebuild)render();else paint();status.textContent=latchMode?'Connected · latch overrides':'Connected · momentary overrides';
 }catch(error){if(active&&ticket===generation)status.textContent=String(error.message||error);}finally{busy=false;}
}
render();interval(poll,125);poll();interval(()=>pollPlacement().catch(()=>{}),1000);pollPlacement().catch(()=>{});
return {
 isHeld:()=>holds.size>0,
 suspend(){if(!active)return;release();active=false;generation++;},
 resume(){if(disposed||active)return;active=true;generation++;void poll();},
 release:release,
 dispose(){release();disposed=true;active=false;generation++;for(const timer of timers)clearInterval(timer);for(const [target,event,callback] of listeners)target.removeEventListener(event,callback);}
};
};
if(window.schwungRemote&&document.getElementById('outputs'))window.mountHBField();
