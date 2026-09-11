#!/usr/bin/env python3
"""Publish v0.2.90: robust full-duration learned harmony lookahead."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

# Main is already on the v0.2.89 implementation. Transform that source
# directly instead of replaying historical normalizers, which are intentionally
# tied to older source shapes and can duplicate/corrupt the getter section.
module = json.loads(MODULE.read_text())
module["version"] = "0.2.90"
module["name"] = "Harmony Bus 0.2.90"
module["abbrev"] = "HB290"
module["description"] = "Harmony Bus v0.2.90 — robust full-duration learned harmony lookahead"
levels = module["capabilities"]["ui_hierarchy"]["levels"]
levels["root"]["name"] = "Harmony Bus 0.2.90"
module["capabilities"]["chain_params"] = [p for p in module["capabilities"]["chain_params"] if p.get("key") != "next_predict"]
next_panel = levels["next_harm"]
next_panel["params"] = [p for p in next_panel["params"] if p.get("key") != "next_predict"]
next_panel["knobs"] = ["next_lookahead", "next_model", "next_shift", "next_loop_length", "next_position", "next_harmony", "next_reset"]
MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = re.sub(r'/\* Harmony Bus v0\.2\.\d+ — Schwung MIDI FX\. \*/', '/* Harmony Bus v0.2.90 — Schwung MIDI FX. */', source, count=1)
source = re.sub(r'#define HB_VERSION "0\.2\.\d+"', '#define HB_VERSION "0.2.90"', source, count=1)

# Canonicalize the beginning of get_param. v0.2.89 currently contains repeated
# Next Harm getter blocks from earlier idempotence mistakes. Remove only the
# region between the harmony declaration and the ordinary Version getter, then
# insert one canonical block.
get_start = source.find('static int get_param(')
if get_start < 0:
    raise SystemExit('get_param function not found')
harmony_decl_text = 'hb_harmony_t harmony=bus_read();'
harmony_decl = source.find(harmony_decl_text, get_start)
if harmony_decl < 0:
    raise SystemExit('get_param harmony declaration not found')
version_marker = source.find('if(!strcmp(key,"version"))', harmony_decl)
if version_marker < 0:
    raise SystemExit('get_param version marker not found')
insert_at = harmony_decl + len(harmony_decl_text)
get_block = '''\nif(!strcmp(key,"next_lookahead")){int index=g_bus.next_lookahead;if(index<0||index>6)index=3;return snprintf(buffer,(size_t)length,"%s",NEXT_LOOKAHEAD_OPTS[index]);}
if(!strcmp(key,"next_model"))return snprintf(buffer,(size_t)length,"%s",g_bus.next_model_locked?"Locked":"Learning");
if(!strcmp(key,"next_shift"))return snprintf(buffer,(size_t)length,"%s",g_bus.next_shift_active?"Early":"Live");
if(!strcmp(key,"next_loop_length")){double beats=hb_next_loop_length();if(beats<=0.0)return snprintf(buffer,(size_t)length,"--");if(((long)(beats+0.5))%4==0&&beats>=4.0)return snprintf(buffer,(size_t)length,"%.2f Bars",beats/4.0);return snprintf(buffer,(size_t)length,"%.2f Beats",beats);}
if(!strcmp(key,"next_position")){double beats=hb_next_loop_length();if(beats<=0.0||!g_bus.have_last_clip_playhead)return snprintf(buffer,(size_t)length,"--");double phase=hb_next_phase(g_bus.last_clip_playhead);if(((long)(beats+0.5))%4==0&&beats>=4.0)return snprintf(buffer,(size_t)length,"%.2f / %.2f Bars",phase/4.0,beats/4.0);return snprintf(buffer,(size_t)length,"%.2f / %.2f Beats",phase,beats);}
if(!strcmp(key,"next_harmony")){if(!g_bus.next_model_locked||g_bus.next_model_count<=0)return snprintf(buffer,(size_t)length,"--");double playhead=g_bus.have_last_clip_playhead?g_bus.last_clip_playhead:hb_clip_playhead();int index=hb_next_upcoming_event(hb_next_phase(playhead));if(index<0)return snprintf(buffer,(size_t)length,"--");return hb_format_harmony(buffer,length,g_bus.next_model[index].harmony);}
if(!strcmp(key,"next_reset"))return snprintf(buffer,(size_t)length,"Off");'''
source = source[:insert_at] + get_block + source[version_marker:]

# Prediction is permanently armed; Lookahead=Off is the bypass.
source = source.replace('g_bus.next_predict=0;g_bus.next_lookahead=3;', 'g_bus.next_predict=1;g_bus.next_lookahead=3;', 1)
source = source.replace('if(!g_bus.next_predict||!g_bus.next_model_locked)hb_effective_write(harmony);', 'if(hb_next_lookahead_beats()<=0.0||!g_bus.next_model_locked)hb_effective_write(harmony);', 1)
source = source.replace('if(!g_bus.next_predict||!g_bus.next_model_locked||g_bus.next_model_count<=0){', 'if(hb_next_lookahead_beats()<=0.0||!g_bus.next_model_locked||g_bus.next_model_count<=0){', 1)
source = source.replace('if(!g_bus.next_predict||!g_bus.next_model_locked||g_bus.next_model_count<=0)return -1.0;', 'if(hb_next_lookahead_beats()<=0.0||!g_bus.next_model_locked||g_bus.next_model_count<=0)return -1.0;', 1)

old_fields = 'double next_last_playhead; int next_have_playhead; int next_learning_started; hb_harmony_t observed_harmony;'
new_fields = 'double next_last_playhead; int next_have_playhead; int next_learning_started; double next_learning_progress_beats; hb_harmony_t observed_harmony;'
if old_fields in source:
    source = source.replace(old_fields, new_fields, 1)
elif 'next_learning_progress_beats' not in source:
    raise SystemExit('learning progress field anchor not found')
source = source.replace('g_bus.next_have_playhead=0;g_bus.next_learning_started=0;memset(&g_bus.observed_harmony', 'g_bus.next_have_playhead=0;g_bus.next_learning_started=0;g_bus.next_learning_progress_beats=0.0;memset(&g_bus.observed_harmony', 1)
source = source.replace('    g_bus.next_learning_started=0;\n}', '    g_bus.next_learning_started=0;\n    g_bus.next_learning_progress_beats=0.0;\n}', 1)

promote_re = r'static void hb_next_promote_learning\(void\)\{.*?\n\}'
promote_new = '''static void hb_next_promote_learning(void){
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
if re.search(r'static void hb_next_promote_learning\(void\)', source):
    source = re.sub(promote_re, promote_new, source, count=1, flags=re.S)
else:
    raise SystemExit('hb_next_promote_learning not found')

update_new = '''static void hb_next_update_playhead(int frames,int sample_rate){
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
            while(g_bus.next_learning_progress_beats>=length)g_bus.next_learning_progress_beats-=length;
        }
    }
    hb_next_apply_effective(playhead);
}'''
if 'static void hb_next_update_playhead(void)' in source:
    source = re.sub(r'static void hb_next_update_playhead\(void\)\{.*?\n\}', update_new, source, count=1, flags=re.S)
elif 'static void hb_next_update_playhead(int frames,int sample_rate)' not in source:
    raise SystemExit('hb_next_update_playhead not found')

# Collapse duplicated v0.2.89 calls and preserve the last sampled position on stop.
source = re.sub(r'(?:\s*if\(instance->role==0\)hb_next_update_playhead\(\);)+', '\n            if(instance->role==0)hb_next_update_playhead(frames,sample_rate);', source)
source = source.replace('g_bus.have_last_clip_playhead=0;g_bus.next_have_playhead=0;g_bus.next_shift_active=0;', 'g_bus.next_have_playhead=0;g_bus.next_shift_active=0;')

DSP.write_text(source)
