#!/usr/bin/env python3
"""Publish v0.2.91: invalidate learned harmony on committed contradiction."""
from __future__ import annotations

import json
import re
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

module = json.loads(MODULE.read_text())
if module.get("version") != "0.2.91":
    if module.get("version") != "0.2.90":
        runpy.run_path(str(ROOT / "scripts/release_v0290.py"), run_name="__main__")
    module = json.loads(MODULE.read_text())
    module["version"] = "0.2.91"
    module["name"] = "Harmony Bus 0.2.91"
    module["abbrev"] = "HB291"
    module["description"] = "Harmony Bus v0.2.91 — adaptive learned harmony lookahead"
    module["capabilities"]["ui_hierarchy"]["levels"]["root"]["name"] = "Harmony Bus 0.2.91"
    MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = re.sub(r'/\* Harmony Bus v0\.2\.\d+ — Schwung MIDI FX\. \*/',
                '/* Harmony Bus v0.2.91 — Schwung MIDI FX. */', source, count=1)
source = re.sub(r'#define HB_VERSION "0\.2\.\d+"', '#define HB_VERSION "0.2.91"', source, count=1)

old_commit = '''static void hb_commit_observed_harmony(hb_harmony_t harmony){
    hb_next_record_observed(harmony);
    if(hb_next_lookahead_beats()<=0.0||!g_bus.next_model_locked)hb_effective_write(harmony);
}
'''
new_commit = '''static int hb_next_model_accepts_observed(hb_harmony_t harmony,double phase){
    if(!g_bus.next_model_locked||g_bus.next_model_count<=0||!harmony.valid)return 1;
    double length=hb_next_loop_length();
    if(length<=0.0)return 1;

    /* Compare against the UNSHIFTED learned schedule. Lookahead is an output
       transform and must never make the predictor disagree with itself. */
    int current=-1;double best_age=1e99;
    for(int index=0;index<g_bus.next_model_count;index++){
        double age=phase-g_bus.next_model[index].phase;
        if(age<0.0)age+=length;
        if(age<best_age){best_age=age;current=index;}
    }
    if(current>=0&&hb_harmony_equal_effective(g_bus.next_model[current].harmony,harmony))return 1;

    /* With Free chord timing, realtime inference can commit a few milliseconds
       before the learned transition. Accept the immediately upcoming learned
       harmony within a small musical tolerance instead of falsely relearning. */
    double tolerance=hb_chord_grid_beats()>0.0?1e-4:0.125;
    for(int index=0;index<g_bus.next_model_count;index++){
        double distance=g_bus.next_model[index].phase-phase;
        if(distance<0.0)distance+=length;
        if(distance<=tolerance+1e-6&&
           hb_harmony_equal_effective(g_bus.next_model[index].harmony,harmony))return 1;
    }
    return 0;
}
static void hb_next_begin_relearning(void){
    /* Preserve clip bounds/playhead, but immediately stop predictive shifting
       and throw away stale musical knowledge. The contradicting committed
       harmony becomes the first event of the new learning pass. */
    g_bus.next_model_locked=0;
    g_bus.next_shift_active=0;
    g_bus.next_model_count=0;
    g_bus.next_learning_count=0;
    g_bus.next_learning_progress_beats=0.0;
    g_bus.next_learning_started=0;
}
static void hb_commit_observed_harmony(hb_harmony_t harmony){
    double phase=hb_next_record_phase();
    if(g_bus.next_model_locked&&!hb_next_model_accepts_observed(harmony,phase)){
        hb_next_begin_relearning();
        hb_effective_write(harmony);
    }
    hb_next_record_observed(harmony);
    if(hb_next_lookahead_beats()<=0.0||!g_bus.next_model_locked)hb_effective_write(harmony);
}
'''
if old_commit in source:
    source = source.replace(old_commit, new_commit, 1)
elif 'static int hb_next_model_accepts_observed' not in source:
    raise SystemExit('hb_commit_observed_harmony anchor not found')

DSP.write_text(source)
