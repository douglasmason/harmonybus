#!/usr/bin/env python3
"""Publish v0.2.88: learned conductor-loop harmony lookahead.

The feature deliberately reuses HarmonyBus's existing clip-local playhead and
loop bounds, but learns harmony from the realtime conductor stream.  It never
uses ahead-of-time clip note contents for prediction.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"


def upsert(params: list[dict[str, object]], spec: dict[str, object]) -> None:
    key = spec["key"]
    for index, item in enumerate(params):
        if item.get("key") == key:
            params[index] = spec
            return
    params.append(spec)


module = json.loads(MODULE.read_text())
module["version"] = "0.2.88"
module["name"] = "Harmony Bus 0.2.88"
module["abbrev"] = "HB288"
module["description"] = "Harmony Bus v0.2.88 — learned loop harmony lookahead"

caps = module["capabilities"]
hierarchy = caps["ui_hierarchy"]
hierarchy["levels"]["root"]["name"] = "Harmony Bus 0.2.88"
chain_params: list[dict[str, object]] = caps["chain_params"]

next_specs: list[dict[str, object]] = [
    {"key": "next_predict", "name": "Predict", "type": "enum", "options": ["Off", "On"], "options_as_string": True, "default": "Off"},
    {"key": "next_lookahead", "name": "Lookahead", "type": "enum", "options": ["Off", "1/32", "1/16", "1/8", "1/4", "1/2", "1 Bar"], "options_as_string": True, "default": "1/8"},
    {"key": "next_model", "name": "Model", "type": "string", "access": "read"},
    {"key": "next_shift", "name": "Shift", "type": "string", "access": "read"},
    {"key": "next_loop_length", "name": "Loop Length", "type": "string", "access": "read"},
    {"key": "next_position", "name": "Position", "type": "string", "access": "read"},
    {"key": "next_harmony", "name": "Next", "type": "string", "access": "read"},
    {"key": "next_reset", "name": "Reset Learn", "type": "enum", "options": ["Off", "Reset"], "options_as_string": True, "default": "Off"},
]
for spec in next_specs:
    upsert(chain_params, spec)

hierarchy["levels"]["next_harm"] = {
    "name": "Next Harm",
    "params": [dict(spec) for spec in next_specs],
    "knobs": [str(spec["key"]) for spec in next_specs],
}
root_params = hierarchy["levels"]["root"]["params"]
if not any(item.get("level") == "next_harm" for item in root_params):
    insert_at = next((index for index, item in enumerate(root_params) if item.get("level") == "ui_test"), len(root_params))
    root_params.insert(insert_at, {"level": "next_harm", "label": "Next Harm"})

MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = re.sub(r'/\* Harmony Bus v0\.2\.\d+ — Schwung MIDI FX\. \*/',
                '/* Harmony Bus v0.2.88 — Schwung MIDI FX. */', source, count=1)
source = re.sub(r'#define HB_VERSION "0\.2\.\d+"', '#define HB_VERSION "0.2.88"', source, count=1)

# Storage: observed harmony is kept separate from the shifted/effective bus so
# the predictor can never train on its own output.
if "HB_MAX_LOOP_HARMONIES" not in source:
    source = source.replace(
        "#define HB_MAX_CLIP_NOTES 1024\n",
        "#define HB_MAX_CLIP_NOTES 1024\n#define HB_MAX_LOOP_HARMONIES 64\n",
        1,
    )

if "hb_loop_harmony_event_t" not in source:
    anchor = '''typedef struct {
    int note;
    double start;
    double duration;
} hb_clip_note_t;
'''
    addition = anchor + '''typedef struct {
    double phase;
    hb_harmony_t harmony;
} hb_loop_harmony_event_t;
'''
    if anchor not in source:
        raise SystemExit("clip-note typedef anchor not found")
    source = source.replace(anchor, addition, 1)

old_struct_tail = "double last_clip_playhead; int have_last_clip_playhead; unsigned cache_rev;"
new_struct_tail = (
    "double last_clip_playhead; int have_last_clip_playhead; "
    "int next_predict; int next_lookahead; int next_model_locked; int next_shift_active; "
    "int next_learning_count; int next_model_count; double next_last_playhead; int next_have_playhead; "
    "hb_harmony_t observed_harmony; hb_loop_harmony_event_t next_learning[HB_MAX_LOOP_HARMONIES]; "
    "hb_loop_harmony_event_t next_model[HB_MAX_LOOP_HARMONIES]; unsigned cache_rev;"
)
if old_struct_tail in source:
    source = source.replace(old_struct_tail, new_struct_tail, 1)
elif "next_model_locked" not in source:
    raise SystemExit("SharedBus playhead tail anchor not found")

# Initialize the feature Off, with a useful 1/8-note lookahead selection ready
# for the user to enable.  Knowledge starts empty.
init_anchor = "g_bus.last_clip_playhead=0.0;g_bus.have_last_clip_playhead=0;g_bus.cache_rev=0;"
init_new = (
    "g_bus.last_clip_playhead=0.0;g_bus.have_last_clip_playhead=0;"
    "g_bus.next_predict=0;g_bus.next_lookahead=3;g_bus.next_model_locked=0;g_bus.next_shift_active=0;"
    "g_bus.next_learning_count=0;g_bus.next_model_count=0;g_bus.next_last_playhead=0.0;g_bus.next_have_playhead=0;"
    "memset(&g_bus.observed_harmony,0,sizeof(g_bus.observed_harmony));g_bus.cache_rev=0;"
)
if init_anchor in source:
    source = source.replace(init_anchor, init_new, 1)
elif "g_bus.next_predict=0" not in source:
    raise SystemExit("ensure_init playhead anchor not found")

# Insert the learned-loop engine immediately after the ordinary bus accessor.
if "static void hb_next_update_playhead" not in source:
    anchor = '''static hb_harmony_t bus_read(void){hb_harmony_t harmony;memset(&harmony,0,sizeof(harmony));for(int tries=0;tries<3;tries++){unsigned before=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE),after;if(before&1u)continue;harmony=g_bus.harmony;after=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);if(before==after&&!(after&1u))return harmony;}return harmony;}
'''
    engine = r'''
static hb_harmony_t hb_observed_read(void){return g_bus.observed_harmony;}
static int hb_harmony_equal_effective(hb_harmony_t left,hb_harmony_t right){
    if(left.valid!=right.valid)return 0;
    if(!left.valid)return 1;
    return left.root_pc==right.root_pc&&left.chord_index==right.chord_index&&left.bass_pc==right.bass_pc;
}
static void hb_effective_write(hb_harmony_t harmony){
    if(!hb_harmony_equal_effective(bus_read(),harmony))bus_write(harmony);
}
static double hb_next_lookahead_beats(void){
    static const double beats[7]={0.0,0.125,0.25,0.5,1.0,2.0,4.0};
    int index=g_bus.next_lookahead;
    return (index>=0&&index<7)?beats[index]:0.0;
}
static double hb_next_loop_length(void){
    double length=g_bus.clip_loop_end-g_bus.clip_loop_start;
    return length>1e-6?length:0.0;
}
static double hb_next_phase(double playhead){
    double length=hb_next_loop_length();
    if(length<=0.0)return 0.0;
    double phase=playhead-g_bus.clip_loop_start;
    while(phase>=length)phase-=length;
    while(phase<0.0)phase+=length;
    return phase;
}
static double hb_next_normalize_phase(double phase){
    double length=hb_next_loop_length();
    if(length<=0.0)return phase;
    while(phase>=length)phase-=length;
    while(phase<0.0)phase+=length;
    return phase;
}
static double hb_next_record_phase(void){
    double phase=hb_next_phase(hb_clip_playhead());
    double grid=hb_chord_grid_beats();
    if(grid<=0.0)return phase;
    double anticipation=hb_anticipation_beats();
    double shifted=(phase+anticipation)/grid;
    long nearest=(long)(shifted+0.5);
    return hb_next_normalize_phase((double)nearest*grid-anticipation);
}
static void hb_next_reset_knowledge(void){
    g_bus.next_model_locked=0;
    g_bus.next_shift_active=0;
    g_bus.next_learning_count=0;
    g_bus.next_model_count=0;
    g_bus.next_have_playhead=0;
    g_bus.next_last_playhead=0.0;
}
static void hb_next_record_observed(hb_harmony_t harmony){
    g_bus.observed_harmony=harmony;
    if(!harmony.valid)return;
    double phase=hb_next_record_phase();
    if(g_bus.next_learning_count>0){
        hb_loop_harmony_event_t *last=&g_bus.next_learning[g_bus.next_learning_count-1];
        if(hb_harmony_equal_effective(last->harmony,harmony))return;
    }
    if(g_bus.next_learning_count>=HB_MAX_LOOP_HARMONIES)return;
    hb_loop_harmony_event_t *event=&g_bus.next_learning[g_bus.next_learning_count++];
    event->phase=phase;
    event->harmony=harmony;
}
static void hb_commit_observed_harmony(hb_harmony_t harmony){
    hb_next_record_observed(harmony);
    if(!g_bus.next_predict||!g_bus.next_model_locked)hb_effective_write(harmony);
}
static int hb_next_model_event_for_phase(double phase,int shifted){
    if(!g_bus.next_model_locked||g_bus.next_model_count<=0)return -1;
    double length=hb_next_loop_length();
    if(length<=0.0)return -1;
    double lookahead=shifted?hb_next_lookahead_beats():0.0;
    int best=-1;double best_age=1e99;
    for(int index=0;index<g_bus.next_model_count;index++){
        double event_phase=hb_next_normalize_phase(g_bus.next_model[index].phase-lookahead);
        double age=phase-event_phase;
        if(age<0.0)age+=length;
        if(age<best_age){best_age=age;best=index;}
    }
    return best;
}
static int hb_next_upcoming_event(double phase){
    if(!g_bus.next_model_locked||g_bus.next_model_count<=0)return -1;
    double length=hb_next_loop_length();
    if(length<=0.0)return -1;
    int best=-1;double best_distance=1e99;
    for(int index=0;index<g_bus.next_model_count;index++){
        double distance=g_bus.next_model[index].phase-phase;
        if(distance<=1e-6)distance+=length;
        if(distance<best_distance){best_distance=distance;best=index;}
    }
    return best;
}
static void hb_next_promote_learning(void){
    if(g_bus.next_learning_count<=0)return;
    g_bus.next_model_count=g_bus.next_learning_count;
    for(int index=0;index<g_bus.next_model_count;index++)g_bus.next_model[index]=g_bus.next_learning[index];
    g_bus.next_learning_count=0;
    g_bus.next_model_locked=1;
}
static void hb_next_apply_effective(double playhead){
    if(!g_bus.next_predict||!g_bus.next_model_locked||g_bus.next_model_count<=0){
        g_bus.next_shift_active=0;
        hb_effective_write(g_bus.observed_harmony);
        return;
    }
    double phase=hb_next_phase(playhead);
    int index=hb_next_model_event_for_phase(phase,1);
    if(index<0)return;
    hb_loop_harmony_event_t *event=&g_bus.next_model[index];
    hb_effective_write(event->harmony);
    double length=hb_next_loop_length();
    double ahead=event->phase-phase;
    if(ahead<0.0)ahead+=length;
    double lookahead=hb_next_lookahead_beats();
    g_bus.next_shift_active=(lookahead>0.0&&ahead>1e-6&&ahead<=lookahead+1e-6)?1:0;
}
static void hb_next_update_playhead(void){
    double playhead=hb_clip_playhead();
    if(g_bus.next_have_playhead&&playhead+1e-4<g_bus.next_last_playhead){
        hb_next_promote_learning();
    }
    g_bus.next_last_playhead=playhead;
    g_bus.next_have_playhead=1;
    hb_next_apply_effective(playhead);
}
static double hb_next_effective_boundary(double absolute_beat){
    if(!g_bus.next_predict||!g_bus.next_model_locked||g_bus.next_model_count<=0)return -1.0;
    double length=hb_next_loop_length();
    if(length<=0.0)return -1.0;
    double phase=hb_next_phase(hb_clip_playhead());
    double lookahead=hb_next_lookahead_beats();
    double best=1e99;
    for(int index=0;index<g_bus.next_model_count;index++){
        double shifted=hb_next_normalize_phase(g_bus.next_model[index].phase-lookahead);
        double distance=shifted-phase;
        if(distance<=1e-6)distance+=length;
        if(distance<best)best=distance;
    }
    return best<1e98?absolute_beat+best:-1.0;
}
'''
    if anchor not in source:
        raise SystemExit("bus_read anchor not found")
    source = source.replace(anchor, anchor + engine, 1)

# Follower Buffer must key from the SAME effective/shifted harmony boundary.
old_queue = '''        double target=hb_follower_capture_target(
            beat,
            hb_chord_grid_beats(),
            hb_anticipation_beats(),
            hb_quant_grid_beats(),
            capture);
        instance->follower_queue_target_beat[slot]=target;
'''
new_queue = '''        double target;
        double learned_boundary=hb_next_effective_boundary(beat);
        if(learned_boundary>=0.0){
            /* Prediction owns the harmonic boundary; Quant Grid remains an
               independent follower-only capture source. */
            target=hb_follower_capture_target(beat,0.0,0.0,hb_quant_grid_beats(),capture);
            double distance=learned_boundary-beat;
            if(distance>=-1e-6&&distance<=capture+1e-6&&
               (target<0.0||learned_boundary<target))target=learned_boundary;
        }else{
            target=hb_follower_capture_target(
                beat,
                hb_chord_grid_beats(),
                hb_anticipation_beats(),
                hb_quant_grid_beats(),
                capture);
        }
        instance->follower_queue_target_beat[slot]=target;
'''
if old_queue in source:
    source = source.replace(old_queue, new_queue, 1)
elif "learned_boundary=hb_next_effective_boundary" not in source:
    raise SystemExit("follower capture block not found")

# Keep the existing clip playhead path, add wrap/model update, and do not touch
# ahead-of-time clip note contents.
old_playhead_tick = '''            double playhead=hb_clip_playhead();
            g_bus.last_clip_playhead=playhead;
            g_bus.have_last_clip_playhead=1;
'''
new_playhead_tick = '''            double playhead=hb_clip_playhead();
            g_bus.last_clip_playhead=playhead;
            g_bus.have_last_clip_playhead=1;
            if(instance->role==0)hb_next_update_playhead();
'''
if old_playhead_tick in source:
    source = source.replace(old_playhead_tick, new_playhead_tick, 1)
elif "hb_next_update_playhead();" not in source:
    raise SystemExit("tick playhead block not found")

# When stopped, retain learned knowledge but stop claiming a shifted state.
source = source.replace(
    "            g_bus.have_last_clip_playhead=0;\n",
    "            g_bus.have_last_clip_playhead=0;g_bus.next_have_playhead=0;g_bus.next_shift_active=0;\n",
    1,
)

# Inference context must be based on what the conductor actually established,
# never on the predictor's shifted bus output.
source = source.replace(
    "    hb_harmony_t committed=bus_read();\n    hb_harmony_t committed_sensor=hb_transpose_harmony(committed,-g_bus.global_transpose);",
    "    hb_harmony_t committed=hb_observed_read();\n    hb_harmony_t committed_sensor=hb_transpose_harmony(committed,-g_bus.global_transpose);",
    1,
)

# Every conductor commit learns the actual event. Effective bus publication is
# handled centrally by hb_commit_observed_harmony / hb_next_apply_effective.
source = source.replace("bus_write(instance->candidate_harmony);", "hb_commit_observed_harmony(instance->candidate_harmony);")
source = source.replace("bus_write(candidate);", "hb_commit_observed_harmony(candidate);")

# Parameters and action.
if "NEXT_PREDICT_OPTS" not in source:
    source = source.replace(
        'static const char *ROLE_OPTS[]={"Conductor","Follower","Off"};',
        'static const char *NEXT_PREDICT_OPTS[]={"Off","On"};static const char *NEXT_LOOKAHEAD_OPTS[]={"Off","1/32","1/16","1/8","1/4","1/2","1 Bar"};static const char *ROLE_OPTS[]={"Conductor","Follower","Off"};',
        1,
    )

set_anchor = 'static void set_param(void *value,const char *key,const char *parameter){Inst *instance=(Inst*)value;if(!instance||!key||!parameter)return;if(!strcmp(key,"live_press"))'
set_replacement = '''static void set_param(void *value,const char *key,const char *parameter){Inst *instance=(Inst*)value;if(!instance||!key||!parameter)return;
if(!strcmp(key,"next_predict")){g_bus.next_predict=enum_index(parameter,NEXT_PREDICT_OPTS,2,g_bus.next_predict);if(!g_bus.next_predict){g_bus.next_shift_active=0;hb_effective_write(g_bus.observed_harmony);}else if(g_bus.next_model_locked)hb_next_apply_effective(hb_clip_playhead());return;}
if(!strcmp(key,"next_lookahead")){g_bus.next_lookahead=enum_index(parameter,NEXT_LOOKAHEAD_OPTS,7,g_bus.next_lookahead);if(g_bus.next_predict&&g_bus.next_model_locked)hb_next_apply_effective(hb_clip_playhead());return;}
if(!strcmp(key,"next_reset")){if(parameter[0]=='1'||!strcmp(parameter,"Reset")){hb_next_reset_knowledge();hb_effective_write(g_bus.observed_harmony);}return;}
if(!strcmp(key,"live_press"))'''
if set_anchor in source:
    source = source.replace(set_anchor, set_replacement, 1)
elif 'if(!strcmp(key,"next_predict"))' not in source:
    raise SystemExit("set_param anchor not found")

# Read-only panel status.  Position and length use the trusted existing clip
# loop/playhead path.  Next is the next *ordinary* learned harmony event.
get_anchor = 'static int get_param(void *value,const char *key,char *buffer,int length){Inst *instance=(Inst*)value;if(!instance||!key||!buffer||length<2)return -1;hb_harmony_t harmony=bus_read();'
get_replacement = r'''static int get_param(void *value,const char *key,char *buffer,int length){Inst *instance=(Inst*)value;if(!instance||!key||!buffer||length<2)return -1;hb_harmony_t harmony=bus_read();
if(!strcmp(key,"next_predict"))return snprintf(buffer,(size_t)length,"%s",NEXT_PREDICT_OPTS[g_bus.next_predict?1:0]);
if(!strcmp(key,"next_lookahead")){int index=g_bus.next_lookahead;if(index<0||index>6)index=3;return snprintf(buffer,(size_t)length,"%s",NEXT_LOOKAHEAD_OPTS[index]);}
if(!strcmp(key,"next_model"))return snprintf(buffer,(size_t)length,"%s",!g_bus.next_predict?"Off":(g_bus.next_model_locked?"Locked":"Learning"));
if(!strcmp(key,"next_shift"))return snprintf(buffer,(size_t)length,"%s",g_bus.next_shift_active?"Early":"Live");
if(!strcmp(key,"next_loop_length")){double beats=hb_next_loop_length();if(beats<=0.0)return snprintf(buffer,(size_t)length,"--");if(((long)(beats+0.5))%4==0&&beats>=4.0)return snprintf(buffer,(size_t)length,"%.2f Bars",beats/4.0);return snprintf(buffer,(size_t)length,"%.2f Beats",beats);}
if(!strcmp(key,"next_position")){double beats=hb_next_loop_length();if(beats<=0.0||!g_bus.have_last_clip_playhead)return snprintf(buffer,(size_t)length,"--");double phase=hb_next_phase(g_bus.last_clip_playhead);if(((long)(beats+0.5))%4==0&&beats>=4.0)return snprintf(buffer,(size_t)length,"%.2f / %.2f Bars",phase/4.0,beats/4.0);return snprintf(buffer,(size_t)length,"%.2f / %.2f Beats",phase,beats);}
if(!strcmp(key,"next_harmony")){if(!g_bus.next_model_locked||g_bus.next_model_count<=0)return snprintf(buffer,(size_t)length,"--");int index=hb_next_upcoming_event(hb_next_phase(hb_clip_playhead()));if(index<0)return snprintf(buffer,(size_t)length,"--");return hb_format_harmony(buffer,length,g_bus.next_model[index].harmony);}
if(!strcmp(key,"next_reset"))return snprintf(buffer,(size_t)length,"Off");'''
if get_anchor in source:
    source = source.replace(get_anchor, get_replacement, 1)
elif 'if(!strcmp(key,"next_model"))' not in source:
    raise SystemExit("get_param anchor not found")

DSP.write_text(source)
