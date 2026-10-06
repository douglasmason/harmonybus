#ifndef HB_MOTIF_H
#define HB_MOTIF_H
/* Bounded, untimed motif recorder. Duration units are 1/24 beat (triplet-safe).
   No wall clock enters this layer. Drafts become visible only on commit. */
#define HB_MT_SLOTS 16
#define HB_MT_STEPS 32
#define HB_MT_VOICES 8
#define HB_MT_QUEUE 8
#define HB_MT_SCHEDULE 256
/* AUTO infers scale steps if both pitches belong to the captured collection;
   otherwise retains exact semitones. Explicit modifiers are stored separately. */
enum { HB_MT_AUTO, HB_MT_CHROMATIC, HB_MT_SCALE };
typedef struct { unsigned char pitch,velocity; } hb_mt_note;
typedef struct {
    int kind,duration,count,relation,modifier,secondary,cadence,chord_mode;
    unsigned scale,flags;
    unsigned long long actions[HB_MOTION_LANES+1];
    hb_mt_note notes[HB_MT_VOICES];
} hb_mt_event;
typedef struct { int count,anchor;hb_mt_event events[HB_MT_STEPS+2];int reference_valid,reference_pitch; int timed;double durations[HB_MT_STEPS+2]; } hb_mt_phrase;
enum { HB_MT_TARGET_START=1, HB_MT_TARGET_END=2 };
/* Canonical motif: body never contains the boundary target. Opening and
   closing target settings are independent of its transposition reference. */
typedef struct {
    hb_mt_phrase body;
    hb_mt_event target_start,target_end;
    int placement,reference_pitch,reference_step;
    /* Old clips address event indices. Keep their index/timing map without
       duplicating any note or operation data in the normalized library. */
    int legacy_count,legacy_anchor;
    unsigned char legacy_event[HB_MT_STEPS];
    unsigned short legacy_duration[HB_MT_STEPS];
} hb_mt_definition;
typedef struct {
    int lane,selected,armed,recording,duration,relation,chord_entry,anchor_next,open;
    int arrival,late,span,target,error,cursor,open_step,changed,undo_valid;
    int playback,preset,tap_grid,completion,placement;
    unsigned char held[16][128],swallow[16][128];
    hb_mt_phrase draft,undo;
} hb_mt_recorder;
typedef struct {double on,off;int pitch,velocity,channel,render,started,used;int pad_owner,pad_playback;
    unsigned override_mask;int override_root,override_semantic;
    unsigned long long override_generation;
    unsigned short trail_target;
    unsigned short semantic_mask,intent_scale;
    int reference_pitch,root_pc,intent_kind,intent_target,intent_minor;
} hb_mt_scheduled;
typedef struct {
    hb_mt_recorder editor;
    int playback_lane;
    hb_mt_scheduled events[HB_MT_SCHEDULE];
    hb_mt_phrase tap_phrase;
    int tap_active,tap_step,tap_input,tap_channel,tap_velocity,tap_target,tap_rhythm,tap_span;
    double tap_arrival,tap_last_due;
    hb_harmony_t tap_harmony;
    double last_beat;int have_beat,was_running,pending,cancel,flash_serial,flash_pitch,flash_step;
} hb_mt_runtime;
static hb_mt_definition g_motifs[HB_MT_SLOTS];
static int g_motifs_restored;
static int g_motif_rhythm,g_motif_span,g_motif_timing_restored;
static const int HB_MT_DURATIONS[]={3,4,6,8,9,12,16,18,24,36,48,72,96};
static const char *HB_MT_DURATION_NAMES[]={"1/32","1/16T","1/16","1/8T","1/16.","1/8","1/4T","1/8.","1/4","1/4.","1/2","1/2.","1 Bar"};
static const char *HB_MT_ARRIVALS[]={"Now","Next Beat","Next Bar","Next Harmony","2 Bars","4 Bars"};
static const char *HB_MT_TARGETS[]={"Played","Root","Third","Fifth","Seventh","Sus2","Sus4"};
static const char *HB_MT_LATE[]={"Trim","Fit","Defer"};
static const char *HB_MT_RELATIONS[]={"Auto","Chromatic","Scale"};
static const char *HB_MT_PLAYBACK[]={"Automatic","Tap Free","Tap Guided","Tap Grid"};
static const char *HB_MT_COMPLETION[]={"Manual","Auto Finish"};
static const char *HB_MT_PLACEMENTS[]={"Saved","End","Start","Both","Omit"};
static int hb_mt_placement(int choice,int saved){
    static const int placements[]={0,HB_MT_TARGET_END,HB_MT_TARGET_START,3,0};
    return choice>0&&choice<5?placements[choice]:saved;
}
static const char *HB_MT_GRIDS[]={"1/32","1/16","1/8","1/4"};
static const char *HB_MT_PRESETS[]={"Library","V-Target","ii-V-Target","iv-bVII-Target","bII7-Target","ii-bII7-Target",
    "bVI-bVII-I","bVI-V-I","bIII-IV-I","vi-V-I","iii-vi-ii-V-I","IV-iv-I","ii halfdim-V-i","I-VI7-ii-V-I","V/V-V-I","ii/V-V/V-V-I","V/ii-ii-V-I","V/vi-vi-ii-V-I","vii dim/V-V-I","III7-VI7-II7-V7-I","vi-ii-V","ii-V-LT","iv-bVII-LT","ii-bII7-LT","iii-vi-ii","IV-ii-V","vii-iii-vi","LT-ii-V"};
#define HB_MT_PRESET_COUNT 28
static int hb_mt_reference_preset(int reference){return reference>0&&reference<20?reference:reference>=36&&reference<44?reference-16:0;}
/* Legacy cadence IDs remain stable for existing clips; presets reference
   those same semantic steps instead of storing rendered pitches. */
static void hb_mt_preset_definition(int preset,hb_mt_definition *definition){
    memset(definition,0,sizeof(*definition));
    if(preset<1||preset>=HB_MT_PRESET_COUNT)return;
    hb_mt_phrase *phrase=&definition->body;
    hb_mt_event target={.duration=24,.count=1,.chord_mode=3,.scale=0xAB5,.notes={{60,100}}};
    definition->target_start=definition->target_end=target;definition->reference_pitch=60;
    if(preset>=20){
        static const int roles[8][3]={{4,1,2},{1,2,-1},{5,6,-1},{1,-2,-1},{8,4,1},{9,1,2},{10,8,4},{-1,1,2}};
        phrase->count=3;phrase->anchor=2;
        for(int step=0;step<3;step++){hb_mt_event *event=&phrase->events[step];int role=roles[preset-20][step];*event=target;if(role<0)event->modifier=role==-1?-1:2;else event->secondary=role;event->actions[HB_MOTION_LANES]=role==-1?1:role==-2?3:hb_mo_role_word(role);}definition->reference_step=2;return;
    }
    static const int simple[][3]={{2,3,0},{1,2,3},{5,6,3},{-2,3,0},{1,-2,3}};
    int count=preset>=6?HB_CADENCES[preset-6].length:(preset==1||preset==4?2:3);
    for(int step=0;step<count;step++){
        hb_mt_event entry=target,*event=&entry;int is_target=0;
        if(preset>=6){event->cadence=(preset-6)*HB_CADENCE_STEPS+step+1;event->actions[HB_MOTION_LANES]=(unsigned long long)event->cadence<<13;
            int kind=HB_CADENCES[preset-6].steps[step].kind;is_target=kind==HB_CAD_TARGET||kind==HB_CAD_MINOR_TARGET;}
        else {int role=simple[preset-1][step];if(role<0)event->modifier=2;else event->secondary=role;
            event->actions[HB_MOTION_LANES]=role<0?3:hb_mo_role_word(role);is_target=role==3;}
        if(is_target&&step==0){definition->target_start=entry;definition->placement|=HB_MT_TARGET_START;}
        else if(is_target&&step==count-1){definition->target_end=entry;definition->placement|=HB_MT_TARGET_END;}
        else phrase->events[phrase->count++]=entry;
    }
    /* A single specified resolution supplies either placement. Both-ended
       motifs retain each boundary's own captured settings. */
    if(definition->placement==HB_MT_TARGET_END)definition->target_start=definition->target_end;
    if(definition->placement==HB_MT_TARGET_START)definition->target_end=definition->target_start;
    phrase->anchor=phrase->count-1;definition->reference_step=phrase->anchor;
}
/* Compatibility view for the existing timed player/editor. It is generated
   from one normalized definition, never a second stored motif version. */
static int hb_mt_materialize(const hb_mt_definition *definition,int placement,hb_mt_phrase *phrase){
    if(placement<0||placement>3||definition->body.count<0||definition->body.count>HB_MT_STEPS)return 0;
    int count=definition->body.count+!!(placement&HB_MT_TARGET_START)+!!(placement&HB_MT_TARGET_END);
    if(count>HB_MT_STEPS+2)return 0;
    hb_mt_phrase result={.reference_valid=1,.reference_pitch=definition->reference_pitch};
    if(placement&HB_MT_TARGET_START)result.events[result.count++]=definition->target_start;
    int offset=result.count;
    for(int index=0;index<definition->body.count;index++)result.events[result.count++]=definition->body.events[index];
    if(placement&HB_MT_TARGET_END){result.anchor=result.count;result.events[result.count++]=definition->target_end;}
    else result.anchor=placement&HB_MT_TARGET_START?0:
        definition->reference_step>=0?offset+definition->reference_step:result.count;
    *phrase=result;return 1;
}
static void hb_mt_preset(int preset,hb_mt_phrase *phrase){
    hb_mt_definition definition;hb_mt_preset_definition(preset,&definition);
    memset(phrase,0,sizeof(*phrase));hb_mt_materialize(&definition,definition.placement,phrase);
}
/* Compare stored input identity, never the currently rendered voicing.
   Velocity/duration are deliberately excluded and retained on extraction. */
static int hb_mt_is_target(const hb_mt_event *event,const hb_mt_event *reference){
    if(event->kind||!event->count||event->modifier)return 0;
    if(event->cadence){
        const hb_cadence_step *cadence=hb_cadence_decode(event->cadence);
        if(!cadence||(cadence->kind!=HB_CAD_TARGET&&cadence->kind!=HB_CAD_MINOR_TARGET))return 0;
    }else if(event->secondary&&event->secondary!=3)return 0;
    /* Recorded approach tokens are not neutral targets, even when the
       stored input pad equals the transposition reference. */
    unsigned long long intent=event->actions[HB_MOTION_LANES];
    if((intent&3)||((intent>>43)&2047))return 0;
    int role=(int)(((intent>>4)&7)|((intent>>5)&8)|((intent>>59)&16));
    if(role&&role!=3)return 0;
    const hb_cadence_step *recorded_cadence=hb_cadence_decode((unsigned)((intent>>13)&127));
    if(recorded_cadence&&recorded_cadence->kind!=HB_CAD_TARGET&&recorded_cadence->kind!=HB_CAD_MINOR_TARGET)return 0;
    if(event->count!=reference->count)return 0;
    unsigned matched=0;
    for(int voice=0;voice<event->count;voice++){
        int found=0;
        for(int other=0;other<reference->count;other++)if(!(matched&(1u<<other))&&event->notes[voice].pitch==reference->notes[other].pitch){matched|=1u<<other;found=1;break;}
        if(!found)return 0;
    }
    return 1;
}
/* Import/editor commit boundary: remove only leading/trailing target blocks.
   Interior targets and approach events remain in original order. A single
   target-only phrase defaults to End, never two duplicated target attacks. */
static int hb_mt_normalize(const hb_mt_phrase *phrase,hb_mt_definition *output){
    if(!phrase||!output||phrase->count<1||phrase->count>HB_MT_STEPS||
       phrase->anchor<0||phrase->anchor>=phrase->count)return 0;
    const hb_mt_event *reference=&phrase->events[phrase->anchor];
    if(reference->kind||reference->count<1||reference->count>HB_MT_VOICES)return 0;
    for(int index=0;index<phrase->count;index++){
        const hb_mt_event *event=&phrase->events[index];
        if(event->duration<1||event->duration>1536||event->kind<0||event->kind>2||
           event->count<0||event->count>HB_MT_VOICES||(!event->kind&&!event->count)||
           (event->kind&&event->count)||(event->kind==2&&(!index||phrase->events[index-1].kind==1)))return 0;
    }
    hb_mt_definition definition={0};
    definition.reference_pitch=reference->notes[0].pitch;definition.reference_step=-1;
    hb_mt_event target={.duration=reference->duration,.count=reference->count,
        .relation=reference->relation,.chord_mode=reference->chord_mode,.scale=reference->scale};
    memcpy(target.notes,reference->notes,sizeof(target.notes));
    definition.target_start=definition.target_end=target;
    int first=0,last=phrase->count;
    while(first<last){
        int tail=last-1;while(tail>first&&phrase->events[tail].kind==2)tail--;
        if(!hb_mt_is_target(&phrase->events[tail],reference))break;
        int duration=0;for(int index=tail;index<last;index++)duration+=phrase->events[index].duration;
        if(!(definition.placement&HB_MT_TARGET_END)){definition.target_end=phrase->events[tail];definition.target_end.duration=0;}
        definition.target_end.duration+=duration;
        definition.placement|=HB_MT_TARGET_END;last=tail;
    }
    while(first<last&&hb_mt_is_target(&phrase->events[first],reference)){
        if(!(definition.placement&HB_MT_TARGET_START)){definition.target_start=phrase->events[first];definition.target_start.duration=0;}
        definition.target_start.duration+=phrase->events[first++].duration;
        while(first<last&&phrase->events[first].kind==2)definition.target_start.duration+=phrase->events[first++].duration;
        definition.placement|=HB_MT_TARGET_START;
    }
    for(int index=first;index<last;index++){
        if(index==phrase->anchor)definition.reference_step=definition.body.count;
        definition.body.events[definition.body.count++]=phrase->events[index];
    }
    definition.body.anchor=definition.reference_step;
    definition.legacy_count=phrase->count;definition.legacy_anchor=phrase->anchor;
    for(int index=0;index<phrase->count;index++){
        definition.legacy_duration[index]=(unsigned short)phrase->events[index].duration;
        definition.legacy_event[index]=(unsigned char)(index<first?
            (phrase->events[index].kind==2?34:32):index>=last?
            (phrase->events[index].kind==2?34:33):index-first);
    }
    if(definition.placement==HB_MT_TARGET_END)definition.target_start=definition.target_end;
    if(definition.placement==HB_MT_TARGET_START)definition.target_end=definition.target_start;
    *output=definition;return 1;
}
static int hb_mt_occupied(const hb_mt_definition *definition){return definition->target_end.count>0;}
static void hb_mt_library_view(int slot,hb_mt_phrase *phrase){
    memset(phrase,0,sizeof(*phrase));
    if(slot<0||slot>=HB_MT_SLOTS||!hb_mt_occupied(&g_motifs[slot]))return;
    const hb_mt_definition *definition=&g_motifs[slot];
    if(!definition->legacy_count){hb_mt_materialize(definition,definition->placement,phrase);return;}
    phrase->count=definition->legacy_count;phrase->anchor=definition->legacy_anchor;
    phrase->reference_valid=1;phrase->reference_pitch=definition->reference_pitch;
    for(int index=0;index<phrase->count;index++){
        int event=definition->legacy_event[index];
        if(event<32)phrase->events[index]=definition->body.events[event];
        else if(event==32)phrase->events[index]=definition->target_start;
        else if(event==33)phrase->events[index]=definition->target_end;
        else phrase->events[index].kind=2;
        phrase->events[index].duration=definition->legacy_duration[index];
    }
}
static const char *HB_MT_SPANS[]={"As Entered","Half","Double"};
static const char *HB_MT_RHYTHMS[]={"As Entered","Even","Long-Short","Short-Long","Accelerate","Decelerate"};
/* Normalize each side of the anchor independently. Redistribute duration,
   preserving both the anchor offset and the tail length before global span. */
static double hb_mt_weight(int rhythm,int index,int count){
    if(rhythm==2)return index%2?1:3;
    if(rhythm==3)return index%2?3:1;
    if(rhythm==4)return count-index;
    if(rhythm==5)return index+1;
    return 1;
}
static double hb_mt_duration(const hb_mt_phrase *phrase,int step,int rhythm,int span){
    double scale=span==1?.5:span==2?2:1;
    if(phrase->timed)return phrase->durations[step]*scale;
    if(!rhythm)return phrase->events[step].duration/24.0*scale;
    int begin=step<phrase->anchor?0:phrase->anchor;
    int end=step<phrase->anchor?phrase->anchor:phrase->count;
    double stored=0,weights=0;
    for(int index=begin;index<end;index++){
        stored+=phrase->events[index].duration/24.0;
        weights+=hb_mt_weight(rhythm,index-begin,end-begin);
    }
    return weights>0?stored*hb_mt_weight(rhythm,step-begin,end-begin)/weights*scale:0;
}
/* Padding is a held target/rest, not a fresh attack or a new rhythm pass. */
static void hb_mt_pad_even(hb_mt_phrase *phrase,int rhythm,int placement){
    if(!phrase->count||phrase->timed)return;
    int units=0;double total=0;
    for(int i=0;i<phrase->count;i++){units+=phrase->events[i].duration;phrase->durations[i]=hb_mt_duration(phrase,i,rhythm,0);total+=phrase->durations[i];}
    /* Rhythm preserves duration. Round stored 1/24-beat units to an even
       beat without a libc math dependency in the freestanding device build. */
    double padding=((units+47)/48)*2.0-total;if(padding<0.000001)return;
    int target=placement&HB_MT_TARGET_END?phrase->count-1:placement&HB_MT_TARGET_START?0:-1;
    if(target<0){
        if(phrase->count>=HB_MT_STEPS+2)return;
        target=phrase->count++;phrase->events[target]=(hb_mt_event){.kind=1,.duration=24};phrase->durations[target]=0;
    }
    phrase->durations[target]+=padding;phrase->timed=1;
}
static void hb_mt_init(hb_mt_runtime *runtime){
    memset(runtime,0,sizeof(*runtime));runtime->editor.recording=-1;
    runtime->playback_lane=-1;runtime->editor.lane=-1;runtime->editor.armed=-1;runtime->editor.duration=2;runtime->editor.arrival=2;
    runtime->editor.draft.anchor=-1;runtime->editor.tap_grid=1;
}
static int hb_mt_held(const hb_mt_recorder *editor){
    for(int channel=0;channel<16;channel++)for(int pitch=0;pitch<128;pitch++)if(editor->held[channel][pitch])return 1;
    return 0;
}
static void hb_mt_lane_load(hb_mt_recorder *editor,const hb_motion_lane *lane){
    editor->preset=hb_mt_reference_preset(lane->amount);
    editor->selected=lane->amount>=20&&lane->amount<36?lane->amount-20:0;
    editor->playback=lane->motif_playback;editor->arrival=lane->motif_arrival;
    editor->target=lane->motif_target;editor->late=lane->motif_late;
    editor->tap_grid=lane->motif_grid;editor->completion=lane->motif_completion;
}
static void hb_mt_begin(hb_mt_recorder *editor,int slot){
    if(slot<0||slot>=HB_MT_SLOTS||editor->recording>=0)return;
    hb_mt_library_view(slot,&editor->draft);if(!editor->draft.count)editor->draft.anchor=-1;
    editor->cursor=0;editor->changed=editor->undo_valid=0;
    memset(editor->held,0,sizeof(editor->held));editor->open=0;
    editor->recording=slot;editor->selected=slot;editor->armed=-1;editor->anchor_next=0;editor->error=0;
}
static int hb_mt_finish(hb_mt_recorder *editor,int commit){
    int slot=editor->recording,saved=0;
    if(slot>=0&&commit&&editor->changed&&editor->draft.count){
        int anchor=editor->draft.anchor;
        if(anchor>=editor->draft.count||(anchor>=0&&!editor->draft.events[anchor].count))anchor=-1;
        for(int step=0;step<editor->draft.count;step++)if(editor->draft.events[step].kind==2&&(!step||editor->draft.events[step-1].kind==1))editor->draft.events[step].kind=1;
        if(anchor<0)for(int step=editor->draft.count-1;step>=0;step--)if(editor->draft.events[step].count){anchor=step;break;}
        int total=editor->draft.count;for(int index=0;index<HB_MT_SLOTS;index++)if(index!=slot)total+=g_motifs[index].body.count+!!(g_motifs[index].placement&1)+!!(g_motifs[index].placement&2);
        if(total>128){editor->error=9;return 0;}
        if(anchor>=0){editor->draft.anchor=anchor;if(!hb_mt_normalize(&editor->draft,&g_motifs[slot])){editor->error=9;return 0;}saved=1;g_motifs_restored=1;}
    }
    editor->recording=-1;editor->open=0;editor->anchor_next=0;
    memset(editor->held,0,sizeof(editor->held));return saved;
}
static void hb_mt_checkpoint(hb_mt_recorder *editor){editor->undo=editor->draft;editor->undo_valid=1;editor->changed=1;editor->error=0;}
static hb_mt_event *hb_mt_entry(hb_mt_recorder *editor){
    if(editor->cursor>=HB_MT_STEPS){editor->error=1;return 0;}
    hb_mt_checkpoint(editor);
    while(editor->draft.count<=editor->cursor){hb_mt_event *gap=&editor->draft.events[editor->draft.count++];memset(gap,0,sizeof(*gap));gap->kind=1;gap->duration=HB_MT_DURATIONS[editor->duration];}
    return &editor->draft.events[editor->cursor];
}
static int hb_mt_add(hb_mt_recorder *editor,int channel,int pitch,int velocity,const hb_mt_event *intent){
    if(editor->recording<0)return 0;
    editor->held[channel][pitch]=1;
    if(!editor->open){
        hb_mt_event *event=hb_mt_entry(editor);if(!event)return 0;*event=*intent;
        event->kind=0;event->count=0;event->duration=HB_MT_DURATIONS[editor->duration];event->relation=editor->relation;
        editor->open=1;editor->open_step=editor->cursor;
        if(editor->anchor_next){editor->draft.anchor=editor->cursor;editor->anchor_next=0;}
    }
    hb_mt_event *event=&editor->draft.events[editor->open_step];
    for(int voice=0;voice<event->count;voice++)if(event->notes[voice].pitch==pitch)return 1;
    if(event->count>=HB_MT_VOICES){editor->error=2;return 0;}
    event->notes[event->count++]=(hb_mt_note){(unsigned char)pitch,(unsigned char)velocity};return 1;
}
static void hb_mt_next(hb_mt_recorder *editor){
    if(editor->open){editor->cursor=editor->open_step+1;while(editor->cursor<editor->draft.count&&editor->draft.events[editor->cursor].kind==2)editor->cursor++;editor->open=0;}
}
static void hb_mt_release(hb_mt_recorder *editor,int channel,int pitch){
    editor->held[channel][pitch]=0;
    if(!editor->chord_entry&&!hb_mt_held(editor))hb_mt_next(editor);
}
static void hb_mt_rest(hb_mt_recorder *editor){
    if(editor->recording<0)return;
    hb_mt_next(editor);hb_mt_event *event=hb_mt_entry(editor);if(!event)return;
    memset(event,0,sizeof(*event));event->kind=1;
    event->duration=HB_MT_DURATIONS[editor->duration];editor->cursor++;
}
static void hb_mt_tie(hb_mt_recorder *editor){
    if(editor->recording<0)return;
    hb_mt_next(editor);
    if(!editor->cursor||editor->draft.events[editor->cursor-1].kind==1){editor->error=3;return;}
    hb_mt_event *event=hb_mt_entry(editor);if(!event)return;
    memset(event,0,sizeof(*event));event->kind=2;
    event->duration=HB_MT_DURATIONS[editor->duration];editor->cursor++;
}
static int hb_mt_steps_between(int first,int last,unsigned scale){
    int steps=0,direction=last>=first?1:-1;
    for(int pitch=first;pitch!=last;){pitch+=direction;if(scale&(1u<<hb_cp_mod(pitch)))steps+=direction;}
    return steps;
}
static int hb_mt_walk(int pitch,int steps,unsigned scale){
    if(!scale)return pitch;
    int direction=steps>=0?1:-1,remaining=steps>=0?steps:-steps;
    while(remaining&&pitch>-128&&pitch<255){pitch+=direction;if(scale&(1u<<hb_cp_mod(pitch)))remaining--;}
    return pitch;
}
static int hb_mt_relative(const hb_mt_event *event,int pitch,int anchor,int target,unsigned scale){
    int use_scale=event->relation==HB_MT_SCALE||(event->relation==HB_MT_AUTO&&
        (event->scale&(1u<<hb_cp_mod(pitch)))&&(event->scale&(1u<<hb_cp_mod(anchor))));
    return use_scale?hb_mt_walk(target,hb_mt_steps_between(anchor,pitch,event->scale),scale):target+pitch-anchor;
}
#endif
