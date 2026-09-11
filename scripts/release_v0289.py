#!/usr/bin/env python3
"""Publish v0.2.89: make learned lookahead use the trusted playhead path independently of clip-note sensing."""
from __future__ import annotations

import json
import re
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

# Keep every v0.2.88 lookahead normalization idempotently, then apply the
# transport/learning corrections below.
runpy.run_path(str(ROOT / "scripts/release_v0288.py"), run_name="__main__")

module = json.loads(MODULE.read_text())
module["version"] = "0.2.89"
module["name"] = "Harmony Bus 0.2.89"
module["abbrev"] = "HB289"
module["description"] = "Harmony Bus v0.2.89 — full-loop learned harmony lookahead"
module["capabilities"]["ui_hierarchy"]["levels"]["root"]["name"] = "Harmony Bus 0.2.89"
MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = source.replace('/* Harmony Bus v0.2.88 — Schwung MIDI FX. */', '/* Harmony Bus v0.2.89 — Schwung MIDI FX. */', 1)
source = source.replace('#define HB_VERSION "0.2.88"', '#define HB_VERSION "0.2.89"', 1)

# A model may only lock after observing a complete traversal from one clip
# playhead wrap to the next. This prevents enabling prediction from a partial
# loop when playback starts or Reset is pressed mid-loop.
old_fields = "double next_last_playhead; int next_have_playhead; hb_harmony_t observed_harmony;"
new_fields = "double next_last_playhead; int next_have_playhead; int next_learning_started; hb_harmony_t observed_harmony;"
if old_fields in source:
    source = source.replace(old_fields, new_fields, 1)
elif "next_learning_started" not in source:
    raise SystemExit("next learning field anchor not found")

old_init = "g_bus.next_learning_count=0;g_bus.next_model_count=0;g_bus.next_last_playhead=0.0;g_bus.next_have_playhead=0;memset(&g_bus.observed_harmony"
new_init = "g_bus.next_learning_count=0;g_bus.next_model_count=0;g_bus.next_last_playhead=0.0;g_bus.next_have_playhead=0;g_bus.next_learning_started=0;memset(&g_bus.observed_harmony"
if old_init in source:
    source = source.replace(old_init, new_init, 1)
elif "g_bus.next_learning_started=0" not in source:
    raise SystemExit("next learning init anchor not found")

source = source.replace(
    "    g_bus.next_have_playhead=0;\n    g_bus.next_last_playhead=0.0;\n}",
    "    g_bus.next_have_playhead=0;\n    g_bus.next_last_playhead=0.0;\n    g_bus.next_learning_started=0;\n}",
    1,
)

old_promote = '''static void hb_next_promote_learning(void){
    if(g_bus.next_learning_count<=0)return;
    g_bus.next_model_count=g_bus.next_learning_count;
    for(int index=0;index<g_bus.next_model_count;index++)g_bus.next_model[index]=g_bus.next_learning[index];
    g_bus.next_learning_count=0;
    g_bus.next_model_locked=1;
}
'''
new_promote = '''static void hb_next_promote_learning(void){
    /* First wrap establishes a trustworthy phase-zero. Discard any partial
       traversal accumulated before it; only the NEXT wrap closes a full loop. */
    if(!g_bus.next_learning_started){
        g_bus.next_learning_count=0;
        g_bus.next_learning_started=1;
        return;
    }
    /* A truly constant-harmony loop has no change events during the pass.
       Represent it explicitly so Model can still become Locked. */
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
}
'''
if old_promote in source:
    source = source.replace(old_promote, new_promote, 1)
elif "First wrap establishes a trustworthy phase-zero" not in source:
    raise SystemExit("next promote function anchor not found")

# v0.2.87 had tied playhead sampling to clip_context, which is now deliberately
# Realtime Only because clip NOTE contents were unreliable. The playhead/loop
# bounds themselves were good, so keep sampling them regardless of that flag.
source = source.replace(
    "    if(g_bus.clip_context&&g_host&&g_host->get_clock_status){\n",
    "    if(g_host&&g_host->get_clock_status){\n",
    1,
)

# Likewise, Realtime Only must stop clearing the metadata cache that contains
# the trusted loop start/end. We never use cached notes for conductor sensing.
source = source.replace(
    '}else if(!strcmp(key,"clip_context")){g_bus.clip_context=0;g_bus.sensor_sources=0;hb_clear_clip_cache();instance->dirty=1;instance->frames_since_change=0;}',
    '}else if(!strcmp(key,"clip_context")){g_bus.clip_context=0;g_bus.sensor_sources=0;instance->dirty=1;instance->frames_since_change=0;}',
    1,
)

# State restore used to discard the whole clip cache when clip contents were
# removed from sensing. Reload once instead so loop bounds/playhead remain
# available to Next Harm without reintroducing clip-note sensing.
source = source.replace(
    "    if(instance->role==0)hb_clear_clip_cache();\n}",
    "    if(instance->role==0){if(!hb_load_clip_cache())hb_clear_clip_cache();}\n}",
    1,
)

DSP.write_text(source)
