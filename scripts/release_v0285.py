#!/usr/bin/env python3
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

module = json.loads(MODULE.read_text())
module["version"] = "0.2.85"
module["name"] = "Harmony Bus 0.2.85"
module["abbrev"] = "HB285"
module["description"] = "Harmony Bus v0.2.85 — pre-boundary follower capture without post-chord latency"
module["capabilities"]["ui_hierarchy"]["levels"]["root"]["name"] = "Harmony Bus 0.2.85"
MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = re.sub(r'/\* Harmony Bus v0\.2\.\d+ — Schwung MIDI FX\. \*/',
                '/* Harmony Bus v0.2.85 — Schwung MIDI FX. */', source, count=1)
source = re.sub(r'#define HB_VERSION "0\.2\.\d+"', '#define HB_VERSION "0.2.85"', source, count=1)

include_line = '#include "../../../src/follower_timing.h"\n'
if include_line not in source:
    anchor = '#include "../../../src/approach_pitch.h"\n'
    if anchor not in source:
        raise SystemExit("approach_pitch include not found")
    source = source.replace(anchor, anchor + include_line, 1)

# Replace the inline capture-window arithmetic with the tested helper. This
# keeps Chord Grid and Quant Grid strictly PRE-boundary: a note after a chord
# start is never captured by the window that just ended.
queue_start = source.index('static int hb_queue_follower_event(')
queue_end = source.index('\nstatic int hb_source_interval_to_default_degree(', queue_start)
queue = source[queue_start:queue_end]

capture_begin = queue.index('    if(is_on){\n        double capture=hb_ms_to_beats(g_bus.boundary_buffer_ms);')
capture_else = queue.index('    }else{\n        /* Preserve articulation.', capture_begin)
new_capture = '''    if(is_on){
        double capture=hb_ms_to_beats(g_bus.boundary_buffer_ms);
        double target=hb_follower_capture_target(
            beat,
            hb_chord_grid_beats(),
            hb_anticipation_beats(),
            hb_quant_grid_beats(),
            capture);
        instance->follower_queue_target_beat[slot]=target;
'''
queue = queue[:capture_begin] + new_capture + queue[capture_else:]

old_age = '''    /* -1 guarantees at least one full tick boundary before release. */
    instance->follower_queue_age_frames[slot]=-1;
'''
new_age = '''    /* Start at age zero. Uncaptured notes can render on the first release
       attempt; a conductor change may impose at most ONE scheduler-tick
       ordering barrier in hb_release_follower_queue(). Captured notes are
       governed only by target_beat and therefore land exactly on the boundary. */
    instance->follower_queue_age_frames[slot]=0;
'''
if old_age in queue:
    queue = queue.replace(old_age, new_age, 1)
elif 'instance->follower_queue_age_frames[slot]=0;' not in queue:
    raise SystemExit("queue age initialization not found")

source = source[:queue_start] + queue + source[queue_end:]

# A queued event used to wait indefinitely while ANY conductor remained dirty
# or had candidate_frames > 0. That turned harmony classification time into
# audible follower latency, including notes played AFTER a chord onset. Keep
# only a one-release-attempt ordering guard for an uncaptured same-cycle note.
release_start = source.index('static int hb_release_follower_queue(')
release_end = source.index('\nstatic int hb_reharmonize_held_follower(', release_start)
release = source[release_start:release_end]

old_negative_age = '''        if(age<0){
            instance->follower_queue_age_frames[index]=0;
            continue;
        }
'''
release = release.replace(old_negative_age, '', 1)

old_barrier = '''        /* Do not map against stale harmony merely because this follower's
           tick happened before the conductor's tick in the same scheduler
           cycle.  This is the actual conductor-first barrier. */
        if(hb_conductor_pending_for_follow()){
            continue;
        }
'''
new_barrier = '''        /* Protect only the first release attempt of an UNCAPTURED note when
           a conductor update is concurrently pending. Never wait for harmony
           confirmation beyond that one scheduler tick, and never hold a note
           beyond an explicit pre-boundary target. */
        if(hb_follower_needs_same_tick_barrier(
               target_beat, age, frames, hb_conductor_pending_for_follow())){
            continue;
        }
'''
if old_barrier in release:
    release = release.replace(old_barrier, new_barrier, 1)
elif 'hb_follower_needs_same_tick_barrier(' not in release:
    raise SystemExit("conductor barrier block not found")

source = source[:release_start] + release + source[release_end:]
DSP.write_text(source)
