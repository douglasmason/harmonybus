#ifndef HB_KEY_CONTEXT_H
#define HB_KEY_CONTEXT_H
/* Render-only, shared musical context. Source degrees and classifier evidence
   stay in the recorded key. Onset snapshots own their eventual note-offs. */
typedef struct { int active,source_root,target_root; unsigned source_mask,target_mask; } hb_key_context;
static hb_key_context g_key_context;
static int g_key_scale_mode=1,g_key_conductor_travel,g_key_settings_restored,g_parallel_manual,g_key_lane_parallel;
static int g_key_armed,g_parallel_scale=2,g_parallel_on,g_parallel_latch;
static hb_key_context g_parallel_previous;
static int hb_key_mod(int pitch){int value=pitch%12;return value<0?value+12:value;}
static int hb_key_map(hb_key_context context,int pitch){
    if(!context.active||!context.source_mask||!context.target_mask)return pitch;
    int source[12],target[12],source_count=0,target_count=0;
    for(int interval=0;interval<12;interval++){
        if(context.source_mask&(1u<<hb_key_mod(context.source_root+interval)))source[source_count++]=interval;
        if(context.target_mask&(1u<<hb_key_mod(context.target_root+interval)))target[target_count++]=interval;
    }
    int relative=pitch-context.source_root,octave=relative/12;
    if(relative<0&&relative%12)octave--;
    int interval=hb_key_mod(relative),degree=0,distance=99,alteration=0;
    for(int index=0;index<source_count;index++){
        int delta=interval-source[index],absolute=delta<0?-delta:delta;
        /* Equal-distance chromatic pitches retain their lower-degree sharp. */
        if(absolute<distance){degree=index;distance=absolute;alteration=delta;}
    }
    int mapped=context.target_root+octave*12+target[degree%target_count]+alteration;
    while(mapped-pitch>6)mapped-=12;while(mapped-pitch< -6)mapped+=12;
    while(mapped<0)mapped+=12;while(mapped>127)mapped-=12;return mapped;
}
static unsigned hb_key_mask(hb_key_context context,unsigned mask){
    unsigned mapped=0;for(int pitch=0;pitch<12;pitch++)if(mask&(1u<<pitch))mapped|=1u<<hb_key_mod(hb_key_map(context,60+pitch));return mapped;
}
#endif
