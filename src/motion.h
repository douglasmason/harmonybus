#ifndef HB_MOTION_H
#define HB_MOTION_H
#include "render_rhythm.h"
#include "chord_player.h"
/* Sixteen shared lane assignments with per-instance voice ownership. Pure evaluation uses transport position, never a
   mutable random stream, so local and MIDI-render routes make identical choices. */
#define HB_MOTION_USER_LANES 16
#define HB_MOTION_LANES 51
#define HB_MOTION_GESTURES (HB_MOTION_LANES+2)
#define HB_MOTION_OWNERS 512
#define HB_MOTION_QUEUE 2048
#define HB_MOTION_BURSTS 128
enum { HB_MO_OFF, HB_MO_VELOCITY, HB_MO_PAN, HB_MO_OCTAVE, HB_MO_ROTATE,
       HB_MO_GATE, HB_MO_SKIP, HB_MO_HARMONY, HB_MO_BELOW, HB_MO_ABOVE,
       HB_MO_ENCLOSE_AB, HB_MO_ENCLOSE_BA, HB_MO_REPEAT, HB_MO_REVERSE,
       HB_MO_TIME_SHIFT, HB_MO_SPEED, HB_MO_TRANSPOSE, HB_MO_RATCHET, HB_MO_ECHO, HB_MO_CHORD_FORM, HB_MO_AUTO_CHORD_REPEAT, HB_MO_SECONDARY_II, HB_MO_SECONDARY_V, HB_MO_SECONDARY_VI, HB_MO_BACKDOOR_II, HB_MO_BACKDOOR_V, HB_MO_CHROM_ABOVE, HB_MO_TRITONE_II, HB_MO_CADENCE_II_V, HB_MO_CADENCE_BACKDOOR, HB_MO_CADENCE_TRITONE, HB_MO_TRITONE_V, HB_MO_SECONDARY_III, HB_MO_SECONDARY_IV, HB_MO_SECONDARY_VII, HB_MO_MIXED_FIRST, HB_MO_MIXED_LAST=HB_MO_MIXED_FIRST+13, HB_MO_MOTIF, HB_MO_CHORD_STATE };
#include "cadences.h"
static int hb_mo_mixed(int operation){return operation>=HB_MO_MIXED_FIRST&&operation<=HB_MO_MIXED_LAST;}
typedef struct { int operation,pattern,amount,offset,enabled,grid,cycle,phase,probability,group,evolve,advance,every,from,through,touch_mode,auto_off; int motif_playback,motif_arrival,motif_target,motif_late,motif_grid,motif_completion; hb_cp_config chord_state;int chord_state_valid,chord_input; } hb_motion_lane;
typedef struct { hb_motion_lane lanes[HB_MOTION_LANES]; int selected,bypass,host_capabilities,enclosure_lane; unsigned serial,held_serial[HB_MOTION_LANES]; unsigned long long held;
    unsigned revision[HB_MOTION_LANES]; unsigned long long render_flags;
    unsigned long long events[HB_MOTION_LANES+1]; /* final slot snapshots the source-gesture approach */
    const unsigned long long *event_override;
    int input_valid; double input_beat; uint8_t input_notes[128];
    unsigned pitch_held,enclosure_revision; int pitch_last,enclosure;
    unsigned long long gesture_down,gesture_used,gesture_was_latched,gesture_latched;
    /* Timestamped host gestures share one detector across knobs and steps. */
    unsigned long long gesture_persistent,gesture_once,gesture_once_used,gesture_was_persistent,gesture_double,gesture_suppressed;
    int gesture_last_lane,gesture_last_valid,gesture_last_off;double gesture_last_up;
    unsigned gesture_serial[HB_MOTION_GESTURES]; int gesture_operation[HB_MOTION_GESTURES],gesture_mode[HB_MOTION_GESTURES],gesture_threshold[HB_MOTION_GESTURES];
    unsigned tap_mask,tap_serial[13]; int tap_owner[13],tap_policy[13],enclosure_auto_off; int tap_first,tap_started,enclosure_step,cadence_program;
} hb_motion_config;
typedef struct { int used,channel,source,pitch,sounding; double off_beat; unsigned long long serial; double repeat_off; int generated,lane,manual; unsigned revision,held_serial; } hb_motion_owner;
typedef struct {
    int used,lane,channel,pitch,velocity,remaining,manual,decay;
    unsigned revision,held_serial;
    double next,spacing,gate;
} hb_motion_burst;
typedef struct {
    hb_rr_route rhythm; double rhythm_now,rhythm_delay; int rhythm_enabled;
    hb_motion_owner owners[HB_MOTION_OWNERS];
    hb_motion_burst bursts[HB_MOTION_BURSTS];
    unsigned short refs[16][128];
    uint8_t queue[HB_MOTION_QUEUE][3];
    int head,count,owned,repeat_pending,pan_dirty[16],base_pan[16];
    unsigned long long next_serial;
    unsigned enclosure_revision;
    int enclosure_step,performance_valid,performance_modifier;
    double performance_beat;
    uint8_t performance_notes[128];
} hb_motion_route;
static int g_hb_hold_ms=350,g_hb_hold_restored=0;
static int hb_mo_clamp(int value,int low,int high){return value<low?low:value>high?high:value;}
static int hb_mo_round(double value){return (int)(value+(value>=0?0.5:-0.5));}
static double hb_mo_floor(double value){long long whole=(long long)value;return (double)whole-(value<(double)whole);}
static double hb_mo_grid(int index){return 0.0625*(1u<<hb_mo_clamp(index,0,8));}
static double hb_mo_cycle(int index){static const double lengths[]={0.5,1,2,4,8,12,16};return lengths[hb_mo_clamp(index,0,6)];}
static void hb_mo_lane_default(hb_motion_lane *lane){memset(lane,0,sizeof(*lane));lane->enabled=1;lane->grid=3;lane->cycle=3;lane->probability=100;lane->every=lane->from=lane->through=1;lane->touch_mode=2;lane->motif_arrival=2;lane->motif_grid=1;}
static void hb_mo_defaults(hb_motion_config *config){memset(config,0,sizeof(*config));for(int index=0;index<HB_MOTION_LANES;index++)hb_mo_lane_default(&config->lanes[index]);
    config->enclosure_lane=-1;
    /* User step slots are empty; named controls have independent settings. */
    static const int operations[]={HB_MO_BELOW,HB_MO_ABOVE,HB_MO_CHROM_ABOVE,HB_MO_SECONDARY_II,
        HB_MO_SECONDARY_V,HB_MO_SECONDARY_VI,HB_MO_BACKDOOR_II,HB_MO_BACKDOOR_V,HB_MO_TRITONE_II,
        HB_MO_CADENCE_II_V,HB_MO_CADENCE_BACKDOOR,HB_MO_CADENCE_TRITONE,
        HB_MO_ENCLOSE_AB,HB_MO_ENCLOSE_BA,HB_MO_CHORD_FORM,HB_MO_AUTO_CHORD_REPEAT,HB_MO_HARMONY,HB_MO_TRITONE_V,HB_MO_SECONDARY_III,HB_MO_SECONDARY_IV,HB_MO_SECONDARY_VII};
    for(int index=HB_MOTION_USER_LANES;index<HB_MOTION_LANES;index++)config->lanes[index].enabled=0;
    for(int index=HB_MOTION_USER_LANES;index<HB_MOTION_LANES;index++){
        config->lanes[index].operation=index<37?operations[index-HB_MOTION_USER_LANES]:HB_MO_MIXED_FIRST+index-37;config->lanes[index].amount=1;
    }
    config->lanes[30].amount=3; /* temporary seventh chord */
    config->lanes[32].amount=100;config->lanes[32].auto_off=1;

}
static void hb_mo_route_init(hb_motion_route *route){memset(route,0,sizeof(*route));for(int channel=0;channel<16;channel++)route->base_pan[channel]=64;}
/* ra1 event words store an operation outcome, never an output MIDI pitch.
   Bit 63 distinguishes snapshots from ordinary advancing event counters. */
#define HB_MO_RECORDED (1ULL<<63)
static unsigned long long hb_mo_recorded_word(const hb_motion_config *config,int index){
    if(config->held&(1ULL<<index))return 0; /* live performance temporarily replaces that recorded lane */
    return config->event_override?config->event_override[index]&HB_MO_RECORDED?config->event_override[index]:0:0;
}
/* Bit 51 extends the historical five-bit operation id without moving fields. */
static int hb_mo_word_operation(unsigned long long word){return (int)(((word>>32)&31)|((word>>46)&32));}
static unsigned long long hb_mo_operation_word(int operation){return ((unsigned long long)(operation&31)<<32)|((unsigned long long)(operation&32)<<46);}
static int hb_mo_has_scale_mode(int operation){return hb_mo_mixed(operation)||operation==HB_MO_SECONDARY_II||operation==HB_MO_SECONDARY_III||operation==HB_MO_SECONDARY_IV||operation==HB_MO_SECONDARY_VI||operation==HB_MO_SECONDARY_VII||operation==HB_MO_CADENCE_II_V;}
static int hb_mo_role(int operation){return operation==HB_MO_SECONDARY_II?1:operation==HB_MO_SECONDARY_V?2:operation==HB_MO_SECONDARY_VI?4:operation==HB_MO_BACKDOOR_II?5:operation==HB_MO_BACKDOOR_V?6:operation==HB_MO_TRITONE_II?7:operation==HB_MO_SECONDARY_III?8:operation==HB_MO_SECONDARY_IV?9:operation==HB_MO_SECONDARY_VII?10:0;}
/* Most render probes only need the operation tag. Avoid copying the full
   lane (including chord-state settings) for every inactive operation scan. */
static int hb_mo_operation(const hb_motion_config *config,int index){
    unsigned long long word=hb_mo_recorded_word(config,index);
    return word?hb_mo_word_operation(word):config->lanes[index].operation;
}
static hb_motion_lane hb_mo_settings(const hb_motion_config *config,int index){
    hb_motion_lane lane=config->lanes[index];
    unsigned long long word=hb_mo_recorded_word(config,index);
    if(word){
        lane.operation=hb_mo_word_operation(word);lane.grid=(int)((word>>37)&15);
        lane.offset=(int)((word>>41)&1023)-400;
        lane.enabled=1;lane.probability=100;lane.every=lane.from=lane.through=1;
    }
    return lane;
}
/* Single-control sequences share the same source-gesture state machine. */
static unsigned hb_mo_enclosure_mask(int operation){
    if(hb_mo_mixed(operation))return 1u<<(13+operation-HB_MO_MIXED_FIRST);
    return operation==HB_MO_ENCLOSE_AB||operation==HB_MO_ENCLOSE_BA?3:
        operation==HB_MO_CADENCE_II_V?12:operation==HB_MO_CADENCE_BACKDOOR?96:
        operation==HB_MO_CADENCE_TRITONE?768:0;
}
static int hb_mo_lane_active(const hb_motion_config *config,int index){
    unsigned long long word=hb_mo_recorded_word(config,index);
    if(word)return hb_mo_word_operation(word)!=0;
    int operation=config->lanes[index].operation;
    if(operation>=HB_MO_REPEAT&&operation<=HB_MO_SPEED&&
        (!config->host_capabilities||(config->host_capabilities<2&&!(config->held&(1ULL<<index)))))return 0;
    if(hb_mo_enclosure_mask(operation))return config->enclosure&&config->enclosure_lane==index;
    return operation && ((config->held&(1ULL<<index)) || (!config->bypass&&config->lanes[index].enabled));
}
static int hb_mo_enabled(const hb_motion_config *config){if(config->events[HB_MOTION_LANES]||(config->event_override&&config->event_override[HB_MOTION_LANES])||config->pitch_held||config->enclosure||config->gesture_down)return 1;for(int index=0;index<HB_MOTION_LANES;index++)if(hb_mo_lane_active(config,index))return 1;return 0;}
/* One ordered pending sequence is shared by knob and step gestures. */
static unsigned hb_mo_trigger_bit(int operation){
    return operation==HB_MO_BELOW?1:operation==HB_MO_ABOVE?2:
        operation==HB_MO_SECONDARY_II?4:operation==HB_MO_SECONDARY_V?8:operation==HB_MO_SECONDARY_VI?16:operation==HB_MO_BACKDOOR_II?32:operation==HB_MO_BACKDOOR_V?64:operation==HB_MO_CHROM_ABOVE?128:operation==HB_MO_TRITONE_II?256:operation==HB_MO_TRITONE_V?512:operation==HB_MO_SECONDARY_III?1024:operation==HB_MO_SECONDARY_IV?2048:operation==HB_MO_SECONDARY_VII?4096:0;
}
static int hb_mo_tap_index(unsigned bit){return bit==4096?12:bit==2048?11:bit==1024?10:bit==512?9:bit==256?8:bit==128?7:bit==64?6:bit==32?5:bit==16?4:bit==8?3:bit==4?2:bit==2?1:0;}
static int hb_mo_sequence(unsigned mask,int first){
    if(mask&8176)return 13;
    return mask==5?(first==4?9:10):mask==10?(first==2?11:12):
        mask==12?(first==4?5:6):mask==4?7:mask==8?8:
        mask==3?(first==2?1:2):mask==2?3:mask==1?4:0;
}
static int hb_mo_first_trigger(const hb_motion_config *config){
    unsigned oldest=~0u;int first=config->tap_first;
    for(int slot=0;slot<13;slot++)if((config->tap_mask&(1u<<slot))&&config->tap_serial[slot]&&config->tap_serial[slot]<oldest){
        oldest=config->tap_serial[slot];first=1<<slot;
    }
    return first;
}
static void hb_mo_tap_rebuild(hb_motion_config *config){
    config->enclosure=hb_mo_sequence(config->tap_mask,config->tap_first);
    config->enclosure_lane=-1;config->tap_started=0;config->enclosure_step=0;config->input_valid=0;config->enclosure_revision++;
    unsigned long long owners=0;
    for(int slot=0;slot<13;slot++)if((config->tap_mask&(1u<<slot))&&config->tap_owner[slot]>0)owners|=1ULL<<(config->tap_owner[slot]-1);
    for(int lane=0;lane<HB_MOTION_LANES;lane++)if((config->gesture_persistent&(1ULL<<lane))&&!(owners&(1ULL<<lane))&&!(config->gesture_down&(1ULL<<lane))&&
        (hb_mo_trigger_bit(config->lanes[lane].operation)||hb_mo_enclosure_mask(config->lanes[lane].operation))){
        config->gesture_persistent&=~(1ULL<<lane);config->gesture_latched&=~(1ULL<<lane);config->held&=~(1ULL<<lane);
    }
}
static void hb_mo_arm_sequence(hb_motion_config *config,int operation,int lane,int policy){
    unsigned mask=hb_mo_enclosure_mask(operation);
    if(hb_mo_mixed(operation)){
        for(int owner=0;owner<HB_MOTION_LANES;owner++)if(owner!=lane&&!(config->gesture_down&(1ULL<<owner))&&hb_mo_enclosure_mask(config->lanes[owner].operation)){
            config->gesture_persistent&=~(1ULL<<owner);config->gesture_latched&=~(1ULL<<owner);config->held&=~(1ULL<<owner);
        }
        config->tap_mask=mask;config->cadence_program=operation-HB_MO_MIXED_FIRST;
        config->enclosure=14;config->enclosure_step=0;config->tap_started=0;
        config->enclosure_lane=lane;config->enclosure_auto_off=policy;
        config->input_valid=0;config->enclosure_revision++;return;
    }
    config->tap_mask=mask;
    /* Above/Below reverses only for the explicitly reversed enclosure. */
    unsigned first=operation==HB_MO_ENCLOSE_AB?2:operation==HB_MO_ENCLOSE_BA?1:
        operation==HB_MO_CADENCE_II_V?4:operation==HB_MO_CADENCE_BACKDOOR?32:256;
    config->tap_first=(int)first;
    for(int slot=0;slot<13;slot++)if(mask&(1u<<slot)){
        config->tap_serial[slot]=++config->serial;config->tap_owner[slot]=0;config->tap_policy[slot]=0;
    }
    config->tap_serial[hb_mo_tap_index(first)]=++config->serial;
    for(int slot=0;slot<13;slot++)if((mask&(1u<<slot))&&(1u<<slot)!=first)config->tap_serial[slot]=++config->serial;
    hb_mo_tap_rebuild(config);config->enclosure_lane=lane;config->enclosure_auto_off=policy;
}
static void hb_mo_tap_toggle_at(hb_motion_config *config,unsigned bit,unsigned serial){
    if(config->enclosure==14){config->enclosure=0;config->tap_mask=0;}
    /* One upper approach (Above/II) and one lower/dominant approach
       (Below/V). Replacing either side preserves the other side's trigger. */
    unsigned same_side=bit>=512?bit:bit==16?16:(bit==2||bit==4||bit==32||bit==256)?294:201;
    config->tap_mask&=~(same_side&~bit);
    if(config->tap_started){unsigned keep=0;for(int k=0;k<13;k++)if(config->tap_policy[k])keep|=1u<<k;config->tap_mask&=keep;config->tap_started=0;}
    int index=hb_mo_tap_index(bit);
    if(config->tap_mask&bit){config->tap_mask&=~bit;config->tap_first=(int)config->tap_mask;}
    else {config->tap_owner[index]=config->tap_policy[index]=0;if(!config->tap_mask)config->tap_first=(int)bit;config->tap_mask|=bit;config->tap_serial[index]=serial;
        unsigned oldest=~0u;
        for(int slot=0;slot<13;slot++)if((config->tap_mask&(1u<<slot))&&config->tap_serial[slot]<oldest){
            oldest=config->tap_serial[slot];config->tap_first=1<<slot;
        }}
    config->tap_first=hb_mo_first_trigger(config);
    hb_mo_tap_rebuild(config);
}
static void hb_mo_tap_owned(hb_motion_config *config,unsigned bit,unsigned serial,int lane){
    hb_mo_tap_toggle_at(config,bit,serial);
    int index=hb_mo_tap_index(bit);
    if(config->tap_mask&bit){config->tap_owner[index]=lane<HB_MOTION_LANES?lane+1:0;config->tap_policy[index]=lane<HB_MOTION_LANES?config->lanes[lane].auto_off:0;}
}
static unsigned long long hb_mo_pending_lanes(const hb_motion_config *config){
    unsigned long long mask=0;
    if(config->enclosure){
        if(config->enclosure_lane>=0)mask|=1ULL<<config->enclosure_lane;
        else for(int k=0;k<13;k++)if((config->tap_mask&(1u<<k))&&config->tap_owner[k]>0)mask|=1ULL<<(config->tap_owner[k]-1);
    }
    return mask;
}
static void hb_mo_end_lanes(hb_motion_config *config,unsigned long long mask){
    config->held&=~mask;config->gesture_latched&=~mask;
    config->gesture_persistent&=~mask;config->gesture_once&=~mask;config->gesture_once_used&=~mask;
    config->gesture_down&=~mask;config->gesture_was_latched&=~mask;
    if(config->enclosure_lane>=0&&(mask&(1ULL<<config->enclosure_lane))){config->enclosure=0;config->tap_mask=0;}
    else if(config->enclosure_lane<0){
        unsigned before=config->tap_mask;
        for(int k=0;k<13;k++)if(config->tap_owner[k]>0&&(mask&(1ULL<<(config->tap_owner[k]-1))))config->tap_mask&=~(1u<<k);
        if(before!=config->tap_mask){config->tap_first=hb_mo_first_trigger(config);hb_mo_tap_rebuild(config);}
    }
}
static void hb_mo_tap_toggle(hb_motion_config *config,unsigned bit){hb_mo_tap_toggle_at(config,bit,++config->serial);}
static void hb_mo_tap_remove(hb_motion_config *config,unsigned bit){
    if(!config->tap_started&&(config->tap_mask&bit)){
        config->tap_mask&=~bit;config->tap_first=hb_mo_first_trigger(config);hb_mo_tap_rebuild(config);
    }
}
static void hb_mo_gesture_reset(hb_motion_config *config){
    config->held&=~(config->gesture_down|config->gesture_latched);
    config->gesture_down=config->gesture_used=config->gesture_was_latched=config->gesture_latched=0;
    config->gesture_persistent=config->gesture_once=config->gesture_once_used=0;
    config->gesture_was_persistent=config->gesture_double=config->gesture_suppressed=0;config->gesture_last_valid=0;
    config->tap_mask=0;config->tap_started=0;config->enclosure_step=0;config->events[HB_MOTION_LANES]=0;
}
static void hb_mo_gesture(hb_motion_config *config,int id,int down,int elapsed){
    unsigned long long bit=1ULL<<id;
    if(down){
        if(config->gesture_down&bit)return;
        int operation=id<HB_MOTION_LANES?config->lanes[id].operation:id==HB_MOTION_LANES?HB_MO_ABOVE:HB_MO_BELOW;
        int mode=id<HB_MOTION_LANES?config->lanes[id].touch_mode:2;
        config->gesture_down|=bit;config->gesture_used&=~bit;
        config->gesture_operation[id]=operation;config->gesture_mode[id]=mode;config->gesture_threshold[id]=g_hb_hold_ms;
        config->gesture_serial[id]=++config->serial;
        if(config->gesture_latched&bit)config->gesture_was_latched|=bit;else config->gesture_was_latched&=~bit;
        if(hb_mo_trigger_bit(operation)!=0){
            if(mode==1)hb_mo_tap_owned(config,hb_mo_trigger_bit(operation),config->gesture_serial[id],id);
            else if(id<HB_MOTION_LANES){config->held|=bit;config->held_serial[id]=config->gesture_serial[id];}
        }else if(hb_mo_enclosure_mask(operation)){
            unsigned wanted=hb_mo_enclosure_mask(operation);
            if(config->enclosure&&config->enclosure_lane==id&&config->tap_mask==wanted&&(!config->tap_started||config->enclosure_auto_off)){config->enclosure=0;config->tap_mask=0;}
            else {hb_mo_arm_sequence(config,operation,id,id<HB_MOTION_LANES?config->lanes[id].auto_off:0);}
        }else if(id<HB_MOTION_LANES){
            if(mode==1){config->gesture_latched^=bit;if(config->gesture_latched&bit)config->held|=bit;else config->held&=~bit;}
            else config->held|=bit;
            config->held_serial[id]=++config->serial;
        }
        return;
    }
    if(!(config->gesture_down&bit))return;
    config->gesture_down&=~bit;
    int operation=config->gesture_operation[id],mode=config->gesture_mode[id];
    int short_tap=elapsed>=0&&elapsed<config->gesture_threshold[id];
    if(hb_mo_trigger_bit(operation)!=0){
        unsigned modifier=hb_mo_trigger_bit(operation);
        if(id<HB_MOTION_LANES)config->held&=~bit;
        if(mode==2&&short_tap&&!(config->gesture_used&bit))hb_mo_tap_owned(config,modifier,config->gesture_serial[id],id);
        else if(mode!=1||elapsed<0)hb_mo_tap_remove(config,modifier);
    }else if(hb_mo_enclosure_mask(operation)){
        if(elapsed<0||mode==0||(!short_tap&&mode!=1)){config->enclosure=0;config->tap_mask=0;}
    }else if(id<HB_MOTION_LANES&&mode!=1){
        if(mode==2&&short_tap&&!(config->gesture_was_latched&bit))config->gesture_latched|=bit;
        else config->gesture_latched&=~bit;
        if(config->gesture_latched&bit)config->held|=bit;else config->held&=~bit;
    }else if(elapsed<0&&id<HB_MOTION_LANES){config->gesture_latched&=~bit;config->held&=~bit;}
}
/* No delayed single-tap dispatch: a second short tap promotes the existing
   activation. A second tap after switching off is absorbed, not re-armed. */
static void hb_mo_modern_gesture(hb_motion_config *config,int id,int down,int elapsed,double stamp){
    unsigned long long bit=1ULL<<id;
    if(down){
        if(config->gesture_down&bit)return;
        int pair=config->gesture_last_valid&&config->gesture_last_lane==id&&stamp>=config->gesture_last_up&&stamp-config->gesture_last_up<=300;
        config->gesture_double=pair?config->gesture_double|bit:config->gesture_double&~bit;
        config->gesture_suppressed&=~bit;
        if(pair&&config->gesture_last_off){
            config->gesture_suppressed|=bit;config->gesture_down|=bit;config->gesture_mode[id]=3;return;
        }
        if(config->gesture_persistent&bit)config->gesture_was_persistent|=bit;else config->gesture_was_persistent&=~bit;
        int operation=id<HB_MOTION_LANES?config->lanes[id].operation:0;
        if((config->gesture_persistent&bit)&&(hb_mo_enclosure_mask(operation))){
            config->gesture_down|=bit;config->gesture_used&=~bit;config->gesture_operation[id]=operation;
            config->gesture_mode[id]=3;config->gesture_threshold[id]=g_hb_hold_ms;return;
        }
        int saved=id<HB_MOTION_LANES?config->lanes[id].touch_mode:2;if(id<HB_MOTION_LANES)config->lanes[id].touch_mode=2;
        hb_mo_gesture(config,id,1,0);if(id<HB_MOTION_LANES)config->lanes[id].touch_mode=saved;
        config->gesture_mode[id]=3;return;
    }
    if(!(config->gesture_down&bit))return;
    config->gesture_down&=~bit;
    if(config->gesture_suppressed&bit){config->gesture_suppressed&=~bit;config->gesture_last_valid=0;return;}
    int operation=config->gesture_operation[id],short_tap=elapsed>=0&&elapsed<config->gesture_threshold[id];
    unsigned trigger=hb_mo_trigger_bit(operation);
    int was_persistent=(config->gesture_was_persistent&bit)!=0;
    if(elapsed<0){hb_mo_end_lanes(config,bit);config->gesture_last_valid=0;return;}
    if(!short_tap||(!was_persistent&&(config->gesture_used&bit))){
        if(was_persistent&&trigger)config->held&=~bit;
        if(!was_persistent){
            hb_mo_end_lanes(config,bit);
            if(trigger)hb_mo_tap_remove(config,trigger);
            else if(hb_mo_enclosure_mask(operation)){config->enclosure=0;config->tap_mask=0;}
        }
        config->gesture_last_valid=0;return;
    }
    int persistent=(config->gesture_double&bit)&&!was_persistent,turned_off=0;
    if(was_persistent){hb_mo_end_lanes(config,bit);turned_off=1;}
    else if(trigger){
        config->held&=~bit;
        if(persistent){
            if(!(config->tap_mask&trigger))hb_mo_tap_owned(config,trigger,config->gesture_serial[id],id);
            config->tap_policy[hb_mo_tap_index(trigger)]=2;
            config->gesture_persistent|=bit;config->gesture_latched|=bit;
        }else{
            hb_mo_tap_owned(config,trigger,config->gesture_serial[id],id);
            config->tap_policy[hb_mo_tap_index(trigger)]=id<HB_MOTION_LANES&&config->lanes[id].auto_off==1?1:0;
            turned_off=!(config->tap_mask&trigger);
        }
    }else if(hb_mo_enclosure_mask(operation)){
        if(persistent){
            hb_mo_arm_sequence(config,operation,id,2);
            config->gesture_persistent|=bit;config->gesture_latched|=bit;
        }else {config->enclosure_auto_off=id<HB_MOTION_LANES&&config->lanes[id].auto_off==1?1:0;turned_off=!config->enclosure;}
    }else if(id<HB_MOTION_LANES&&operation){
        if(persistent){config->gesture_persistent|=bit;config->gesture_once&=~bit;config->gesture_latched|=bit;config->held|=bit;}
        else if(config->gesture_was_latched&bit){hb_mo_end_lanes(config,bit);turned_off=1;}
        else {config->gesture_latched|=bit;config->held|=bit;config->gesture_once|=bit;}
    }else config->held&=~bit;
    config->gesture_last_lane=id;config->gesture_last_up=stamp;
    config->gesture_last_valid=1;config->gesture_last_off=turned_off;
}
/* Knobs have explicit latch turns; taps never promote via double-tap. */
static void hb_mo_knob_gesture(hb_motion_config *config,int id,int down,int elapsed,double stamp){
    unsigned long long bit=1ULL<<id;
    if(down){
        if(config->gesture_down&bit)return;
        if(!(config->gesture_persistent&bit))hb_mo_end_lanes(config,bit);
        config->gesture_last_valid=0;
        hb_mo_modern_gesture(config,id,1,0,stamp);config->gesture_mode[id]=4;return;
    }
    if(!(config->gesture_down&bit))return;
    if(elapsed>=0&&(config->gesture_was_persistent&bit)){
        config->gesture_down&=~bit;
        if(hb_mo_trigger_bit(config->gesture_operation[id]))config->held&=~bit;
        config->gesture_last_valid=0;return;
    }
    config->gesture_double&=~bit;
    hb_mo_modern_gesture(config,id,0,elapsed,stamp);config->gesture_last_valid=0;
}
static void hb_mo_latch_set(hb_motion_config *config,int id,int on){
    unsigned long long bit=1ULL<<id;
    if(on&&(config->gesture_persistent&bit)){
        config->gesture_down&=~bit;
        if(hb_mo_trigger_bit(config->lanes[id].operation))config->held&=~bit;
        config->gesture_last_valid=0;return;
    }
    hb_mo_end_lanes(config,bit);config->gesture_last_valid=0;
    int operation=config->lanes[id].operation;
    if(!on||!operation)return;
    unsigned trigger=hb_mo_trigger_bit(operation);
    if(trigger){
        hb_mo_tap_owned(config,trigger,++config->serial,id);
        config->tap_policy[hb_mo_tap_index(trigger)]=2;
    }else if(hb_mo_enclosure_mask(operation))hb_mo_arm_sequence(config,operation,id,2);
    else {config->held|=bit;config->held_serial[id]=++config->serial;}
    config->gesture_persistent|=bit;config->gesture_latched|=bit;
}
/* Live gates do not consume pending source gestures. */
static unsigned hb_mo_held_trigger(hb_motion_config *config){
    unsigned newest=0,selected=0;
    for(int lane=0;lane<HB_MOTION_LANES;lane++)if(config->held&(1ULL<<lane)){
        unsigned trigger=hb_mo_trigger_bit(config->lanes[lane].operation);
        if(trigger&&config->held_serial[lane]>=newest){newest=config->held_serial[lane];selected=trigger;}
    }
    for(int id=0;id<HB_MOTION_GESTURES;id++)if((config->gesture_down&(1ULL<<id))&&config->gesture_mode[id]!=1){
        unsigned trigger=hb_mo_trigger_bit(config->gesture_operation[id]);
        if(trigger){
            config->gesture_used|=1ULL<<id;
            if(config->gesture_serial[id]>=newest){newest=config->gesture_serial[id];selected=trigger;}
        }
    }
    if(!selected&&config->pitch_held)selected=config->pitch_last==1?1:2;
    return selected;
}
static int hb_mo_held_modifier(hb_motion_config *config){
    unsigned trigger=hb_mo_held_trigger(config);return trigger==1?-1:trigger==2?1:(trigger==128||trigger==512)?2:0;
}
static int hb_mo_held_secondary(hb_motion_config *config){
    unsigned trigger=hb_mo_held_trigger(config);return trigger==4?1:trigger==8?2:trigger==16?4:trigger==32?5:trigger==64?6:trigger==256?7:trigger==1024?8:trigger==2048?9:trigger==4096?10:0;
}
/* Bits 0-1: pitch approach; 2-3: piano alias; 4-6: secondary II/V/target/VI.
   Bit 7 is output-only: chord construction already applied pitch approaches. */
#define HB_MO_CHORD_APPROACH (1ULL<<7)
static int hb_mo_chord_approach_done(const hb_motion_config *config){
    return config->event_override&&(config->event_override[HB_MOTION_LANES]&HB_MO_CHORD_APPROACH);
}
#define HB_MO_SIMPLE (1ULL<<9)
#define HB_MO_CONNECTOR_ABOVE (1ULL<<10)
#define HB_MO_LEGACY_II (1ULL<<11)
#define HB_MO_SIMPLE_SCALE (1ULL<<12)
#define HB_MO_CADENCE_MASK (127ULL<<13)
#define HB_MO_INTENT_MASK (((1ULL<<23)-1)<<20)
#define HB_MO_SOURCE_MASK (HB_MO_SOURCE_MASK_BASE|(4095ULL<<43)|(255ULL<<55))
#define HB_MO_SOURCE_MASK_BASE (8063ULL|HB_MO_CADENCE_MASK|HB_MO_INTENT_MASK)
static unsigned long long hb_mo_role_word(int role){return ((unsigned long long)(role&7)<<4)|((unsigned long long)(role&8)<<5);}
static unsigned long long hb_mo_source_flags(hb_motion_config *config,unsigned trigger){
    int lane=-1;unsigned newest=0;
    for(int index=0;index<HB_MOTION_LANES;index++)if(hb_mo_trigger_bit(config->lanes[index].operation)==trigger){
        unsigned serial=(config->held&(1ULL<<index))?config->held_serial[index]:0;
        if((config->gesture_down&(1ULL<<index))&&config->gesture_mode[index]!=1&&config->gesture_serial[index]>serial)serial=config->gesture_serial[index];
        if(serial&&serial>=newest){newest=serial;lane=index;}
    }
    if(lane<0&&trigger){int owner=config->tap_owner[hb_mo_tap_index(trigger)];lane=owner?owner-1:config->enclosure_lane;}
    return (trigger==128?HB_MO_CONNECTOR_ABOVE:0)|(((trigger==4||trigger==16||trigger==1024||trigger==2048||trigger==4096)&&lane>=0&&hb_mo_has_scale_mode(config->lanes[lane].operation)&&config->lanes[lane].amount>=2)?(HB_MO_SIMPLE|(config->lanes[lane].amount==3?HB_MO_SIMPLE_SCALE:0)):0);
}
static const hb_cadence_step *hb_mo_current_cadence(hb_motion_config *config){
    if(hb_mo_held_trigger(config))return 0;
    const unsigned long long *events=config->event_override?config->event_override:config->events;
    return hb_cadence_decode((unsigned)((events[HB_MOTION_LANES]>>13)&127));
}
static int hb_mo_source_secondary(const hb_motion_config *config){
    const unsigned long long *events=config->event_override?config->event_override:config->events;
    return (int)(((events[HB_MOTION_LANES]>>4)&7)|((events[HB_MOTION_LANES]>>5)&8));
}
static int hb_mo_source_modifier(const hb_motion_config *config){
    if(config->event_override){
        if(config->pitch_held)return 0;
        for(int lane=0;lane<HB_MOTION_LANES;lane++)if((config->held&(1ULL<<lane))&&hb_mo_trigger_bit(config->lanes[lane].operation))return 0;
        for(int gesture=0;gesture<HB_MOTION_GESTURES;gesture++)if((config->gesture_down&(1ULL<<gesture))&&config->gesture_mode[gesture]!=1&&hb_mo_trigger_bit(config->gesture_operation[gesture]))return 0;
    }
    const unsigned long long *events=config->event_override?config->event_override:config->events;
    return (events[HB_MOTION_LANES]&3)==1?-1:(events[HB_MOTION_LANES]&3)==2?1:(events[HB_MOTION_LANES]&3)==3?2:0;
}
static const unsigned HB_MO_SEQUENCE_FIRST[]={0,2,1,2,1,4,8,4,8,4,1,2,8};
static const unsigned HB_MO_SEQUENCE_SECOND[]={0,1,2,0,0,8,4,0,0,1,4,8,2};
static int hb_mo_sequence_count(const hb_motion_config *config){
    if(config->enclosure==14)return HB_CADENCES[config->cadence_program].length;
    if(config->enclosure!=13)return HB_MO_SEQUENCE_SECOND[hb_mo_clamp(config->enclosure,0,12)]?2:config->enclosure?1:0;
    int count=0;for(int slot=0;slot<13;slot++)if(config->tap_mask&(1u<<slot))count++;return count;
}
static unsigned hb_mo_sequence_trigger(const hb_motion_config *config,int step){
    if(config->enclosure==14)return 0;
    if(config->enclosure!=13){int kind=hb_mo_clamp(config->enclosure,0,12);return step==0?HB_MO_SEQUENCE_FIRST[kind]:step==1?HB_MO_SEQUENCE_SECOND[kind]:0;}
    unsigned remaining=config->tap_mask;
    for(int index=0;index<=step;index++){
        unsigned oldest=~0u,trigger=0;
        for(int slot=0;slot<13;slot++)if((remaining&(1u<<slot))&&config->tap_serial[slot]<=oldest){oldest=config->tap_serial[slot];trigger=1u<<slot;}
        if(index==step)return trigger;
        remaining&=~trigger;
    }
    return 0;
}
static const char *hb_mo_pending_status(const hb_motion_config *config){
    if(config->enclosure==14){
        static char ordered[128];int used=0;
        const hb_cadence_program *program=&HB_CADENCES[config->cadence_program];
        for(int step=config->enclosure_step;step<program->length&&used<(int)sizeof(ordered);step++)
            used+=snprintf(ordered+used,sizeof(ordered)-(size_t)used,"%s%s",step==config->enclosure_step?"":" > ",program->steps[step].label);
        return ordered;
    }
    static const char *start[]={"Off","Above > Below > Target","Below > Above > Target","Armed Above","Armed Below",
        "II > V > Target","V > II > Target","Armed II","Armed V","II > Below > Target","Below > II > Target","Above > V > Target","V > Above > Target"};
    if(config->enclosure==13){
        static char sequence[96];int used=0,count=hb_mo_sequence_count(config);
        if(count==1){unsigned trigger=hb_mo_sequence_trigger(config,0);return trigger==32?"Armed Back II":trigger==64?"Armed Back V":trigger==128?"Armed Chrom Above":trigger==256?"Armed Tritone II":trigger==512?"Armed Tritone V":trigger==1024?"Armed III":trigger==2048?"Armed IV":trigger==4096?"Armed VII":"Armed VI";}
        for(int step=config->enclosure_step;step<count;step++){
            unsigned trigger=hb_mo_sequence_trigger(config,step);
            const char *name=trigger==1?"Below":trigger==2?"Above":trigger==4?"II":trigger==8?"V":trigger==16?"VI":trigger==32?"Back II":trigger==64?"Back V":trigger==128?"Chrom Above":trigger==256?"Tritone II":trigger==512?"Tritone V":trigger==1024?"III":trigger==2048?"IV":"VII";
            used+=snprintf(sequence+used,sizeof(sequence)-(size_t)used,"%s > ",name);
        }
        snprintf(sequence+used,sizeof(sequence)-(size_t)used,"Target");return sequence;
    }
    int kind=hb_mo_clamp(config->enclosure,0,12);
    if(!kind||!HB_MO_SEQUENCE_SECOND[kind])return start[kind];
    if(config->enclosure_step>=2)return "Armed Target";
    if(config->enclosure_step==1){unsigned second=HB_MO_SEQUENCE_SECOND[kind];return second==1?"Below > Target":second==2?"Above > Target":second==4?"II > Target":"V > Target";}
    return start[kind];
}
static const char *hb_mo_lane_status(const hb_motion_config *config,int lane){
    unsigned long long bit=1ULL<<lane;int operation=config->lanes[lane].operation;
    if(config->gesture_down&bit)return "Held";
    if(config->gesture_persistent&bit)return "Persistent";
    if(config->gesture_once&bit)return "Armed";
    if(config->gesture_latched&bit)return "Latched";
    if(config->held&bit)return "Held";
    if(config->enclosure&&((config->enclosure_lane==lane)||
        (hb_mo_trigger_bit(operation)&config->tap_mask)))
        return hb_mo_pending_status(config);
    return "Off";
}
/* Source events advance once before mapping; UI reads and generated copies are pure. */
static void hb_mo_input_reset(hb_motion_config *config){
    memset(config->events,0,sizeof(config->events));config->input_valid=0;
}
static void hb_mo_input(hb_motion_config *config,int pitch,double beat,double grouping){
    double elapsed=beat-config->input_beat;
    int chord=!config->input_valid||elapsed<0||elapsed>grouping||config->input_notes[pitch];
    if(chord){config->input_valid=1;config->input_beat=beat;memset(config->input_notes,0,sizeof(config->input_notes));}
    config->input_notes[pitch]=1;
    config->gesture_used|=config->gesture_down;
    int held=hb_mo_held_modifier(config)||hb_mo_held_secondary(config); /* mark touches used before buffering */
    if(chord){
        int modifier=0,secondary=0;unsigned long long flags=0;
        if(!held&&config->enclosure==14){
            const hb_cadence_program *program=&HB_CADENCES[config->cadence_program];
            int step=config->enclosure_step++,lane=config->enclosure_lane;
            flags=(unsigned long long)(config->cadence_program*HB_CADENCE_STEPS+step+1)<<13;
            if(lane>=0&&config->lanes[lane].amount>=2)flags|=HB_MO_SIMPLE|(config->lanes[lane].amount==3?HB_MO_SIMPLE_SCALE:0);
            config->tap_started=1;
            if(config->enclosure_step>=program->length){
                if(config->enclosure_auto_off){config->enclosure_step=0;config->tap_started=0;}
                else config->enclosure=0;
            }
        }else if(!held&&config->enclosure){
            int step=config->enclosure_step++;
            int count=hb_mo_sequence_count(config);
            unsigned trigger=hb_mo_sequence_trigger(config,step);
            modifier=trigger==1?-1:trigger==2?1:(trigger==128||trigger==512)?2:0;
            secondary=trigger==4?1:trigger==8?2:trigger==16?4:trigger==32?5:trigger==64?6:trigger==256?7:trigger==1024?8:trigger==2048?9:trigger==4096?10:step>=count&&(config->tap_mask&8188)?3:0;
            flags=hb_mo_source_flags(config,trigger);
            config->tap_started=1;
            if(count==1||config->enclosure_step>=count+1){
                int repeat=config->enclosure_lane>=0?config->enclosure_auto_off:0;
                if(config->enclosure_lane<0){
                    unsigned keep=0;
                    for(int k=0;k<13;k++)if(config->tap_policy[k]&&(config->tap_mask&(1u<<k)))keep|=1u<<k;
                    config->tap_mask=keep;
                    if(keep){config->tap_first=hb_mo_first_trigger(config);config->enclosure=hb_mo_sequence(keep,config->tap_first);repeat=1;}
                }
                if(repeat){config->enclosure_step=0;config->tap_started=0;}
                else config->enclosure=0;
            }
        }
        config->events[HB_MOTION_LANES]=(modifier<0?1:modifier==2?3:modifier>0?2:0)|hb_mo_role_word(secondary)|flags;
    }
    for(int lane=0;lane<HB_MOTION_LANES;lane++)
        if(config->lanes[lane].advance==1||(config->lanes[lane].advance==2&&chord))config->events[lane]++;
}
static unsigned hb_mo_hash(unsigned value){value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;return value^(value>>16);}
/* Conditions use transport cycles, independently of pattern phase or input
   advancement. Negative beat means stopped; unrestricted lanes keep working. */
static int hb_mo_condition_cycle(const hb_motion_lane *lane,double beat){
    int every=hb_mo_clamp(lane->every,1,16);
    if(beat<0)return 1;
    long long cycle=(long long)hb_mo_floor((beat+1e-9)/hb_mo_cycle(lane->cycle));
    return (int)(cycle%every)+1;
}
static int hb_mo_condition(const hb_motion_config *config,int index,double beat){
    hb_motion_lane resolved=hb_mo_settings(config,index);const hb_motion_lane *lane=&resolved;
    if(config->held&(1ULL<<index))return 1;
    if(lane->every<=1)return 1;
    if(beat<0)return 0;
    int cycle=hb_mo_condition_cycle(lane,beat);
    return cycle>=lane->from&&cycle<=lane->through;
}
/* Return false means do not perform this operation; it never means skip a note. */
static int hb_mo_value_at(const hb_motion_config *config,int index,double beat,double condition_beat,int voice,double *value){
    unsigned long long word=hb_mo_recorded_word(config,index);
    if(word){*value=(double)(int32_t)(uint32_t)word/1000.0;return hb_mo_word_operation(word)!=0;}

    if(!hb_mo_lane_active(config,index))return 0;
    /* Recorded overrides returned above; the live lane can be read directly. */
    const hb_motion_lane *lane=&config->lanes[index];
    int held=(config->held&(1ULL<<index))!=0;
    if(!hb_mo_condition(config,index,condition_beat)||(!held&&!lane->probability))return 0;
    double grid=hb_mo_grid(lane->grid),cycle=hb_mo_cycle(lane->cycle);
    if(lane->advance){const unsigned long long *events=config->event_override?config->event_override:config->events;
        beat=(double)(events[index]?events[index]-1:0)*grid;}
    double shifted=beat+(double)lane->phase*grid;
    long long iteration=(long long)hb_mo_floor(shifted/cycle);
    double position=shifted-(double)iteration*cycle;
    int steps=(int)(cycle/grid+0.999999),step=(int)hb_mo_floor((position+1e-8)/grid);
    if(steps<1)steps=1;if(step>=steps)step=steps-1;
    unsigned seed=(unsigned)(index+1)*0x9e3779b9u^(unsigned)step*0x85ebca6bu;
    if(lane->evolve)seed^=(unsigned)iteration*0xc2b2ae35u;
    if(lane->group)seed^=(unsigned)(voice+1)*0x27d4eb2du;
    if(!held&&lane->probability<100&&hb_mo_hash(seed)%100u>=(unsigned)lane->probability)return 0;
    double pattern=1.0;
    if(lane->pattern==1)pattern=(step&1)?1.0:-1.0;
    else if(lane->pattern==2)pattern=steps>1?2.0*step/(steps-1)-1.0:0;
    else if(lane->pattern==3)pattern=steps>1?1.0-2.0*step/(steps-1):0;
    else if(lane->pattern==4){double phase=(double)step/steps;pattern=phase<0.5?4*phase-1:3-4*phase;}
    else if(lane->pattern==5)pattern=((int)hb_mo_floor(position)&1)?1.0:0.0;
    else if(lane->pattern==6)pattern=2.0*(hb_mo_hash(seed^0xa511e9b3u)%10001u)/10000.0-1.0;
    *value=(lane->operation==HB_MO_ECHO?0:lane->offset)+lane->amount*pattern;return 1;
}
static void hb_mo_capture(hb_motion_config *config,double beat,double condition,int voice,unsigned long long result[HB_MOTION_LANES+1]){
    for(int lane=0;lane<HB_MOTION_LANES;lane++){
        double value=0;hb_motion_lane settings=config->lanes[lane];
        int active=hb_mo_value_at(config,lane,beat,condition,voice,&value);
        if((config->held&(1ULL<<lane))&&(settings.operation==HB_MO_BELOW||settings.operation==HB_MO_ABOVE||settings.operation==HB_MO_CHROM_ABOVE||settings.operation==HB_MO_TRITONE_V))active=0;
        /* Explicitly evolving automatic lanes remain live on replay. */
        if(settings.operation==HB_MO_AUTO_CHORD_REPEAT||(settings.evolve&&!(config->held&(1ULL<<lane)))){result[lane]=config->events[lane];continue;}
        result[lane]=HB_MO_RECORDED|hb_mo_operation_word(active?settings.operation:0)|
            ((unsigned long long)settings.grid<<37)|((unsigned long long)(settings.offset+400)<<41)|
            (unsigned long long)(uint32_t)(int32_t)hb_mo_round(value*1000.0);
    }
    int modifier=hb_mo_held_modifier(config);
    int secondary=hb_mo_held_secondary(config);
    result[HB_MOTION_LANES]=(secondary?hb_mo_role_word(secondary):modifier<0?1:modifier==2?3:modifier>0?2:config->events[HB_MOTION_LANES]);
    if(secondary||modifier)result[HB_MOTION_LANES]|=hb_mo_source_flags(config,hb_mo_held_trigger(config));
}
static int hb_mo_value(const hb_motion_config *config,int index,double beat,int voice,double *value){
    return hb_mo_value_at(config,index,beat,beat,voice,value);
}
static int hb_mo_push(hb_motion_route *route,int status,int pitch,int velocity){
    if(route->rhythm_enabled||hb_rr_active(&route->rhythm)){
        uint8_t message[3]={(uint8_t)status,(uint8_t)pitch,(uint8_t)velocity};
        hb_rr_push(&route->rhythm,message,route->rhythm_now,route->rhythm_delay);return 1;
    }
    if(route->count>=HB_MOTION_QUEUE)return 0;
    int index=(route->head+route->count)%HB_MOTION_QUEUE;
    route->queue[index][0]=(uint8_t)status;route->queue[index][1]=(uint8_t)pitch;route->queue[index][2]=(uint8_t)velocity;route->count++;return 1;
}
static int hb_mo_pop(hb_motion_route *route,uint8_t message[3]){if(!route->count)return hb_rr_pop(&route->rhythm,route->rhythm_now,message);memcpy(message,route->queue[route->head],3);route->head=(route->head+1)%HB_MOTION_QUEUE;route->count--;return 1;}
/* Releasing one source must not silence a different source mapped to that pitch. */
static int hb_mo_release(hb_motion_route *route,hb_motion_owner *owner){
    if(!owner->sounding)return 1;
    unsigned short *refs=&route->refs[owner->channel][owner->pitch];
    if((*refs==1||route->rhythm_enabled||hb_rr_active(&route->rhythm))&&!hb_mo_push(route,0x80|owner->channel,owner->pitch,0))return 0;
    if(*refs)(*refs)--;owner->sounding=0;return 1;
}
static void hb_mo_due(hb_motion_route *route,double beat){
    route->rhythm_now=beat;
    if(!route->owned)return;
    for(int index=0;index<HB_MOTION_OWNERS;index++){
        hb_motion_owner *owner=&route->owners[index];
        if(owner->used&&!owner->generated&&owner->sounding&&owner->off_beat>=0&&beat+1e-9>=owner->off_beat)hb_mo_release(route,owner);
    }
}
static void hb_mo_panic(hb_motion_route *route){
    memset(route->bursts,0,sizeof(route->bursts));
    for(int index=0;index<HB_MOTION_OWNERS;index++){
        hb_motion_owner *owner=&route->owners[index];
        if(owner->used&&hb_mo_release(route,owner)){owner->used=0;route->owned--;}
    }
    if(hb_rr_active(&route->rhythm))hb_rr_panic(&route->rhythm);
}
/* The adapter supplies the finished output pitch/velocity, optional pan, and
   gate deadline. Skipped notes still get an owner to swallow their matching OFF. */
static int hb_mo_event(hb_motion_route *route,const uint8_t message[3],int pitch,int velocity,int pan,double off_beat,int skip){
    int status=message[0]&0xf0,channel=message[0]&15,source=message[1]&127;
    if(status==0xb0&&source==10)route->base_pan[channel]=message[2]&127;
    if(status!=0x90&&status!=0x80)return hb_mo_push(route,message[0],message[1],message[2]);
    int on=status==0x90&&message[2];
    if(!on){
        hb_motion_owner *oldest=0;
        for(int index=0;index<HB_MOTION_OWNERS;index++){
            hb_motion_owner *owner=&route->owners[index];
            if(owner->used&&!owner->generated&&owner->channel==channel&&owner->source==source&&(!oldest||owner->serial<oldest->serial))oldest=owner;
        }
        if(oldest){
            if(!hb_mo_release(route,oldest))return 0;
            oldest->used=0;route->owned--;return 1;
        }
        /* Notes that were already held when lanes were enabled are unowned. */
        if(route->refs[channel][source])return 1;
        return hb_mo_push(route,0x80|channel,source,0);
    }
    int slot=-1;
    for(int index=0;index<HB_MOTION_OWNERS;index++)if(!route->owners[index].used){slot=index;break;}
    if(slot<0||route->count>HB_MOTION_QUEUE-2)return 0;
    hb_motion_owner *owner=&route->owners[slot];
    *owner=(hb_motion_owner){.used=1,.channel=channel,.source=source,.pitch=pitch,.sounding=!skip,.off_beat=off_beat,.serial=route->next_serial++};route->owned++;
    if(skip)return 1;
    if(pan<0&&route->pan_dirty[channel]){
        hb_mo_push(route,0xb0|channel,10,route->base_pan[channel]);route->pan_dirty[channel]=0;
    }
    if(pan>=0){hb_mo_push(route,0xb0|channel,10,pan);route->pan_dirty[channel]=1;}
    route->refs[channel][pitch]++;
    return hb_mo_push(route,0x90|channel,pitch,velocity);
}
/* Generated notes use the monotonic DSP beat, independent of transport seeks.
   Repeats from different lanes add; they never pass through input processing. */
static int hb_mo_repeat_active(const hb_motion_config *config,int lane,int manual,unsigned revision,unsigned held_serial){
    if(manual==2)return 1; /* A recorded burst owns its captured settings. */
    return config->revision[lane]==revision&&hb_mo_lane_active(config,lane)&&
        (!manual||((config->held&(1ULL<<lane))&&config->held_serial[lane]==held_serial));
}
static void hb_mo_repeat_cancel(hb_motion_route *route,const hb_motion_config *config,int all){
    if(!route->repeat_pending)return;
    int pending=0;
    for(int index=0;index<HB_MOTION_BURSTS;index++){
        hb_motion_burst *burst=&route->bursts[index];
        if(burst->used&&(all||!hb_mo_repeat_active(config,burst->lane,burst->manual,burst->revision,burst->held_serial)))burst->used=0;
        pending|=burst->used;
    }
    for(int index=0;index<HB_MOTION_OWNERS;index++){
        hb_motion_owner *owner=&route->owners[index];
        if(owner->used&&owner->generated&&(all||!hb_mo_repeat_active(config,owner->lane,owner->manual,owner->revision,owner->held_serial)))
            if(hb_mo_release(route,owner)){owner->used=0;route->owned--;}
        pending|=owner->used&&(owner->generated||(owner->sounding&&owner->repeat_off>0));
    }
    route->repeat_pending=pending;
}
static void hb_mo_repeat_schedule(hb_motion_route *route,const hb_motion_config *config,
                                const uint8_t message[3],int pitch,int velocity,double pattern_beat,double condition_beat,double now){
    for(int lane=0;lane<HB_MOTION_LANES;lane++){
        hb_motion_lane resolved=hb_mo_settings(config,lane);const hb_motion_lane *settings=&resolved;double value;
        if((settings->operation!=HB_MO_RATCHET&&settings->operation!=HB_MO_ECHO)||!hb_mo_value_at(config,lane,pattern_beat,condition_beat,message[1],&value))continue;
        int ratchet=settings->operation==HB_MO_RATCHET;
        int count=hb_mo_clamp(hb_mo_round(value),ratchet?1:0,16);
        int remaining=count-ratchet;if(remaining<=0)continue;
        int slot=-1;for(int index=0;index<HB_MOTION_BURSTS;index++)if(!route->bursts[index].used){slot=index;break;}
        if(slot<0)continue; /* Overload leaves the original note intact. */
        double spacing=hb_mo_grid(settings->grid)/(ratchet?count:1);
        route->bursts[slot]=(hb_motion_burst){.used=1,.lane=lane,.channel=message[0]&15,.pitch=pitch,.velocity=velocity,
            .remaining=remaining,.manual=hb_mo_recorded_word(config,lane)?2:(config->held&(1ULL<<lane))!=0,.decay=ratchet?0:hb_mo_clamp(settings->offset,0,100),
            .revision=config->revision[lane],.held_serial=config->held_serial[lane],.next=now+spacing,.spacing=spacing,.gate=spacing*0.5};
        route->repeat_pending=1;
        if(ratchet)for(int index=0;index<HB_MOTION_OWNERS;index++){
            hb_motion_owner *owner=&route->owners[index];
            if(owner->used&&!owner->generated&&owner->serial==route->next_serial-1){
                double deadline=now+spacing*0.5;
                if(owner->repeat_off<=0||deadline<owner->repeat_off)owner->repeat_off=deadline;
                break;
            }
        }
    }
}
static void hb_mo_repeat_tick(hb_motion_route *route,const hb_motion_config *config,double now){
    hb_mo_repeat_cancel(route,config,0);
    if(!route->repeat_pending)return;
    for(int index=0;index<HB_MOTION_OWNERS;index++){
        hb_motion_owner *owner=&route->owners[index];
        if(owner->used&&!owner->generated&&owner->repeat_off>0&&now+1e-9>=owner->repeat_off)hb_mo_release(route,owner);
        if(owner->used&&owner->generated&&now+1e-9>=owner->off_beat&&hb_mo_release(route,owner)){owner->used=0;route->owned--;}
    }
    /* Earliest deadline first, at most one attack per burst per audio block. */
    for(int emitted=0;emitted<HB_MOTION_BURSTS;emitted++){
        hb_motion_burst *burst=0;
        for(int index=0;index<HB_MOTION_BURSTS;index++){
            hb_motion_burst *candidate=&route->bursts[index];
            if(candidate->used&&candidate->next<=now+1e-9&&(!burst||candidate->next<burst->next))burst=candidate;
        }
        if(!burst)break;
        burst->velocity=hb_mo_round(burst->velocity*(100-burst->decay)/100.0);
        int slot=-1;for(int index=0;index<HB_MOTION_OWNERS;index++)if(!route->owners[index].used){slot=index;break;}
        if(burst->velocity>0&&slot>=0&&route->count<HB_MOTION_QUEUE-1){
            hb_motion_owner *owner=&route->owners[slot];
            *owner=(hb_motion_owner){.used=1,.channel=burst->channel,.source=-1,.pitch=burst->pitch,.sounding=1,
                .off_beat=now+burst->gate,.serial=route->next_serial++,.generated=1,.lane=burst->lane,.manual=burst->manual,
                .revision=burst->revision,.held_serial=burst->held_serial};
            route->owned++;route->refs[owner->channel][owner->pitch]++;
            hb_mo_push(route,0x90|owner->channel,owner->pitch,burst->velocity);
        }
        burst->remaining--;burst->next+=burst->spacing;
        /* Drop missed attacks instead of dumping a late catch-up burst. */
        while(burst->remaining>0&&burst->next<=now+1e-9){
            burst->remaining--;burst->next+=burst->spacing;
            burst->velocity=hb_mo_round(burst->velocity*(100-burst->decay)/100.0);
        }
        if(burst->remaining<=0||burst->velocity<=0)burst->used=0;
    }
}
#endif
