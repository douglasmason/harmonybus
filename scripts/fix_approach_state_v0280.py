#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

module = json.loads(MODULE.read_text())
module["version"] = "0.2.80"
module["name"] = "Harmony Bus 0.2.80"
module["abbrev"] = "HB280"
module["description"] = "Harmony Bus v0.2.80 — neutral approach startup and reliable Scale Above"
MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = source.replace('/* Harmony Bus v0.2.79 — Schwung MIDI FX. */', '/* Harmony Bus v0.2.80 — Schwung MIDI FX. */')
source = source.replace('#define HB_VERSION "0.2.79"', '#define HB_VERSION "0.2.80"')

# The approach enum is 0=chrom below, 1=off, 2=scale above. Zero-filled global
# state therefore accidentally means Chrom Below. Make the global default neutral.
old = 'static SharedBus g_bus={0}; static int g_init=0;'
new = 'static SharedBus g_bus={.approach_control=1,.approach_mode=0}; static int g_init=0;'
if old in source:
    source = source.replace(old, new, 1)
elif new not in source:
    raise SystemExit("SharedBus initializer not found")

# Same problem existed per instance: zero-filled approach_pad_armed means a queued
# one-shot Chrom Below. Explicitly initialize it to the neutral sentinel (1).
old = 'instance->retrigger_held=0;instance->follow_lookahead_ms=0;instance->follower_queue_count=0;'
new = 'instance->retrigger_held=0;instance->follow_lookahead_ms=0;instance->approach_pad_armed=1;instance->approach_below_held=0;instance->approach_above_held=0;instance->follower_queue_count=0;'
if old in source:
    source = source.replace(old, new, 1)
elif new not in source:
    raise SystemExit("create_inst initialization block not found")

# Transport/role resets should clear a pending one-shot modifier but leave the
# persistent K3/K4 toggle (g_bus.approach_control) alone.
old = 'instance->active_count=0;\n    instance->last_inferred_count=0;\n    instance->candidate_frames=0;'
new = 'instance->active_count=0;\n    instance->last_inferred_count=0;\n    instance->approach_pad_armed=1;\n    instance->approach_below_held=0;\n    instance->approach_above_held=0;\n    instance->candidate_frames=0;'
if old in source:
    source = source.replace(old, new, 1)
elif new not in source:
    raise SystemExit("clear-note-state block not found")

# Scale Above means STRICTLY the next legal scale pitch above the normal rendered
# pitch. Prefer the follower parent/chord scale; if that cannot be resolved,
# fall back to the detected harmony's quality-derived scale rather than silently
# doing nothing.
old = '''    if(approach==2){
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
'''
new = '''    if(approach==2){
        hb_harmony_t detected=bus_read();
        if(!detected.valid)return mapped;
        hb_harmony_t scale_target=hb_follower_scale_target(instance,detected);
        uint16_t scale_mask=(uint16_t)(scale_target.pitch_mask&0x0FFFu);
        if(!scale_mask)scale_mask=(uint16_t)(hb_scale_mask(detected)&0x0FFFu);
        for(int semitones=1;semitones<=12;semitones++){
            int candidate=mapped+semitones;
            if(candidate>127)break;
            if(scale_mask&(1u<<mod12(candidate)))return candidate;
        }
    }
'''
if old in source:
    source = source.replace(old, new, 1)
elif new not in source:
    raise SystemExit("Scale Above implementation block not found")

DSP.write_text(source)
