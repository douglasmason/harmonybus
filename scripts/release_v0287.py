#!/usr/bin/env python3
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

module = json.loads(MODULE.read_text())
module["version"] = "0.2.87"
module["name"] = "Harmony Bus 0.2.87"
module["abbrev"] = "HB287"
module["description"] = "Harmony Bus v0.2.87 — proximity-first Closest Split"
module["capabilities"]["ui_hierarchy"]["levels"]["root"]["name"] = "Harmony Bus 0.2.87"

# Preserve the v0.2.86 K2 Follower Travel shortcut idempotently.
panel = module["capabilities"]["ui_hierarchy"]["levels"]["follower_mod"]
panel["params"] = [
    ({"key": "travel_map"} if param.get("key") == "foll_mod_blank2" else param)
    for param in panel["params"]
]
panel["knobs"] = ["travel_map" if key == "foll_mod_blank2" else key for key in panel["knobs"]]
MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = re.sub(r'/\* Harmony Bus v0\.2\.\d+ — Schwung MIDI FX\. \*/',
                '/* Harmony Bus v0.2.87 — Schwung MIDI FX. */', source, count=1)
source = re.sub(r'#define HB_VERSION "0\.2\.\d+"', '#define HB_VERSION "0.2.87"', source, count=1)

old_nominal = '''    int source_degree_interval=hb_nth_scale_interval_from_root(parent_scale,source_root,source_degree);
    int source_root_note=source_note-source_degree_interval;
    source_root_note=hb_note_near_pc(source_root_note,source_root);
    int target_root_note=hb_note_near_pc(source_root_note,detected.root_pc);

    for(int degree=0;degree<HB_CLOSEST_SPLIT_DEGREES;degree++){
        int target_interval=hb_nth_scale_interval_from_root(chord_scale,detected.root_pc,degree);
        int degree_pc=mod12(detected.root_pc+target_interval);
        nominal_by_degree[degree]=target_root_note+target_interval;
'''
new_nominal = '''    int source_degree_interval=hb_nth_scale_interval_from_root(parent_scale,source_root,source_degree);
    int source_root_note=source_note-source_degree_interval;
    source_root_note=hb_note_near_pc(source_root_note,source_root);

    for(int degree=0;degree<HB_CLOSEST_SPLIT_DEGREES;degree++){
        int source_interval=hb_nth_scale_interval_from_root(parent_scale,source_root,degree);
        int target_interval=hb_nth_scale_interval_from_root(chord_scale,detected.root_pc,degree);
        int degree_pc=mod12(detected.root_pc+target_interval);
        /* Closest Split stays near the source degree's ACTUAL register position.
           Using target_root+target_degree here reconstructs Relative mapping. */
        nominal_by_degree[degree]=source_root_note+source_interval;
'''
if old_nominal in source:
    source = source.replace(old_nominal, new_nominal, 1)
elif new_nominal not in source:
    raise SystemExit("Closest Split nominal block not found")

old_comment = '''    /* Solve the seven follower degrees jointly.  For the explicit 135/2467
       and 1357/246 splits, the preferred masks partition the seven-note
       chord-scale, so closest_split.h assigns every scale pitch class exactly
       once while keeping rendered MIDI pitches strictly ascending.  Only when
       Content removes too many pitch classes do we permit octave-repeated
       pitch classes; MIDI-note ordering remains strict in all cases. */
    if(!hb_build_monotonic_degree_ladder(nominal_by_degree,allowed_by_degree,output_by_degree))
'''
new_comment = '''    /* Solve jointly only to avoid exact MIDI-note collisions. Proximity is
       primary; pitch-class repetition and local inversions are allowed so
       Closest Split remains musically distinct from Relative. */
    if(!hb_build_closest_split_assignment(nominal_by_degree,allowed_by_degree,output_by_degree))
'''
if old_comment in source:
    source = source.replace(old_comment, new_comment, 1)
elif 'hb_build_closest_split_assignment(nominal_by_degree,allowed_by_degree,output_by_degree)' not in source:
    raise SystemExit("Closest Split solver call not found")

DSP.write_text(source)
