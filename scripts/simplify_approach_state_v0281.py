#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

module = json.loads(MODULE.read_text())
module["version"] = "0.2.81"
module["name"] = "Harmony Bus 0.2.81"
module["abbrev"] = "HB281"
module["description"] = "Harmony Bus v0.2.81 — simplified and tested follower modifier state"
MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = source.replace('/* Harmony Bus v0.2.80 — Schwung MIDI FX. */', '/* Harmony Bus v0.2.81 — Schwung MIDI FX. */')
source = source.replace('#define HB_VERSION "0.2.80"', '#define HB_VERSION "0.2.81"')

include_anchor = '#include "../../../src/harmony_core.h"\n'
include_new = include_anchor + '#include "../../../src/approach_state.h"\n'
if '#include "../../../src/approach_state.h"' not in source:
    if include_anchor not in source:
        raise SystemExit("harmony_core include anchor not found")
    source = source.replace(include_anchor, include_new, 1)

# Keep legacy fields for persistence/ABI compatibility, but make the active
# decision strictly persistent + per-instance next-note override.
old_active = '''static int hb_active_approach(const Inst *instance){
    if(!instance)return 1;
    if(instance->approach_pad_armed!=1)return instance->approach_pad_armed;
    if(g_bus.approach_mode==1){
        if(instance->approach_below_held)return 0;
        if(instance->approach_above_held)return 2;
    }
    return g_bus.approach_control;
}
'''
new_active = '''static int hb_active_approach(const Inst *instance){
    if(!instance)return HB_APPROACH_OFF;
    return hb_approach_effective(g_bus.approach_control,instance->approach_pad_armed);
}
'''
if old_active in source:
    source = source.replace(old_active, new_active, 1)
elif new_active not in source:
    raise SystemExit("hb_active_approach block not found")

old_consume = '''static void hb_consume_next_approach(Inst *instance){
    if(!instance)return;
    instance->approach_pad_armed=1;
}
'''
new_consume = '''static void hb_consume_next_approach(Inst *instance){
    if(!instance)return;
    instance->approach_pad_armed=hb_approach_consume_next(instance->approach_pad_armed);
}
'''
if old_consume in source:
    source = source.replace(old_consume, new_consume, 1)
elif new_consume not in source:
    raise SystemExit("hb_consume_next_approach block not found")

# Replace the old Behavior-dependent parameter handling with a two-layer state:
# persistent toggle/manual value + optional next-note override.
start = source.find('if(!strcmp(key,"live_press"))')
end = source.find('if(!strcmp(key,"track_role")||!strcmp(key,"role"))', start)
if start < 0 or end < 0:
    raise SystemExit("set_param modifier prefix not found")
new_prefix = '''if(!strcmp(key,"live_press")){if(parameter[0]=='1')hb_receive_live_vouch(instance);return;}
if(!strcmp(key,"approach_below_pad")){if(parameter[0]=='1'){instance->approach_pad_armed=HB_APPROACH_CHROM_BELOW;}return;}
if(!strcmp(key,"approach_above_pad")){if(parameter[0]=='1'){instance->approach_pad_armed=HB_APPROACH_SCALE_ABOVE;}return;}
if(!strcmp(key,"approach_reset")){if(parameter[0]=='1'||!strcmp(parameter,"Reset")){g_bus.approach_control=HB_APPROACH_OFF;instance->approach_pad_armed=HB_APPROACH_OFF;instance->approach_below_held=0;instance->approach_above_held=0;}return;}
if(!strcmp(key,"mod_scale_above")){int on=parameter[0]=='1'||!strcmp(parameter,"On");g_bus.approach_control=hb_approach_toggle(g_bus.approach_control,HB_APPROACH_SCALE_ABOVE,on);instance->approach_pad_armed=HB_APPROACH_OFF;return;}
if(!strcmp(key,"mod_chrom_below")){int on=parameter[0]=='1'||!strcmp(parameter,"On");g_bus.approach_control=hb_approach_toggle(g_bus.approach_control,HB_APPROACH_CHROM_BELOW,on);instance->approach_pad_armed=HB_APPROACH_OFF;return;}
if(!strcmp(key,"approach_scale_next")){if(parameter[0]=='1'||!strcmp(parameter,"Scale +"))instance->approach_pad_armed=HB_APPROACH_SCALE_ABOVE;return;}
if(!strcmp(key,"approach_chrom_next")){if(parameter[0]=='1'||!strcmp(parameter,"Chrom -"))instance->approach_pad_armed=HB_APPROACH_CHROM_BELOW;return;}
'''
source = source[:start] + new_prefix + source[end:]

# Manual K1 is another way to set the persistent state. Behavior is retained as
# a compatibility parameter only; it cannot alter modifier semantics anymore.
old_manual = 'else if(!strcmp(key,"approach")){g_bus.approach_control=enum_index(parameter,APPROACH_OPTS,3,g_bus.approach_control);}else if(!strcmp(key,"approach_mode")){g_bus.approach_mode=enum_index(parameter,APPROACH_MODE_OPTS,3,g_bus.approach_mode);instance->approach_pad_armed=1;instance->approach_below_held=0;instance->approach_above_held=0;}'
new_manual = 'else if(!strcmp(key,"approach")){g_bus.approach_control=enum_index(parameter,APPROACH_OPTS,3,HB_APPROACH_OFF);instance->approach_pad_armed=HB_APPROACH_OFF;}else if(!strcmp(key,"approach_mode")){g_bus.approach_mode=0;instance->approach_pad_armed=HB_APPROACH_OFF;instance->approach_below_held=0;instance->approach_above_held=0;}'
if old_manual in source:
    source = source.replace(old_manual, new_manual, 1)
elif new_manual not in source:
    raise SystemExit("manual/legacy behavior block not found")

old_queue_head = '''    int emitted=0;
    int next_approach=(g_bus.approach_mode==0)?hb_active_approach(instance):1;
    int next_approach_used=0;
    double next_arrival=-1.0;
'''
new_queue_head = '''    int emitted=0;
    int next_override=hb_approach_normalize(instance->approach_pad_armed);
    int active_approach=hb_approach_effective(g_bus.approach_control,next_override);
    int next_approach_used=0;
    double next_arrival=-1.0;
'''
if old_queue_head in source:
    source = source.replace(old_queue_head, new_queue_head, 1)
elif new_queue_head not in source:
    raise SystemExit("queue modifier head not found")

old_apply = '''            int approach=(g_bus.approach_mode==0)?next_approach:hb_active_approach(instance);
            if(approach!=1){
                int apply=1;
                if(g_bus.approach_mode==0){
                    double arrival=instance->follower_queue_arrival_beat[index];
                    if(next_arrival<0.0)next_arrival=arrival;
                    double difference=arrival-next_arrival;
                    if(difference<0.0)difference=-difference;
                    /* One physical chord/gesture may serialize into several
                       MIDI note-ons. Consume once, but decorate the whole
                       effectively-simultaneous group. */
                    if(difference>0.002)apply=0;
                }
                if(apply){
                    mapped=hb_apply_approach(instance,mapped,approach);
                    if(g_bus.approach_mode==0)next_approach_used=1;
                }
            }
'''
new_apply = '''            int approach=active_approach;
            if(approach!=HB_APPROACH_OFF){
                int apply=1;
                if(next_override!=HB_APPROACH_OFF){
                    double arrival=instance->follower_queue_arrival_beat[index];
                    if(next_arrival<0.0)next_arrival=arrival;
                    double difference=arrival-next_arrival;
                    if(difference<0.0)difference=-difference;
                    /* A one-shot decorates one physical chord/gesture, even if
                       its note-ons serialize. Persistent toggles apply to every
                       subsequent gesture until explicitly disabled. */
                    if(difference>0.002)apply=0;
                }
                if(apply){
                    mapped=hb_apply_approach(instance,mapped,approach);
                    if(next_override!=HB_APPROACH_OFF)next_approach_used=1;
                }
            }
'''
if old_apply in source:
    source = source.replace(old_apply, new_apply, 1)
elif new_apply not in source:
    raise SystemExit("queue modifier application block not found")

# Make all neutral initialization/reset sites explicit in enum terms.
source = source.replace('instance->approach_pad_armed=1;', 'instance->approach_pad_armed=HB_APPROACH_OFF;')
source = source.replace('static SharedBus g_bus={.approach_control=1,.approach_mode=0};', 'static SharedBus g_bus={.approach_control=HB_APPROACH_OFF,.approach_mode=0};')

DSP.write_text(source)
