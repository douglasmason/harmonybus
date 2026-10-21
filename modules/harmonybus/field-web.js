/* Remote control uses Schwung's existing parameter bridge; DSP owns all values. */
(()=>{
'use strict';
const api=window.schwungRemote,prefix=(api.component||'midi_fx1')+':';
const container=document.getElementById('outputs'),orientation=document.getElementById('orientation'),bank=document.getElementById('bank'),count=document.getElementById('count'),status=document.getElementById('status'),override=document.getElementById('override');
let values=Array.from({length:16},()=>({raw:.5,value:.5,target:0})),holds=new Map(),vertical=false,bankIndex=0,dirty=false,lastSend=0,busy=false,latchMode=false;
const bound=value=>Math.max(0,Math.min(1,value));
const placementKeys=['field_distribution','field_spread','field_randomness','field_placement_seed'];
const placementControls=placementKeys.map(key=>document.getElementById(key));
placementControls.forEach((control,index)=>{control.onchange=()=>{api.setParam(prefix+placementKeys[index],control.value);};});
let placementBusy=false;
async function pollPlacement(){
 if(placementBusy||document.hidden)return;placementBusy=true;
 try{const parts=String(await api.getParam(prefix+'field_placement_view')).split(',');
 if(parts[0]==='nfp1'&&parts.length===5&&parts.slice(1).every(value=>Number.isFinite(Number(value))))placementControls.forEach((control,index)=>{if(document.activeElement!==control)control.value=parts[index+1];});
 }finally{placementBusy=false;}
}
const send=()=>{api.setParam(prefix+'field_outputs_remote','fo1,'+values.map((_,lane)=>holds.has(lane)?Math.round(holds.get(lane)*10000):-1).join(','));lastSend=Date.now();dirty=false;};
// Cancellation (blur/disconnect/layout) never creates a latch.
const release=()=>{holds.clear();send();};
const clear=lane=>{if(lane===16)holds.clear();else holds.delete(lane);api.setParam(prefix+'field_output_clear',String(lane));send();};
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
  slider.onpointerdown=event=>{if(event.button!==0)return;slider.setPointerCapture(event.pointerId);move(event);send();};
  slider.onpointermove=event=>{if(slider.hasPointerCapture(event.pointerId)&&holds.has(lane))move(event);};
  const end=(commit=false)=>{
   if(commit&&holds.has(lane))api.setParam(prefix+'field_output_finish',`1,${lane},${Math.round(holds.get(lane)*10000)}`);
   holds.delete(lane);send();
  };
  slider.onpointerup=()=>end(true);slider.onpointercancel=()=>end();slider.onlostpointercapture=()=>end();
  slider.onkeydown=event=>{if(['ArrowLeft','ArrowDown','ArrowRight','ArrowUp'].includes(event.key)){event.preventDefault();if(event.repeat&&!holds.has(lane))return;holds.set(lane,bound((holds.get(lane)??values[lane].value)+(['ArrowLeft','ArrowDown'].includes(event.key)?-1:1)*(event.shiftKey?.001:.01)));send();}if(event.key==='Escape')clear(lane);};
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
}
orientation.onchange=()=>{release();vertical=orientation.value==='1';api.setParam(prefix+'field_orientation',orientation.value);render();};
bank.onchange=()=>{release();bankIndex=Number(bank.value);api.setParam(prefix+'field_bank',bank.value);render();};count.onchange=()=>{release();render();};document.getElementById('release').onclick=()=>clear(16);
override.onchange=()=>api.setParam(prefix+'field_override',override.value);
window.addEventListener('blur',release);window.addEventListener('pagehide',release);document.addEventListener('visibilitychange',()=>{if(document.hidden)release();});
setInterval(()=>{if(!document.hidden&&((dirty&&Date.now()-lastSend>=33)||(holds.size&&Date.now()-lastSend>=400)))send();},33);
async function poll(){
 if(busy||document.hidden)return;busy=true;
 try{const text=await api.getParam(prefix+'field_outputs_view2');const snapshot=String(text).split(',');
 if(snapshot[0]!=='fo2'||snapshot.length!==56)throw Error('Waiting for a compatible HarmonyBus instance');
 const numbers=snapshot.slice(1).map(Number);if(!numbers.every(Number.isFinite))throw Error('Invalid field snapshot');
 latchMode=numbers[51]===1;override.value=String(Number(latchMode));
 const blocked=numbers[54];let canceled=false;for(const lane of holds.keys())if(blocked&(1<<lane)){holds.delete(lane);canceled=true;}if(canceled)send();
 const nextVertical=numbers[1]===1,nextBank=numbers[2];const rebuild=nextVertical!==vertical||nextBank!==bankIndex;
 if(rebuild){release();vertical=nextVertical;bankIndex=nextBank;orientation.value=String(Number(vertical));bank.value=String(bankIndex);}
 const labelRows=String(await api.getParam(prefix+'field_output_labels')).split(';');
 const labels=labelRows[0]==='fl1'&&labelRows.length===17?labelRows.slice(1).map(row=>row.split('|')):[];
 values=values.map((_,lane)=>({raw:numbers[3+lane*3]/10000,value:numbers[4+lane*3]/10000,target:numbers[5+lane*3],latched:!!(numbers[52]&(1<<lane)),label:labels[lane]?.[0],description:labels[lane]?.[1]}));
 if(rebuild)render();else paint();status.textContent=latchMode?'Connected · latch overrides':'Connected · momentary overrides';
 }catch(error){status.textContent=String(error.message||error);}finally{busy=false;}
}
render();setInterval(poll,125);poll();setInterval(()=>pollPlacement().catch(()=>{}),1000);pollPlacement().catch(()=>{});
})();
