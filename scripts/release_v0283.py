#!/usr/bin/env python3
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"

module = json.loads(MODULE.read_text())
module["version"] = "0.2.83"
module["name"] = "Harmony Bus 0.2.83"
module["abbrev"] = "HB283"
module["description"] = "Harmony Bus v0.2.83 — scale ornaments independent of Content; monotonic Closest Split"
module["capabilities"]["ui_hierarchy"]["levels"]["root"]["name"] = "Harmony Bus 0.2.83"
MODULE.write_text(json.dumps(module, indent=2) + "\n")

source = DSP.read_text()
source = re.sub(r'/\* Harmony Bus v0\.2\.\d+ — Schwung MIDI FX\. \*/',
                '/* Harmony Bus v0.2.83 — Schwung MIDI FX. */', source, count=1)
source = re.sub(r'#define HB_VERSION "0\.2\.\d+"', '#define HB_VERSION "0.2.83"', source, count=1)

for header in ('approach_pitch.h', 'closest_split.h'):
    include_line = f'#include "../../../src/{header}"\n'
    if include_line not in source:
        anchor = '#include "../../../src/approach_state.h"\n'
        if anchor not in source:
            raise SystemExit("approach_state include not found")
        source = source.replace(anchor, anchor + include_line, 1)

# Replace Closest Split wholesale so this normalization is idempotent and does
# not depend on the exact implementation inherited from earlier versions.
closest_start = source.index('static int hb_map_follower_note_closest_split(')
closest_end = source.index('\nstatic int hb_map_follower_note_now(', closest_start)
closest_impl = r'''static int hb_map_follower_note_closest_split(Inst *instance,int source_note,hb_harmony_t detected,
                                               hb_harmony_t content_target){
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))
        return hb_map_note(source_note,reference_root(instance),content_target,HB_MAP_NEAREST);

    int parent_scale_index=hb_parent_scale_index(instance,detected);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);
    int source_degree=hb_source_degree_from_parent_scale(mod12(source_note-source_root),
                                                          source_root,parent_scale);
    if(source_degree<0)source_degree=0;
    if(source_degree>6)source_degree=6;

    uint16_t chord_scale=parent_scale;
    if(!parent_scale||(parent_scale&(1u<<mod12(detected.root_pc)))==0)
        chord_scale=hb_scale_mask(detected);

    uint16_t legal=(uint16_t)(content_target.pitch_mask&0x0FFFu);
    if(!legal)return source_note;

    uint16_t inferred_chord_mask=hb_harmony_chord_mask(detected);
    uint8_t active_notes[64];
    int active_count=0;
    uint16_t active_mask=0;
    if(g_bus.follower_split_map==3){
        active_count=hb_observed_notes(0,active_notes,64);
        for(int index=0;index<active_count;index++)
            active_mask|=(uint16_t)(1u<<mod12(active_notes[index]));
    }

    unsigned int allowed_by_degree[HB_CLOSEST_SPLIT_DEGREES];
    int nominal_by_degree[HB_CLOSEST_SPLIT_DEGREES];
    int output_by_degree[HB_CLOSEST_SPLIT_DEGREES];

    unsigned on_bits_135=(1u<<0)|(1u<<2)|(1u<<4);
    unsigned on_bits_1357=on_bits_135|(1u<<6);
    uint16_t on_mask_135=hb_scale_degree_mask(chord_scale,detected.root_pc,on_bits_135);
    uint16_t off_mask_135=hb_scale_degree_mask(chord_scale,detected.root_pc,0x7Fu&~on_bits_135);
    uint16_t on_mask_1357=hb_scale_degree_mask(chord_scale,detected.root_pc,on_bits_1357);
    uint16_t off_mask_1357=hb_scale_degree_mask(chord_scale,detected.root_pc,0x7Fu&~on_bits_1357);

    int source_degree_interval=hb_nth_scale_interval_from_root(parent_scale,source_root,source_degree);
    int source_root_note=source_note-source_degree_interval;
    source_root_note=hb_note_near_pc(source_root_note,source_root);
    int target_root_note=hb_note_near_pc(source_root_note,detected.root_pc);

    for(int degree=0;degree<HB_CLOSEST_SPLIT_DEGREES;degree++){
        int target_interval=hb_nth_scale_interval_from_root(chord_scale,detected.root_pc,degree);
        int degree_pc=mod12(detected.root_pc+target_interval);
        nominal_by_degree[degree]=target_root_note+target_interval;

        uint16_t preferred=0;
        int split=g_bus.follower_split_map;
        if(split==1){
            int is_on=(on_bits_135&(1u<<degree))!=0;
            preferred=(uint16_t)(legal&(is_on?on_mask_135:off_mask_135));
        }else if(split==2){
            int is_on=(on_bits_1357&(1u<<degree))!=0;
            preferred=(uint16_t)(legal&(is_on?on_mask_1357:off_mask_1357));
        }else if(split==3){
            int is_on=(active_mask&(1u<<degree_pc))!=0;
            uint16_t out_mask=(uint16_t)(chord_scale&(uint16_t)(~active_mask)&0x0FFFu);
            preferred=(uint16_t)(legal&(is_on?active_mask:out_mask));
        }else{
            int is_on=(inferred_chord_mask&(1u<<degree_pc))!=0;
            uint16_t out_mask=(uint16_t)(chord_scale&(uint16_t)(~inferred_chord_mask)&0x0FFFu);
            preferred=(uint16_t)(legal&(is_on?inferred_chord_mask:out_mask));
        }
        allowed_by_degree[degree]=(unsigned int)(preferred?preferred:legal);
    }

    /* Old Closest Split solved every source degree independently. That can
       collapse two degrees onto one pitch or even invert adjacent degrees
       (e.g. 4 and 6 both below 5). Solve the seven-degree mapping as one
       ordered ladder instead: every degree is distinct and degree order is
       strictly monotonic while each degree stays as close as possible to its
       normal target and honors its split side whenever that side is legal. */
    if(!hb_build_monotonic_degree_ladder(nominal_by_degree,allowed_by_degree,output_by_degree))
        return hb_map_note(source_note,reference_root(instance),content_target,HB_MAP_NEAREST);
    return output_by_degree[source_degree];
}'''
source = source[:closest_start] + closest_impl + source[closest_end:]

# Scale Above is a post-mapping ornament and must not inherit Follower Content.
# It always uses the separately selected/inferred follower parent scale, even
# when Content=Free makes the normal mapper chromatic.
apply_start = source.index('static int hb_apply_approach(')
apply_end = source.index('\nstatic void hb_consume_next_approach(', apply_start)
apply_impl = r'''static int hb_apply_approach(Inst *instance,int mapped,int approach){
    if(approach==HB_APPROACH_CHROM_BELOW){
        return mapped>0?mapped-1:0;
    }
    if(approach==HB_APPROACH_SCALE_ABOVE){
        hb_harmony_t detected=bus_read();
        if(!detected.valid)return mapped;
        int source_root=0;
        if(!hb_resolve_follower_reference_root(instance,&source_root))
            source_root=detected.root_pc;
        int parent_scale_index=hb_parent_scale_index(instance,detected);
        uint16_t scale_mask=hb_explicit_scale_mask(source_root,parent_scale_index);
        if(!scale_mask)scale_mask=(uint16_t)(hb_scale_mask(detected)&0x0FFFu);
        return hb_next_scale_pitch_above(mapped,(unsigned int)scale_mask);
    }
    return mapped;
}'''
source = source[:apply_start] + apply_impl + source[apply_end:]

DSP.write_text(source)
