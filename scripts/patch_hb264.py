from pathlib import Path
import json

DSP = Path('modules/harmonybus/dsp/harmonybus.c')
text = DSP.read_text()

if '#define HB_VERSION "0.2.63"' not in text:
    raise SystemExit('HB264 patch expects HarmonyBus 0.2.63 source')

# ---------------------------------------------------------------------------
# Shared approach state. Values intentionally match the visible enum:
#   0 = Chrom Below, 1 = Off, 2 = Scale Above
# Mode: 0 = Next gesture, 1 = Held/gated.
# Pad state is per follower instance; knob state is global like the other
# follower mapping controls.
# ---------------------------------------------------------------------------
old = 'int follower_content_map; int follower_travel_map; int follower_scale; int follower_split_map; int inference_window_ms;'
new = 'int follower_content_map; int follower_travel_map; int follower_scale; int follower_split_map; int approach_control; int approach_mode; int inference_window_ms;'
if old not in text:
    raise SystemExit('SharedBus follower fields not found')
text = text.replace(old, new, 1)

old = 'int render_last_note; int retrigger_held; int follow_lookahead_ms; int follower_queue_count;'
new = 'int render_last_note; int retrigger_held; int follow_lookahead_ms; int approach_pad_armed; uint8_t approach_below_held; uint8_t approach_above_held; int follower_queue_count;'
if old not in text:
    raise SystemExit('Inst render fields not found')
text = text.replace(old, new, 1)

old = 'g_bus.follower_content_map=0;g_bus.follower_travel_map=0;g_bus.follower_scale=0;g_bus.inference_window_ms=25;'
new = 'g_bus.follower_content_map=0;g_bus.follower_travel_map=0;g_bus.follower_scale=0;g_bus.approach_control=1;g_bus.approach_mode=0;g_bus.inference_window_ms=25;'
if old not in text:
    raise SystemExit('ensure_init follower defaults not found')
text = text.replace(old, new, 1)

old = 'for(int index=0;index<HB_MAX_INSTANCES;index++){memset(&g_pool[index],0,sizeof(g_pool[index]));for(int note=0;note<128;note++)g_pool[index].mapped[note]=-1;}g_init=1;'
new = 'for(int index=0;index<HB_MAX_INSTANCES;index++){memset(&g_pool[index],0,sizeof(g_pool[index]));g_pool[index].approach_pad_armed=1;for(int note=0;note<128;note++)g_pool[index].mapped[note]=-1;}g_init=1;'
if old not in text:
    raise SystemExit('pool initialization not found')
text = text.replace(old, new, 1)

# ---------------------------------------------------------------------------
# Render-stage modifier. This intentionally runs AFTER normal follower mapping.
# Chrom Below is an exact semitone. Scale Above searches the active follower
# scale and never feeds the decoration back into harmony/content selection.
# ---------------------------------------------------------------------------
anchor = 'static int hb_conductor_pending_for_follow(void){'
helper = r'''static int hb_active_approach(const Inst *instance){
    if(!instance)return 1;
    if(g_bus.approach_mode==0){
        if(instance->approach_pad_armed!=1)return instance->approach_pad_armed;
        return g_bus.approach_control;
    }
    if(instance->approach_below_held)return 0;
    if(instance->approach_above_held)return 2;
    return g_bus.approach_control;
}
static int hb_apply_approach(Inst *instance,int mapped,int approach){
    if(approach==0){
        return mapped>0?mapped-1:0;
    }
    if(approach==2){
        hb_harmony_t detected=bus_read();
        if(!detected.valid)return mapped;
        hb_harmony_t scale_target=hb_follower_scale_target(instance,detected);
        uint16_t scale_mask=(uint16_t)(scale_target.pitch_mask&0x0FFFu);
        if(!scale_mask)return mapped;
        for(int semitones=1;semitones<=12;semitones++){
            int candidate=mapped+semitones;
            if(candidate>127)break;
            if(scale_mask&(1u<<mod12(candidate)))return candidate;
        }
    }
    return mapped;
}
static void hb_consume_next_approach(Inst *instance){
    if(!instance||g_bus.approach_mode!=0)return;
    instance->approach_pad_armed=1;
    /* The knob is another way to arm exactly the same one-shot state. */
    g_bus.approach_control=1;
}
'''
if anchor not in text:
    raise SystemExit('follower release anchor not found')
text = text.replace(anchor, helper + anchor, 1)

old = '    int emitted=0;\n    for(int index=0;index<instance->follower_queue_count;index++){'
new = '''    int emitted=0;
    int next_approach=(g_bus.approach_mode==0)?hb_active_approach(instance):1;
    int next_approach_used=0;
    double next_arrival=-1.0;
    for(int index=0;index<instance->follower_queue_count;index++){'''
if old not in text:
    raise SystemExit('follower release loop start not found')
text = text.replace(old, new, 1)

old = '''        if(is_on){
            mapped=hb_map_follower_note_now(instance,source_note);
            instance->mapped[source_note]=mapped;'''
new = '''        if(is_on){
            mapped=hb_map_follower_note_now(instance,source_note);
            int approach=(g_bus.approach_mode==0)?next_approach:hb_active_approach(instance);
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
            instance->mapped[source_note]=mapped;'''
if old not in text:
    raise SystemExit('follower note-on mapping block not found')
text = text.replace(old, new, 1)

old = '    instance->follower_queue_count=write;\n    return emitted;\n}'
new = '    instance->follower_queue_count=write;\n    if(next_approach_used)hb_consume_next_approach(instance);\n    return emitted;\n}'
if old not in text:
    raise SystemExit('follower release return not found')
text = text.replace(old, new, 1)

# ---------------------------------------------------------------------------
# Parameters. Pad params are deliberately momentary hooks; physical Move-pad
# identity belongs to the Schwung UI/host layer rather than the musical MIDI
# stream, so the host can bind upper-left/right without stealing note pitches.
# ---------------------------------------------------------------------------
old = 'static const char *ROLE_OPTS[]={"Conductor","Follower","Off"};static const char *RETRIGGER_OPTS[]={"Off","On"};'
new = 'static const char *ROLE_OPTS[]={"Conductor","Follower","Off"};static const char *RETRIGGER_OPTS[]={"Off","On"};static const char *APPROACH_OPTS[]={"Chrom Below","Off","Scale Above"};static const char *APPROACH_MODE_OPTS[]={"Next","Held"};'
if old not in text:
    raise SystemExit('parameter option declarations not found')
text = text.replace(old, new, 1)

old = 'static void set_param(void *value,const char *key,const char *parameter){Inst *instance=(Inst*)value;if(!instance||!key||!parameter)return;if(!strcmp(key,"live_press")){if(parameter[0]==\'1\')hb_receive_live_vouch(instance);return;}if(!strcmp(key,"track_role")||!strcmp(key,"role")){'
new = '''static void set_param(void *value,const char *key,const char *parameter){Inst *instance=(Inst*)value;if(!instance||!key||!parameter)return;if(!strcmp(key,"live_press")){if(parameter[0]=='1')hb_receive_live_vouch(instance);return;}if(!strcmp(key,"approach_below_pad")){int down=parameter[0]=='1';if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;}else instance->approach_below_held=(uint8_t)down;return;}if(!strcmp(key,"approach_above_pad")){int down=parameter[0]=='1';if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;}else instance->approach_above_held=(uint8_t)down;return;}if(!strcmp(key,"track_role")||!strcmp(key,"role")){'''
if old not in text:
    raise SystemExit('set_param preamble not found')
text = text.replace(old, new, 1)

old = '}else if(!strcmp(key,"travel_map")){static const char *opts[]={"Relative","Closest","Upward","Closest Split","Downward"};hb_set_global_travel_map(enum_index(parameter,opts,5,hb_global_travel_map()));}else if(!strcmp(key,"split_map")){static const char *opts[]={"Harm. / Out","135 / 2467","1357 / 246","Act. / Out"};g_bus.follower_split_map=enum_index(parameter,opts,4,g_bus.follower_split_map);}else if(!strcmp(key,"map_target"))'
new = '}else if(!strcmp(key,"travel_map")){static const char *opts[]={"Relative","Closest","Upward","Closest Split","Downward"};hb_set_global_travel_map(enum_index(parameter,opts,5,hb_global_travel_map()));}else if(!strcmp(key,"split_map")){static const char *opts[]={"Harm. / Out","135 / 2467","1357 / 246","Act. / Out"};g_bus.follower_split_map=enum_index(parameter,opts,4,g_bus.follower_split_map);}else if(!strcmp(key,"approach")){g_bus.approach_control=enum_index(parameter,APPROACH_OPTS,3,g_bus.approach_control);}else if(!strcmp(key,"approach_mode")){g_bus.approach_mode=enum_index(parameter,APPROACH_MODE_OPTS,2,g_bus.approach_mode);instance->approach_pad_armed=1;instance->approach_below_held=0;instance->approach_above_held=0;}else if(!strcmp(key,"map_target"))'
if old not in text:
    raise SystemExit('set_param follower mapping chain not found')
text = text.replace(old, new, 1)

old = 'static int get_param(void *value,const char *key,char *buffer,int length){Inst *instance=(Inst*)value;if(!instance||!key||!buffer||length<2)return -1;hb_harmony_t harmony=bus_read();if(!strcmp(key,"version"))return snprintf(buffer,(size_t)length,"%s",HB_VERSION);if(!strcmp(key,"retrigger_held"))'
new = 'static int get_param(void *value,const char *key,char *buffer,int length){Inst *instance=(Inst*)value;if(!instance||!key||!buffer||length<2)return -1;hb_harmony_t harmony=bus_read();if(!strcmp(key,"version"))return snprintf(buffer,(size_t)length,"%s",HB_VERSION);if(!strcmp(key,"approach"))return snprintf(buffer,(size_t)length,"%s",APPROACH_OPTS[(g_bus.approach_control>=0&&g_bus.approach_control<3)?g_bus.approach_control:1]);if(!strcmp(key,"approach_mode"))return snprintf(buffer,(size_t)length,"%s",APPROACH_MODE_OPTS[g_bus.approach_mode?1:0]);if(!strcmp(key,"approach_below_pad"))return snprintf(buffer,(size_t)length,"%d",instance->approach_below_held?1:0);if(!strcmp(key,"approach_above_pad"))return snprintf(buffer,(size_t)length,"%d",instance->approach_above_held?1:0);if(!strcmp(key,"retrigger_held"))'
if old not in text:
    raise SystemExit('get_param preamble not found')
text = text.replace(old, new, 1)

text = text.replace('0.2.63', '0.2.64').replace('HB263', 'HB264')
DSP.write_text(text)

# ---------------------------------------------------------------------------
# UI metadata. Put Approach + Approach Mode directly on the Foll Root knob page.
# The two pad hooks are chain params but intentionally not ordinary knobs.
# ---------------------------------------------------------------------------
module_path = Path('modules/harmonybus/module.json')
module = json.loads(module_path.read_text())
levels = module['capabilities']['ui_hierarchy']['levels']
follower = levels['follower_source']

params = follower['params']
insert_at = next((i for i, value in enumerate(params) if value.get('key') == 'retrigger_held'), len(params))
params[insert_at:insert_at] = [
    {
        'key': 'approach',
        'name': 'Approach',
        'type': 'enum',
        'options': ['Chrom Below', 'Off', 'Scale Above'],
        'options_as_string': True,
        'default': 'Off',
    },
    {
        'key': 'approach_mode',
        'name': 'Approach Mode',
        'type': 'enum',
        'options': ['Next', 'Held'],
        'options_as_string': True,
        'default': 'Next',
    },
]
follower['knobs'] = [
    'content_map',
    'follower_scale',
    'travel_map',
    'retrigger_held',
    'approach',
    'approach_mode',
    'follower_root_policy',
    'follower_explicit_root',
]

chain_params = module['capabilities'].setdefault('chain_params', [])
known_keys = {value.get('key') for value in chain_params if isinstance(value, dict)}
for key, name in [('approach_below_pad', 'Approach Below Pad'), ('approach_above_pad', 'Approach Above Pad')]:
    if key not in known_keys:
        chain_params.append({'key': key, 'name': name, 'type': 'int', 'min': 0, 'max': 1, 'step': 1})

module['name'] = 'Harmony Bus 0.2.64'
module['version'] = '0.2.64'
module['abbrev'] = 'HB264'
module['description'] = 'Harmony Bus v0.2.64 — one-shot/held chromatic-below and scale-above follower approach modifiers'
levels['root']['name'] = 'Harmony Bus 0.2.64'
module_path.write_text(json.dumps(module, indent=2) + '\n')

for name in ['scripts/build_harmonybus_move.sh', 'release.json']:
    path = Path(name)
    value = path.read_text().replace('0.2.63', '0.2.64').replace('HB263', 'HB264')
    path.write_text(value)
