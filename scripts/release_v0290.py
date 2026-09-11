#!/usr/bin/env python3
"""Publish v0.2.90: make loop learning duration-based and prediction always available.

The trusted clip playhead remains the phase source for recording/display. The
model no longer depends on detecting a playhead wrap: one full loop LENGTH of
continuous playback is sufficient to cover every clip phase, even when learning
starts in the middle of a loop. Lookahead=Off is the operational bypass.
"""
from __future__ import annotations

import json
import re
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

runpy.run_path(str(ROOT / "scripts/release_v0289.py"), run_name="__main__")

module = json.loads(MODULE.read_text())
module["version"] = "0.2.90"
module["name"] = "Harmony Bus 0.2.90"
module["abbrev"] = "HB290"
module["description"] = "Harmony Bus v0.2.90 — robust full-duration learned harmony lookahead"
levels = module["capabilities"]["ui_hierarchy"]["levels"]
levels["root"]["name"] = "Harmony Bus 0.2.90"
module["capabilities"]["chain_params"] = [
    param for param in module["capabilities"]["chain_params"]
    if param.get("key") != "next_predict"
]
next_panel = levels["next_harm"]
next_panel["params"] = [param for param in next_panel["params"] if param.get("key") != "next_predict"]
next_panel["knobs"] = [
    "next_lookahead", "next_model", "next_shift", "next_loop_length",
    "next_position", "next_harmony", "next_reset"
]
MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = re.sub(r'/\* Harmony Bus v0\.2\.\d+ — Schwung MIDI FX\. \*/',
                '/* Harmony Bus v0.2.90 — Schwung MIDI FX. */', source, count=1)
source = re.sub(r'#define HB_VERSION "0\.2\.\d+"', '#define HB_VERSION "0.2.90"', source, count=1)

# Remove every repeated Next Harm getter block left by older non-idempotent
# normalizers, then insert one canonical block immediately after get_param's
# local harmony declaration.
next_get_pattern = re.compile(
    r'if\(!strcmp\(key,"next_predict"\)\).*?if\(!strcmp\(key,"next_reset"\)\)return snprintf\(buffer,\(size_t\)length,"Off"\);',
    re.S,
)
source = next_get_pattern.sub("", source)
get_block = r'''if(!strcmp(key,"next_lookahead")){int index=g_bus.next_lookahead;if(index<0||index>6)index=3;return snprintf(buffer,(size_t)length,"%s",NEXT_LOOKAHEAD_OPTS[index]);}
if(!strcmp(key,"next_model"))return snprintf(buffer,(size_t)length,"%s",g_bus.next_model_locked?"Locked":"Learning");
if(!strcmp(key,"next_shift"))return snprintf(buffer,(size_t)length,"%s",g_bus.next_shift_active?"Early":"Live");
if(!strcmp(key,"next_loop_length")){double beats=hb_next_loop_length();if(beats<=0.0)return snprintf(buffer,(size_t)length,"--");if(((long)(beats+0.5))%4==0&&beats>=4.0)return snprintf(buffer,(size_t)length,"%.2f Bars",beats/4.0);return snprintf(buffer,(size_t)length,"%.2f Beats",beats);}
if(!strcmp(key,"next_position")){double beats=hb_next_loop_length();if(beats<=0.0||!g_bus.have_last_clip_playhead)return snprintf(buffer,(size_t)length,"--");double phase=hb_next_phase(g_bus.last_clip_playhead);if(((long)(beats+0.5))%4==0&&beats>=4.0)return snprintf(buffer,(size_t)length,"%.2f / %.2f Bars",phase/4.0,beats/4.0);return snprintf(buffer,(size_t)length,"%.2f / %.2f Beats",phase,beats);}
if(!strcmp(key,"next_harmony")){if(!g_bus.next_model_locked||g_bus.next_model_count<=0)return snprintf(buffer,(size_t)length,"--");double playhead=g_bus.have_last_clip_playhead?g_bus.last_clip_playhead:hb_clip_playhead();int index=hb_next_upcoming_event(hb_next_phase(playhead));if(index<0)return snprintf(buffer,(size_t)length,"--");return hb_format_harmony(buffer,length,g_bus.next_model[index].harmony);}
if(!strcmp(key,"next_reset"))return snprintf(buffer,(size_t)length,"Off");
'''
get_pattern = re.compile(
    r'(static int get_param\(void \*value,const char \*key,char \*buffer,int length\)\{Inst \*instance=\(Inst\*\)value;if\(!instance\|\|!key\|\|!buffer\|\|length<2\)return -1;hb_harmony_t harmony=bus_read\(\);)'
)
source, count = get_pattern.subn(r'\1\n' + get_block, source, count=1)
if count != 1:
    raise SystemExit("get_param insertion point not found")

source = source.replace('g_bus.next_predict=0;g_bus.next_lookahead=3;',
                        'g_bus.next_predict=1;g_bus.next_lookahead=3;', 1)
source = source.replace('if(!g_bus.next_predict||!g_bus.next_model_locked)hb_effective_write(harmony);',
                        'if(hb_next_lookahead_beats()<=0.0||!g_bus.next_model_locked)hb_effective_write(harmony);', 1)
source = source.replace('if(!g_bus.next_predict||!g_bus.next_model_locked||g_bus.next_model_count<=0){',
                        'if(hb_next_lookahead_beats()<=0.0||!g_bus.next_model_locked||g_bus.next_model_count<=0){', 1)
source = source.replace('if(!g_bus.next_predict||!g_bus.next_model_locked||g_bus.next_model_count<=0)return -1.0;',
                        'if(hb_next_lookahead_beats()<=0.0||!g_bus.next_model_locked||g_bus.next_model_count<=0)return -1.0;', 1)

old_fields = 'double next_last_playhead; int next_have_playhead; int next_learning_started; hb_harmony_t observed_harmony;'
new_fields = ('double next_last_playhead; int next_have_playhead; int next_learning_started; '
              'double next_learning_progress_beats; hb_harmony_t observed_harmony;')
if old_fields in source:
    source = source.replace(old_fields, new_fields, 1)
elif 'next_learning_progress_beats' not in source:
    raise SystemExit("learning progress field anchor not found")
source = source.replace('g_bus.next_have_playhead=0;g_bus.next_learning_started=0;memset(&g_bus.observed_harmony',
                        'g_bus.next_have_playhead=0;g_bus.next_learning_started=0;g_bus.next_learning_progress_beats=0.0;memset(&g_bus.observed_harmony', 1)
source = source.replace('    g_bus.next_learning_started=0;\n}',
                        '    g_bus.next_learning_started=0;\n    g_bus.next_learning_progress_beats=0.0;\n}', 1)

promote_pattern = re.compile(r'static void hb_next_promote_learning\(void\)\{.*?\n\}', re.S)
promote_new = r'''static void hb_next_promote_learning(void){
    /* A full loop LENGTH of observation covers every circular clip phase even
       when learning started mid-loop. No explicit playhead-wrap event needed. */
    if(g_bus.next_learning_count<=0&&g_bus.observed_harmony.valid){
        g_bus.next_learning[0].phase=0.0;
        g_bus.next_learning[0].harmony=g_bus.observed_harmony;
        g_bus.next_learning_count=1;
    }
    if(g_bus.next_learning_count<=0)return;
    g_bus.next_model_count=g_bus.next_learning_count;
    for(int index=0;index<g_bus.next_model_count;index++)g_bus.next_model[index]=g_bus.next_learning[index];
    g_bus.next_learning_count=0;
    g_bus.next_model_locked=1;
    g_bus.next_learning_started=1;
}'''
source, count = promote_pattern.subn(promote_new, source, count=1)
if count != 1:
    raise SystemExit("hb_next_promote_learning not found")

update_pattern = re.compile(r'static void hb_next_update_playhead\(void\)\{.*?\n\}', re.S)
update_new = r'''static void hb_next_update_playhead(int frames,int sample_rate){
    double playhead=hb_clip_playhead();
    g_bus.next_last_playhead=playhead;
    g_bus.next_have_playhead=1;
    double length=hb_next_loop_length();
    if(length>0.0&&frames>0&&sample_rate>0){
        double bpm=(g_host&&g_host->get_bpm)?g_host->get_bpm():120.0;
        if(bpm<=0.0)bpm=120.0;
        g_bus.next_learning_progress_beats += ((double)frames*bpm)/(60.0*(double)sample_rate);
        if(g_bus.next_learning_progress_beats+1e-6>=length){
            hb_next_promote_learning();
            while(g_bus.next_learning_progress_beats>=length)
                g_bus.next_learning_progress_beats-=length;
        }
    }
    hb_next_apply_effective(playhead);
}'''
source, count = update_pattern.subn(update_new, source, count=1)
if count != 1:
    raise SystemExit("hb_next_update_playhead not found")

source = re.sub(r'(?:\s*if\(instance->role==0\)hb_next_update_playhead\(\);)+',
                '\n            if(instance->role==0)hb_next_update_playhead(frames,sample_rate);', source)
source = source.replace(
    'g_bus.have_last_clip_playhead=0;g_bus.next_have_playhead=0;g_bus.next_shift_active=0;',
    'g_bus.next_have_playhead=0;g_bus.next_shift_active=0;',
)
source = re.sub(
    r'if\(!strcmp\(key,"next_predict"\)\)\{[^}]*\}',
    'if(!strcmp(key,"next_predict")){g_bus.next_predict=1;return;}',
    source,
)
DSP.write_text(source)
