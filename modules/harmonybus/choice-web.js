/* Probability controls are distinct from raw Field Outputs. The DSP owns
 * normalization, leases, latch behavior and sampling; this view only edits. */
(()=>{
'use strict';
const api=window.schwungRemote,prefix=(api.component||'midi_fx1')+':';
const panel=document.getElementById('choices'),rows=document.getElementById('choice-rows'),status=document.getElementById('choice-status');
let busy=false,membership='',held=new Map();
const send=(slot,value)=>api.setParam(prefix+'motif_choice_edit',`${slot},${value}`);
function finish(slot,commit){if(!held.has(slot))return;held.delete(slot);api.setParam(prefix+'motif_choice_release',`${slot},${commit?1:0}`);}
function cancel(){for(const slot of [...held.keys()])finish(slot,false);}
document.getElementById('choice-clear').onclick=()=>{held.clear();api.setParam(prefix+'motif_choice_clear','16');};
document.getElementById('choice-use').onclick=async()=>{
 try{if(String(await api.getParam(prefix+'motion_operation'))!=='Play Motif'){status.textContent='Select a Play Motif operation lane first.';return;}
 api.setParam(prefix+'motion_field_target','3');status.textContent='Selected Play Motif lane now uses weighted user slots.';
 }catch(error){status.textContent=String(error.message||error);}
};
function build(entries){
 cancel();rows.replaceChildren();
 for(const [slot] of entries){
  const row=document.createElement('div'),name=document.createElement('span'),slider=document.createElement('input'),value=document.createElement('output'),clear=document.createElement('button');
  row.style.display='flex';row.style.gap='12px';row.style.alignItems='center';name.textContent=`User ${slot+1}`;slider.type='range';slider.min='0';slider.max='10000';slider.step='1';slider.dataset.slot=String(slot);slider.style.flex='1';slider.setAttribute('aria-label',`User ${slot+1} probability`);
  const edit=()=>{const requested=Number(slider.value);held.set(slot,requested);send(slot,requested);};
  slider.onpointerdown=event=>{slider.setPointerCapture(event.pointerId);edit();};slider.oninput=edit;
  slider.onpointerup=()=>finish(slot,true);slider.onpointercancel=()=>finish(slot,false);slider.onlostpointercapture=()=>finish(slot,false);
  slider.onkeyup=event=>{if(event.key.startsWith('Arrow')||['Home','End','PageUp','PageDown'].includes(event.key))finish(slot,true);};
  slider.onblur=()=>finish(slot,false);
  clear.type='button';clear.textContent='Release';clear.onclick=()=>{held.delete(slot);api.setParam(prefix+'motif_choice_clear',String(slot));};
  row.append(name,slider,value,clear);rows.append(row);
 }
}
async function poll(){
 if(busy||!panel.open||document.hidden)return;busy=true;
 try{
  const parts=String(await api.getParam(prefix+'motif_choice_view')).split(';'),header=parts[0].split(',');
  if(header[0]!=='mc1'||header.length!==6)throw Error('Probability controls require the updated HarmonyBus engine.');
  const [count,last,draws,heldMask,latchedMask]=header.slice(1).map(Number);
  const entries=parts.slice(1).map(part=>part.split(',').map(Number));
  if(![count,last,draws,heldMask,latchedMask].every(Number.isFinite)||count<0||count>16||entries.length!==count||entries.some(entry=>entry.length!==2||!Number.isInteger(entry[0])||entry[0]<0||entry[0]>15||!Number.isFinite(entry[1])||entry[1]<0||entry[1]>1)||new Set(entries.map(entry=>entry[0])).size!==count)throw Error('Invalid probability snapshot');
  const next=entries.map(entry=>entry[0]).join(',');if(next!==membership){membership=next;build(entries);}
  for(let index=0;index<entries.length;index++){
   const [slot,probability]=entries[index],row=rows.children[index],slider=row.children[1];
   // Display the accepted value, including a clamp caused by other holds.
   slider.value=String(Math.round(probability*10000));slider.setAttribute('aria-valuetext',`${(probability*100).toFixed(1)} percent`);
   row.children[2].textContent=`${(probability*100).toFixed(1)}%${latchedMask&(1<<slot)?' · L':heldMask&(1<<slot)?' · held':''}`;
  }
  status.textContent=count?`${count} choices · ${count-1} degrees of freedom · ${draws?`last chosen User ${last+1}`:'no phrase drawn yet'}`:'Record user motifs to populate these choices.';
 }catch(error){status.textContent=String(error.message||error);}finally{busy=false;}
}
panel.ontoggle=()=>{if(!panel.open)cancel();else poll();};
window.addEventListener('blur',cancel);window.addEventListener('pagehide',cancel);document.addEventListener('visibilitychange',()=>{if(document.hidden)cancel();});
setInterval(()=>{if(!document.hidden&&panel.open){for(const [slot,value] of held)send(slot,value);poll();}},250);
})();
