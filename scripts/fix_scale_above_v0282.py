#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

module = json.loads(MODULE.read_text())
module["version"] = "0.2.82"
module["name"] = "Harmony Bus 0.2.82"
module["abbrev"] = "HB282"
module["description"] = "Harmony Bus v0.2.82 — reliable scale-above rendered-note modifier"
levels = module["capabilities"]["ui_hierarchy"]["levels"]
levels["root"]["name"] = "Harmony Bus 0.2.82"
MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = source.replace('/* Harmony Bus v0.2.81 — Schwung MIDI FX. */', '/* Harmony Bus v0.2.82 — Schwung MIDI FX. */')
source = source.replace('#define HB_VERSION "0.2.81"', '#define HB_VERSION "0.2.82"')

include_line = '#include "../../../src/approach_pitch.h"\n'
if include_line not in source:
    anchor = '#include "../../../src/approach_state.h"\n'
    if anchor not in source:
        raise SystemExit("approach_state include not found")
    source = source.replace(anchor, anchor + include_line, 1)

old = '''    if(approach==2){
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
new = '''    if(approach==HB_APPROACH_SCALE_ABOVE){
        hb_harmony_t detected=bus_read();
        if(!detected.valid)return mapped;

        /* Scale Above is a post-render ornament: take the NORMAL rendered
           pitch and move it to the next strictly higher pitch in the follower
           parent scale.  Do not route this through hb_follower_scale_target(),
           which is designed for follower mapping and can collapse/rotate the
           target in ways that are inappropriate for a one-note ornament. */
        int source_root=0;
        if(!hb_resolve_follower_reference_root(instance,&source_root))
            source_root=detected.root_pc;
        int parent_scale_index=hb_parent_scale_index(instance,detected);
        uint16_t scale_mask=hb_explicit_scale_mask(source_root,parent_scale_index);
        if(!scale_mask)scale_mask=(uint16_t)(hb_scale_mask(detected)&0x0FFFu);
        return hb_next_scale_pitch_above(mapped,(unsigned int)scale_mask);
    }
'''
if old in source:
    source = source.replace(old, new, 1)
elif 'return hb_next_scale_pitch_above(mapped,(unsigned int)scale_mask);' not in source:
    raise SystemExit("Scale Above block not found and new implementation absent")

DSP.write_text(source)
