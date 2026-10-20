#!/usr/bin/env python3
"""Publish v0.2.92: make follower transformation state local to each follower instance."""
from __future__ import annotations

import json
import re
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"


def replace_once(source: str, before: str, after: str, label: str) -> str:
    """Replace one exact source seam, accepting an already-normalized result."""
    if after in source:
        return source
    count = source.count(before)
    if count != 1:
        raise SystemExit(f"{label}: expected one source seam, found {count}")
    return source.replace(before, after, 1)


module = json.loads(MODULE.read_text())
if module.get("version") != "0.2.92":
    if module.get("version") != "0.2.91":
        runpy.run_path(str(ROOT / "scripts/release_v0291.py"), run_name="__main__")
        module = json.loads(MODULE.read_text())
    module["version"] = "0.2.92"
    module["name"] = "Harmony Bus 0.2.92"
    module["abbrev"] = "HB292"
    module["description"] = "Harmony Bus v0.2.92 — independent follower behavior per track"
    module["capabilities"]["ui_hierarchy"]["levels"]["root"]["name"] = "Harmony Bus 0.2.92"
    MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = re.sub(r'/\* Harmony Bus v0\.2\.\d+ — Schwung MIDI FX\. \*/',
                '/* Harmony Bus v0.2.92 — Schwung MIDI FX. */', source, count=1)
source = re.sub(r'#define HB_VERSION "0\.2\.\d+"', '#define HB_VERSION "0.2.92"', source, count=1)

# The cross-process shared block now owns only source-root semantics. Content,
# travel and scale are voice/follower choices and must not leak between tracks.
source = replace_once(
    source,
    '''    volatile int follower_root_policy;\n    volatile int follower_explicit_root;\n    volatile int follower_content_map;\n    volatile int follower_travel_map;\n    volatile int follower_scale;\n''',
    '''    volatile int follower_root_policy;\n    volatile int follower_explicit_root;\n''',
    "global follower shared fields",
)
source = source.replace('        shared->follower_content_map=0; /* In Chord */\n', '')
source = source.replace('        shared->follower_travel_map=0;  /* Relative */\n', '')
source = source.replace('        shared->follower_scale=0;       /* Infer */\n', '')

# Remove obsolete global getters/setters for voice-local mapping state.
source = re.sub(
    r'static int hb_global_content_map\(void\)\{.*?\n\}\nstatic int hb_global_travel_map\(void\)\{.*?\n\}\nstatic int hb_global_scale\(void\)\{.*?\n\}\n',
    '', source, count=1, flags=re.S,
)
source = re.sub(
    r'static void hb_set_global_content_map\(int value\)\{.*?\n\}\nstatic void hb_set_global_travel_map\(int value\)\{.*?\n\}\nstatic void hb_set_global_scale\(int value\)\{.*?\n\}\n',
    '', source, count=1, flags=re.S,
)

# Extend each MIDI-FX instance with the behavior that belongs to that follower.
source = replace_once(
    source,
    'typedef struct { int used,role,mode,content_map,travel_map,window_ms,dirty,frames_since_change;',
    'typedef struct { int used,role,mode,content_map,travel_map,follower_scale,follower_split_map,quant_timing,boundary_buffer_ms,approach_control,approach_mode,window_ms,dirty,frames_since_change;',
    "instance follower fields",
)

# Defaults are per instance. Chord Grid / Anticipation remain shared because they
# define the shared learned-harmony boundary; Quant Grid / Buffer are follower
# capture policy and therefore local.
source = replace_once(
    source,
    'instance->used=1;instance->role=2;instance->mode=0;instance->content_map=0;instance->travel_map=0;instance->map_target=0;',
    'instance->used=1;instance->role=2;instance->mode=0;instance->content_map=0;instance->travel_map=0;instance->follower_scale=0;instance->follower_split_map=0;instance->quant_timing=0;instance->boundary_buffer_ms=20;instance->approach_control=HB_APPROACH_OFF;instance->approach_mode=0;instance->map_target=0;',
    "instance defaults",
)

# Mapping and modifier execution must consult the current follower instance.
source = source.replace('int explicit_scale=hb_global_scale();', 'int explicit_scale=instance?instance->follower_scale:0;')
source = source.replace('if(g_bus.follower_split_map==3){', 'if(instance->follower_split_map==3){')
source = source.replace('int split=g_bus.follower_split_map;', 'int split=instance->follower_split_map;')
source = source.replace('int content_map=hb_global_content_map();', 'int content_map=instance->content_map;')
source = source.replace('int travel=hb_global_travel_map();', 'int travel=instance->travel_map;')
source = replace_once(
    source,
    'return hb_approach_effective(g_bus.approach_control,instance->approach_pad_armed);',
    'return hb_approach_effective(instance->approach_control,instance->approach_pad_armed);',
    "active approach is per follower",
)
source = source.replace('int active_approach=hb_approach_effective(g_bus.approach_control,next_override);',
                        'int active_approach=hb_approach_effective(instance->approach_control,next_override);')

# Quant Grid and Follower Buffer are local timing policy. Shared Chord Grid and
# Anticipation deliberately remain global because lookahead learning uses them.
source = replace_once(
    source,
    'static double hb_quant_grid_beats(void){\n    static const double beats[7]={0.0,0.25,0.5,1.0,2.0,4.0,8.0};\n    int index=g_bus.quant_timing;\n    return (index>=0&&index<7)?beats[index]:0.0;\n}',
    'static double hb_quant_grid_beats_for(const Inst *instance){\n    static const double beats[7]={0.0,0.25,0.5,1.0,2.0,4.0,8.0};\n    int index=instance?instance->quant_timing:0;\n    return (index>=0&&index<7)?beats[index]:0.0;\n}',
    "per-follower quant helper",
)
source = source.replace('hb_quant_grid_beats(),capture', 'hb_quant_grid_beats_for(instance),capture')
source = source.replace('double capture=hb_ms_to_beats(g_bus.boundary_buffer_ms);',
                        'double capture=hb_ms_to_beats(instance->boundary_buffer_ms);')

# Parameter writes: follower mapping, modifiers, quantization and buffer now
# mutate only the instance the UI is editing.
source = source.replace('int value=hb_global_content_map();', 'int value=instance->content_map;')
source = source.replace('    hb_set_global_content_map(value);', '    instance->content_map=value;')
source = source.replace('hb_set_global_travel_map(enum_index(parameter,opts,6,hb_global_travel_map()))',
                        'instance->travel_map=enum_index(parameter,opts,6,instance->travel_map)')
source = source.replace('g_bus.follower_split_map=enum_index(parameter,opts,4,g_bus.follower_split_map)',
                        'instance->follower_split_map=enum_index(parameter,opts,4,instance->follower_split_map)')
source = source.replace('g_bus.quant_timing=enum_index(parameter,QUANT_GRID_OPTS,7,g_bus.quant_timing)',
                        'instance->quant_timing=enum_index(parameter,QUANT_GRID_OPTS,7,instance->quant_timing)')
source = source.replace('int parsed=parse_i(parameter,g_bus.boundary_buffer_ms);if(parsed<0)parsed=0;if(parsed>1000)parsed=1000;g_bus.boundary_buffer_ms=parsed',
                        'int parsed=parse_i(parameter,instance->boundary_buffer_ms);if(parsed<0)parsed=0;if(parsed>1000)parsed=1000;instance->boundary_buffer_ms=parsed')
source = source.replace('hb_set_global_scale(enum_index(parameter,FOLLOWER_SCALE_OPTS,10,hb_global_scale()))',
                        'instance->follower_scale=enum_index(parameter,FOLLOWER_SCALE_OPTS,10,instance->follower_scale)')
source = source.replace('g_bus.approach_control=enum_index(parameter,APPROACH_OPTS,3,HB_APPROACH_OFF);instance->approach_pad_armed=HB_APPROACH_OFF;',
                        'instance->approach_control=enum_index(parameter,APPROACH_OPTS,3,HB_APPROACH_OFF);instance->approach_pad_armed=HB_APPROACH_OFF;')
source = source.replace('g_bus.approach_mode=0;instance->approach_pad_armed=HB_APPROACH_OFF;',
                        'instance->approach_mode=0;instance->approach_pad_armed=HB_APPROACH_OFF;')
source = source.replace('g_bus.approach_control=HB_APPROACH_OFF;instance->approach_pad_armed=HB_APPROACH_OFF;',
                        'instance->approach_control=HB_APPROACH_OFF;instance->approach_pad_armed=HB_APPROACH_OFF;')
source = source.replace('g_bus.approach_control=hb_approach_toggle(g_bus.approach_control,HB_APPROACH_SCALE_ABOVE,on);',
                        'instance->approach_control=hb_approach_toggle(instance->approach_control,HB_APPROACH_SCALE_ABOVE,on);')
source = source.replace('g_bus.approach_control=hb_approach_toggle(g_bus.approach_control,HB_APPROACH_CHROM_BELOW,on);',
                        'instance->approach_control=hb_approach_toggle(instance->approach_control,HB_APPROACH_CHROM_BELOW,on);')

# Parameter reads mirror the same instance-local state.
source = source.replace('opts[hb_global_content_map()]', 'opts[instance->content_map]')
source = source.replace('int travel=hb_global_travel_map();', 'int travel=instance->travel_map;')
source = source.replace('int split=g_bus.follower_split_map;', 'int split=instance->follower_split_map;')
source = source.replace('QUANT_GRID_OPTS[g_bus.quant_timing]', 'QUANT_GRID_OPTS[instance->quant_timing]')
source = source.replace('return snprintf(buffer,(size_t)length,"%d",g_bus.boundary_buffer_ms)',
                        'return snprintf(buffer,(size_t)length,"%d",instance->boundary_buffer_ms)')
source = source.replace('FOLLOWER_SCALE_OPTS[hb_global_scale()]', 'FOLLOWER_SCALE_OPTS[instance->follower_scale]')
source = source.replace('g_bus.approach_control==2?"On":"Off"', 'instance->approach_control==2?"On":"Off"')
source = source.replace('g_bus.approach_control==0?"On":"Off"', 'instance->approach_control==0?"On":"Off"')
source = source.replace('APPROACH_OPTS[(g_bus.approach_control>=0&&g_bus.approach_control<3)?g_bus.approach_control:1]',
                        'APPROACH_OPTS[(instance->approach_control>=0&&instance->approach_control<3)?instance->approach_control:1]')
source = source.replace('int mode=g_bus.approach_mode;', 'int mode=instance->approach_mode;')

# v0.2.92 state format (hb16) persists local follower behavior. Existing hb15
# snapshots migrate naturally: they held the then-global values redundantly in
# every instance, so restoring them locally preserves the old sound.
old_state = 'if(!strcmp(key,"state"))return snprintf(buffer,(size_t)length,"hb15,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",instance->role,instance->mode,instance->map_target,g_bus.inference_window_ms,hb_global_root_policy(),hb_global_explicit_root(),g_bus.global_input_root,g_bus.global_transpose,g_bus.follower_split_map,g_bus.stability,g_bus.accidentals,instance->render_channel,instance->source_channel,g_bus.chord_timing,g_bus.context,g_bus.clip_context,instance->follow_lookahead_ms,instance->retrigger_held,g_bus.anticipation,g_bus.boundary_buffer_ms,g_bus.analysis_release_ms,hb_global_content_map(),hb_global_travel_map(),hb_global_scale(),g_bus.quant_timing);'
new_state = 'if(!strcmp(key,"state"))return snprintf(buffer,(size_t)length,"hb16,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",instance->role,instance->mode,instance->map_target,g_bus.inference_window_ms,hb_global_root_policy(),hb_global_explicit_root(),g_bus.global_input_root,g_bus.global_transpose,instance->follower_split_map,g_bus.stability,g_bus.accidentals,instance->render_channel,instance->source_channel,g_bus.chord_timing,g_bus.context,g_bus.clip_context,instance->follow_lookahead_ms,instance->retrigger_held,g_bus.anticipation,instance->boundary_buffer_ms,g_bus.analysis_release_ms,instance->content_map,instance->travel_map,instance->follower_scale,instance->quant_timing);'
source = replace_once(source, old_state, new_state, "hb16 state serialization")

source = replace_once(
    source,
    '    int parsed=sscanf(state,"hb15,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",',
    '    int parsed=sscanf(state,"hb16,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",',
    "hb16 restore parser",
)
source = replace_once(source, '    int is_hb15=(parsed==25);\n    if(!is_hb15)parsed=sscanf(state,"hb14,',
                      '    int is_hb16=(parsed==25);\n    if(!is_hb16)parsed=sscanf(state,"hb15,',
                      "hb16 restore fallback")
# The inserted hb15 parser has the same 25-value argument list as hb16 and is
# followed by the old hb14 logic; rename the first classification accordingly.
source = source.replace('    int is_hb14=(!is_hb15&&parsed==24);',
                        '    int is_hb15=(!is_hb16&&parsed==25);\n    if(!is_hb16&&!is_hb15)parsed=sscanf(state,"hb14,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",\n        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],\n        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],\n        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],\n        &values[18],&values[19],&values[20],&values[21],&values[22],&values[23]);\n    int is_hb14=(!is_hb16&&!is_hb15&&parsed==24);', 1)
# All modern-format gates must include hb16.
source = source.replace('(is_hb15||', '(is_hb16||is_hb15||')
source = source.replace('if(!is_hb15&&', 'if(!is_hb16&&!is_hb15&&')

# Replace the old "global follower mapping" restore block with local migration.
source = re.sub(
    r'    /\* Follower mapping controls are global from hb14 onward\..*?g_follower_globals_restored=1;\n    \}\n',
    '''    /* Follower behavior is per instance from hb16 onward. hb15/hb14 stored\n       the same then-global mapping values in every instance, so reading each\n       copy locally is a lossless migration and avoids last-visited-track wins. */\n    instance->follow_lookahead_ms=0;\n    if((is_hb16||is_hb15||is_hb14)){\n        if(values[8]>=0&&values[8]<4)instance->follower_split_map=values[8];\n        if(values[19]>=0&&values[19]<=1000)instance->boundary_buffer_ms=values[19];\n        if(values[21]>=0&&values[21]<9)instance->content_map=values[21];\n        if(values[22]>=0&&values[22]<6)instance->travel_map=values[22];\n        if(values[23]>=0&&values[23]<10)instance->follower_scale=values[23];\n        if(values[24]>=0&&values[24]<7)instance->quant_timing=values[24];\n    }\n    if((is_hb16||is_hb15||is_hb14)&&values[0]==0&&!g_follower_globals_restored){\n        /* Only source-root semantics remain global. Restore their canonical\n           conductor copy once; follower voice settings above remain local. */\n        if(values[4]>=0&&values[4]<3)hb_set_global_root_policy(values[4]);\n        if(values[5]>=0&&values[5]<12)hb_set_global_explicit_root(values[5]);\n        g_follower_globals_restored=1;\n    }\n''',
    source, count=1, flags=re.S,
)

# Shared timing restore no longer owns Quant Grid or Buffer. Chord Grid and
# Anticipation stay shared because they define the shared harmony/prediction clock.
source = source.replace('        if(is_hb15&&values[24]>=0&&values[24]<7)g_bus.quant_timing=values[24];\n', '')
source = source.replace('        if((is_hb16||is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10)&&values[19]>=0&&values[19]<=1000)g_bus.boundary_buffer_ms=values[19];\n', '')

# Ensure old global mapping helpers are gone and local state is actually used.
for forbidden in ('hb_global_content_map()', 'hb_global_travel_map()', 'hb_global_scale()',
                  'hb_set_global_content_map(', 'hb_set_global_travel_map(', 'hb_set_global_scale('):
    if forbidden in source:
        raise SystemExit(f"v0.2.92 normalization left obsolete global follower mapping call: {forbidden}")

for required in (
    'int content_map=instance->content_map;',
    'int travel=instance->travel_map;',
    'int explicit_scale=instance?instance->follower_scale:0;',
    'int split=instance->follower_split_map;',
    'hb_quant_grid_beats_for(instance)',
    'hb_ms_to_beats(instance->boundary_buffer_ms)',
    'instance->approach_control=hb_approach_toggle',
    '"hb16,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d"',
):
    if required not in source:
        raise SystemExit(f"v0.2.92 normalization missing expected local follower behavior: {required}")

DSP.write_text(source)
