/* Harmony Bus v0.2.136 — Schwung MIDI FX. */
#define HB_VERSION "0.2.262"
#ifdef HB_FREESTANDING
typedef __SIZE_TYPE__ size_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef signed int int32_t;
typedef struct _IO_FILE FILE;
extern int snprintf(char *, size_t, const char *, ...);
extern int sscanf(const char *, const char *, ...);
extern void *memset(void *, int, size_t);
extern void *memcpy(void *, const void *, size_t);
extern int memcmp(const void *, const void *, size_t);
extern size_t strlen(const char *);
extern int strcmp(const char *, const char *);
extern int strncmp(const char *, const char *, size_t);
extern char *strchr(const char *, int);
extern long strtol(const char *, char **, int);
extern unsigned long long strtoull(const char *, char **, int);
extern double strtod(const char *, char **);
extern void *malloc(size_t);
extern void free(void *);
extern FILE *fopen(const char *, const char *);
extern int fclose(FILE *);
extern size_t fread(void *, size_t, size_t, FILE *);
extern size_t fwrite(const void *, size_t, size_t, FILE *);
extern int fseek(FILE *, long, int);
extern long ftell(FILE *);
extern char *fgets(char *, int, FILE *);
extern char *strstr(const char *, const char *);
extern int shm_open(const char *, int, unsigned);
extern int ftruncate(int, long);
extern void *mmap(void *, size_t, int, int, int, long);
extern int close(int);
#else
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#endif
#include "schwung_midi_api.h"
#include "../../../src/harmony_core.h"
#include "../../../src/scale_catalog.h"
#include "../../../src/approach_state.h"
#include "../../../src/closest_split.h"
#include "../../../src/approach_pitch.h"
#include "../../../src/follower_timing.h"
#include "../../../src/movy_loop.h"
#include "../../../src/chord_player.h"
#include "../../../src/role_policy.h"
#include "../../../src/render_rhythm.h"
#include "../../../src/motion.h"
#include "../../../src/follower_play.h"
#include "../../../src/motif.h"
#include "../../../src/motif_clip.h"

struct host_api_v1 {
    uint32_t api_version;
    int sample_rate;
    int frames_per_block;
    uint8_t *mapped_memory;
    int audio_out_offset;
    int audio_in_offset;
    void (*log)(const char *msg);
    int (*midi_send_internal)(const uint8_t *msg, int len);
    int (*midi_send_external)(const uint8_t *msg, int len);
    int (*get_clock_status)(void);
    void *mod_emit_value;
    void *mod_clear_source;
    void *mod_host_ctx;
    float (*get_bpm)(void);
    int (*midi_inject_to_move)(const uint8_t *msg, int len);
    int (*slot_recv_channel)(void *instance);
    double (*get_beat_position)(void);
};
static const host_api_v1_t *g_host = 0;
#define HB_MIDI_OUT_BYTES 80

#define HB_MAX_INSTANCES 16
#define HB_MAX_CLIP_NOTES 1024
#define HB_MAX_LOOP_HARMONIES 64
#define HB_MONITOR_MAGIC 0x48424d31u
#define HB_MONITOR_VERSION 2u
#define HB_MONITOR_SHM "/harmonybus-monitor-v2"
#define HB_O_RDWR 2
#define HB_O_CREAT 64
#define HB_PROT_READ 1
#define HB_PROT_WRITE 2
#define HB_MAP_SHARED 1
#define HB_MAP_FAILED ((void *)-1)
typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile uint32_t seq;
    volatile uint32_t generation;
    volatile uint32_t external_note_events;
    volatile uint32_t realtime_events;
    volatile uint8_t playing;
    volatile uint8_t velocities[16][128];
    volatile uint8_t root_valid[16];
    volatile uint8_t root_pc[16];
    volatile uint8_t root_confirmations[16];
} hb_monitor_shared_t;
static hb_monitor_shared_t *g_monitor=0;
#define HB_GLOBAL_MAGIC 0x4842474Cu
#define HB_GLOBAL_VERSION 5u
#define HB_GLOBAL_SHM "/harmonybus-global-v5"
typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile uint32_t seq;
    volatile int follower_root_policy;
    volatile int follower_explicit_root;
    volatile int follower_content_map;
    volatile int follower_travel_map;
    volatile int follower_scale;
    volatile int dominant_scale,borrowed_scale;
    volatile int role_ready,role_values[2][HB_POLICY_FIELDS];
} hb_global_shared_t;
static hb_global_shared_t *g_global_shared=0;
/* These controls are truly global. A per-instance restore guard is not
   sufficient because Schwung can recreate instances while navigating UI. */
static int g_follower_globals_restored=0;
static int g_scale_restored=0;
static int g_scale_fallback=1;
static unsigned g_infer_revision=0,g_infer_cached_revision=0;
static int g_infer_cached_root=-1,g_infer_cached_transpose=0,g_infer_cached_count=-1;
static unsigned g_infer_cached_observed=0;
static int g_infer_scale=1,g_infer_ambiguous=1,g_infer_has_evidence=0;
static int g_scale_exceptions[2]={6,0},g_scale_exceptions_restored=0;
static int g_pad_play_color=3;
static int g_pad_both_color=2;
static int g_pad_tonic_color=9;
static int g_pad_chord_form=HB_CP_FOLLOW_DETECTED;
static int g_pad_next_chord_form=HB_CP_FOLLOW_DETECTED;
static int g_pad_next_pulse=0;
static int g_pad_adjacent_shading=0;
static const char *PAD_NEXT_PULSE[]={"None","3","7","3+7","1","5","1+5","9","11","13","9+11+13","All"};
static const unsigned PAD_NEXT_ROLES[]={0,4,64,68,1,16,17,2,8,32,42,127};
static int g_pad_settings[5]={6,3,3,2,0};
static int g_pad_restored=0;
static int g_humanize[3]={0,0,0},g_humanize_restored=0;
static const char *PAD_KEYS[]={"pad_display","pad_pulse_rate","pad_pulse_shape","pad_current_color","pad_lookahead_color"};
static const int PAD_LIMITS[]={8,8,4,9,9};
static const char *PAD_MODES[]={"Effective","Current","Effective","Both","Lookahead","Full Lookahead","Both Full Lookahead","Harmony Off"};
static const char *PAD_RATES[]={"Off","1/16","1/8","1/4","1/2","1 Bar","2 Bars","4 Bars"};
static const char *PAD_SHAPES[]={"Smooth","Triangle","Square","None"};
static const char *PAD_COLORS[]={"Red","Orange","Yellow","Green","Cyan","Blue","Purple","Pink","Track"};
static const char *PAD_PLAY_COLORS[]={"Red","Orange","Yellow","Green","Cyan","Blue","Purple","Pink","Track","Grey","White","Off"};
static const char *PAD_TONIC_COLORS[]={"Red","Orange","Yellow","Green","Cyan","Blue","Purple","Pink","Track","Grey"};
static const char *PAD_BOTH_COLORS[]={"Blend","Red","Orange","Yellow","Green","Cyan","Blue","Purple","Pink","Track"};
static const char **PAD_OPTIONS[]={PAD_MODES,PAD_RATES,PAD_SHAPES,PAD_COLORS,PAD_COLORS};
static void hb_pad_defaults(void){int defaults[5]={6,3,3,2,0};g_pad_play_color=3;g_pad_both_color=2;g_pad_tonic_color=9;g_pad_chord_form=HB_CP_FOLLOW_DETECTED;g_pad_next_chord_form=HB_CP_FOLLOW_DETECTED;g_pad_next_pulse=0;g_pad_adjacent_shading=0;memcpy(g_pad_settings,defaults,sizeof(defaults));g_pad_restored=0;}

static int g_buffer_restored=0;
static int g_lookahead_restored=0;
static const char *BUFFER_DIVISIONS[]={"1/64","1/32","1/16","1/8","1/4","1/2","1 Bar","2 Bars","4 Bars"};
static int mod12(int value);

static void hb_global_open(void){
    if(g_global_shared)return;
    int fd=shm_open(HB_GLOBAL_SHM,HB_O_RDWR|HB_O_CREAT,0666);
    if(fd<0)return;
    if(ftruncate(fd,(long)sizeof(hb_global_shared_t))!=0){close(fd);return;}
    void *ptr=mmap(0,sizeof(hb_global_shared_t),HB_PROT_READ|HB_PROT_WRITE,HB_MAP_SHARED,fd,0);
    close(fd);
    if(ptr==HB_MAP_FAILED)return;
    hb_global_shared_t *shared=(hb_global_shared_t*)ptr;
    if(shared->magic!=HB_GLOBAL_MAGIC||shared->version!=HB_GLOBAL_VERSION){
        memset(shared,0,sizeof(*shared));
        shared->magic=HB_GLOBAL_MAGIC;
        shared->version=HB_GLOBAL_VERSION;
        /* Default to inferred Move input key, never an undefined policy. */
        shared->follower_root_policy=0;
        shared->follower_explicit_root=0;
        shared->follower_scale=1;
    }
    g_global_shared=shared;
}
static int hb_global_root_policy(void){
    hb_global_open();
    return g_global_shared?g_global_shared->follower_root_policy:0;
}
static int hb_global_explicit_root(void){
    hb_global_open();
    return g_global_shared?g_global_shared->follower_explicit_root:0;
}
/* One input collection for every track, including independently loaded hosts. */
static int hb_shared_dominant_scale(void){
    hb_global_open();return g_global_shared?g_global_shared->dominant_scale:g_scale_exceptions[0];
}
static int hb_shared_borrowed_scale(void){
    hb_global_open();return g_global_shared?g_global_shared->borrowed_scale:g_scale_exceptions[1];
}
static void hb_store_scale_exceptions(int dominant,int borrowed){
    g_scale_exceptions[0]=dominant;g_scale_exceptions[1]=borrowed;
    hb_global_open();
    if(g_global_shared){g_global_shared->seq++;g_global_shared->dominant_scale=dominant;g_global_shared->borrowed_scale=borrowed;g_global_shared->seq++;}
}
static int hb_shared_follower_scale(void){
    hb_global_open();
    int scale=g_global_shared?g_global_shared->follower_scale:g_scale_fallback;
    return scale>=0&&scale<HB_SCALE_COUNT?scale:1;
}
static void hb_set_shared_follower_scale(int scale){
    if(scale<0||scale>=HB_SCALE_COUNT)return;
    g_scale_fallback=scale;
    hb_global_open();
    if(g_global_shared){
        g_global_shared->seq++;
        g_global_shared->follower_scale=scale;
        g_global_shared->seq++;
    }
    g_scale_restored=1;
}
static void hb_set_global_root_policy(int policy){
    hb_global_open();
    if(!g_global_shared)return;
    if(policy<0||policy>2)policy=0;
    g_global_shared->seq++;
    g_global_shared->follower_root_policy=policy;
    g_global_shared->seq++;
}
static void hb_set_global_explicit_root(int root){
    hb_global_open();
    if(!g_global_shared)return;
    root=mod12(root);
    g_global_shared->seq++;
    g_global_shared->follower_explicit_root=root;
    g_global_shared->seq++;
}
typedef struct {
    int note;
    double start;
    double duration;
} hb_clip_note_t;
typedef struct {
    double phase;
    hb_harmony_t harmony;
} hb_loop_harmony_event_t;
typedef struct { volatile unsigned seq; hb_harmony_t harmony; int global_transpose; int global_root_policy; int global_explicit_root; int global_input_root; int sensor_sources; int chord_timescale; int stability; int chord_timing; int quant_timing; int anticipation; int boundary_buffer_ms; int analysis_release_ms; int follower_content_map; int follower_travel_map; int follower_scale; int follower_split_map; int approach_control; int approach_mode; int inference_window_ms; int context; int accidentals; int auto_spell_sharps; int auto_spell_locked; int clip_track; int clip_slot; int clip_valid; int clip_note_count; int clip_stage; int clip_context; int last_clock_status; double clip_loop_start; double clip_loop_end; unsigned long clip_clock_ticks; unsigned clip_refresh_counter; double last_clip_playhead; int have_last_clip_playhead; int next_predict; int next_lookahead; int next_anti_buffer_ms; int next_model_locked; int next_shift_active; int next_learning_count; int next_model_count; double next_last_playhead; int next_have_playhead; int next_learning_started; double next_learning_progress_beats; hb_harmony_t observed_harmony; hb_loop_harmony_event_t next_learning[HB_MAX_LOOP_HARMONIES]; hb_loop_harmony_event_t next_model[HB_MAX_LOOP_HARMONIES]; unsigned cache_rev; unsigned sense_rev; int last_sense_count; uint8_t last_sense_notes[64]; unsigned global_process_count; unsigned global_note_event_count; unsigned global_accepted_note_count; unsigned global_tick_count; int global_last_status; int global_last_note; int global_last_channel; int global_last_instance; int follower_root_policy; int follower_explicit_root; hb_clip_note_t clip_notes[HB_MAX_CLIP_NOTES]; } SharedBus;
static SharedBus g_bus={.approach_control=HB_APPROACH_OFF,.approach_mode=0}; static int g_init=0;
typedef struct { uint8_t source,pitch,velocity,on; } hb_rx_event;
static int g_render_window=0,g_render_restored=0;
#include "../../../src/approach_rows.h"
#include "../../../src/key_context.h"
#include "../../../src/shared_context.h"
#include "../../../src/follower_ownership.h"
typedef struct { hb_key_context context; hb_harmony_t input,output; int valid,major_choice,minor_choice; } hb_key_harmony_cache;
typedef struct { int dominant_color_family,dominant_color_held,dominant_color_latched; unsigned long long timeline_lane_revision,timeline_lane_hash;int timeline_lane_cached; int target_scale_policy[4]; double trail_at[128],trail_previous_at[128]; unsigned trail_previous_chord[128]; uint8_t trail_previous_valid[128]; unsigned trail_chord_at[128],trail_chord,trail_signature; uint8_t trail_valid[128]; unsigned trail_input[256],trail_queued[256],trail_serial,trail_event_serial,trail_queue_serial[64]; unsigned short trail_outputs[128]; int trail_enabled,preview_target; int sc_landing_pitch,sc_anchor_valid,sc_anchor_pitch; hb_key_context sc_anchor_context; hb_key_harmony_cache key_cache[4]; unsigned key_cache_cursor; hb_key_context key_onset,key_queue[64],key_pending_context; double key_pending_at; int key_action,key_shift,key_return_after,key_pending_action; int key_scope,key_schedule_arm,key_pending,key_owner; int key_reference[HB_CP_KEYS][HB_CP_VOICES]; unsigned long long key_lane_active; int defaults_editor; hb_ar_state approach_rows; char opening_preview[12288]; unsigned long long opening_hash; uint8_t opening_pitches[128]; unsigned opening_quality; int opening_root; int chord_pair_input,chord_pair_render,chord_pair_top; uint8_t chord_pair_held[2][16][128]; int chord_pair_owner[2][16]; int chord_edit_lane; int rhythm_mode,rhythm_pattern,rhythm_window,rhythm_host; uint8_t rhythm_output_prewarped[128]; hb_mt_runtime motif; int motif_load[6],motif_load_last; unsigned policy_overrides;int policy_values[HB_POLICY_FIELDS],policy_last[6],policy_initialized;int render_velocity_gain; unsigned long long recorded_actions[128][HB_MOTION_LANES+1]; uint8_t recorded_action_valid[128]; unsigned long long action_queue[64][HB_MOTION_LANES+1]; uint8_t action_pitch[64]; int action_head,action_count; int next_predict,next_lookahead,next_anti_buffer_ms,boundary_buffer_ms,lookahead_restored; double motion_beat; hb_motion_config motion; hb_motion_route motion_local,motion_render; unsigned motion_harmony_signature; int motion_render_suppress[16]; hb_rx_event receiver_queue[256]; int receiver_count; uint8_t receiver_refs[16][128]; uint8_t receiver_sounding[128]; hb_chord_player player; hb_fp_config play; unsigned play_revision, play_applied;
hb_follower_voice follower_voices[HB_FOLLOWER_VOICES];
hb_input_intent follower_queue_input[64];
uint8_t follower_input_held[2][128];
int motion_event_owner,motion_output_owner[128];
double follower_origin_delay[2][128];
unsigned long long motion_follower_events[64][HB_MOTION_LANES+1],motion_output_events[128][HB_MOTION_LANES+1];
unsigned long long motion_player_events[HB_CP_KEYS][HB_MOTION_LANES+1],motion_held_events[128][HB_MOTION_LANES+1];
uint8_t motion_output_valid[128]; uint8_t (*motion_output_base)[3];
hb_harmony_t follower_path_harmony[128];
hb_closest_cache closest_assignments;
char follower_display[8][24]; int follower_display_valid;
int follower_input_seen; /* Direct MIDI takes ownership from monitor fallback. */
char harmony_display[4][48]; int harmony_display_valid;
unsigned long long next_touch_mask; double next_touch_started,next_touch_until; hb_harmony_t next_touch_observed;
uint8_t physical_velocity[16][128];unsigned short physical_target,target_attack_owner;int adopt_held,advance_pending,advance_phase,synthetic_advance;unsigned short advance_owner;
int pressure_full_velocity;uint16_t pad_sounding[128];double pad_flash_seconds[128];unsigned long long preview_single_low,preview_single_high;unsigned preview_gap_mask;int approach_layout,move_approach_layout,surface_approach_layout,surface_enabled;int surface_count[2],surface_notes[2][32],surface_targets[2][32],surface_rows[2][32];int preview_count,preview_notes[32],preview_targets[32],preview_rows[32]; signed char movy_pad_shift[128]; int movy_pad_pending, movy_pad_pending_shift; uint8_t movy_input_degree[128],movy_input_target[128]; uint8_t follower_origin[128], follower_queue_origin[64]; int used,role,mode,content_map,travel_map,chromatic_map,touch_lanes[9],follower_split_map,quant_timing,approach_control,approach_mode,window_ms,dirty,frames_since_change; uint8_t active[128]; uint8_t held_now[128]; uint8_t held_count[128]; int pending_off_frames[128]; int mapped[128]; uint8_t follower_held[128]; uint8_t follower_sounding[128]; uint8_t follower_velocity[128]; unsigned follower_bus_seq; uint8_t source_seen[12]; int resolved_root,resolved_confidence; unsigned rx_count; unsigned note_on_count; unsigned note_off_count; int last_note; int last_status; int last_velocity; int active_count; int last_inferred_count; unsigned raw_event_count; unsigned raw_note_count; unsigned raw_note_on_count; unsigned raw_note_off_count; int raw_last_note; int raw_last_status; int raw_last_velocity; int raw_last_channel; int raw_last_cable; uint8_t raw_prev[HB_MIDI_OUT_BYTES]; int map_target; hb_harmony_t candidate_harmony; int candidate_frames; int committed_frames; int render_channel; int source_channel; int resolved_source_channel; unsigned live_press_count; int live_vouch_pending; int live_vouch_age; int recent_live_note[16]; int recent_live_age[16]; uint8_t recent_live_valid[16]; unsigned render_count; unsigned render_fail_count; int render_last_note; int retrigger_held; int follow_lookahead_ms; int approach_pad_armed; uint8_t approach_below_held; uint8_t approach_above_held; int follower_queue_count; uint8_t follower_queue_note[64]; uint8_t follower_queue_velocity[64]; uint8_t follower_queue_on[64]; uint8_t follower_queue_channel[64]; int follower_queue_age_frames[64]; double follower_queue_target_beat[64]; double follower_queue_quant_beat[64]; double follower_queue_harmony_beat[64]; hb_harmony_t render_harmony; int render_harmony_active; double follower_queue_arrival_beat[64]; double follower_note_delay_beats[128]; uint8_t follower_role_interval[128]; uint8_t published_conductor[128]; uint8_t published_follower[128]; int settle_frames_remaining; int clip_event_idle_frames; int last_transport_playing; uint8_t trace_note[8]; uint8_t trace_on[8]; uint8_t trace_channel[8]; unsigned trace_count; int local_sense_count; int conductor_note_on_pending; uint8_t local_sense_notes[64]; int global_timing_restored; uint8_t role_flush_pending[128]; int role_flush_cursor; int movy_track,movy_playback,movy_passthrough; uint8_t recorded_sounding[16][128],recorded_source_pitch[16][128],passthrough_held[128]; } Inst;
static hb_input_intent hb_input_get(const Inst *instance,int source){
    return (hb_input_intent){instance->movy_input_degree[source],instance->movy_input_target[source],instance->movy_pad_shift[source],instance->approach_rows.tokens[source]};
}
static void hb_input_set(Inst *instance,int source,hb_input_intent input){
    instance->movy_input_degree[source]=(uint8_t)input.degree;instance->movy_input_target[source]=(uint8_t)input.target;
    instance->movy_pad_shift[source]=(signed char)input.shift;instance->approach_rows.tokens[source]=input.token;
}
/* Prefer live input consistently when a clip shares its source pitch. */
static const hb_follower_voice *hb_follower_display_voice(const Inst *instance,int source){
    const hb_follower_voice *selected=0;
    for(int index=0;index<HB_FOLLOWER_VOICES;index++){
        const hb_follower_voice *voice=&instance->follower_voices[index];
        if(voice->used&&voice->source==source&&(!selected||!voice->origin))selected=voice;
    }
    return selected;
}
/* Keep legacy pitch diagnostics representative of a remaining owner. */
static void hb_follower_summary(Inst *instance,int source){
    const hb_follower_voice *selected=hb_follower_display_voice(instance,source);
    instance->follower_sounding[source]=selected!=0;
    instance->mapped[source]=selected?selected->pitch:-1;
    if(selected){
        instance->follower_origin[source]=(uint8_t)selected->origin;
        instance->follower_velocity[source]=(uint8_t)selected->velocity;
        instance->follower_path_harmony[source]=selected->harmony;
        memcpy(instance->motion_held_events[source],selected->events,sizeof(selected->events));
    }
}
#include "../../../src/role_policy_runtime.h"
static hb_harmony_t hb_mapping_target(hb_harmony_t harmony,int map_target);
/* Shared controls; voice ownership and recorded events remain local. */
static hb_motion_config g_motion_settings;
static int g_motion_settings_ready=0,g_motion_settings_restored=0,g_quant_restored=0;
static void hb_motion_copy_settings(hb_motion_config *target,const hb_motion_config *from){
    for(int lane=0;lane<HB_MOTION_LANES;lane++){
        if(memcmp(&target->lanes[lane],&from->lanes[lane],sizeof(hb_motion_lane)))target->revision[lane]++;
        target->lanes[lane]=from->lanes[lane];
    }
    target->selected=from->selected;target->bypass=from->bypass;
}

static int hb_secondary_at(Inst *instance,int source_note);
static double hb_motion_position(Inst *instance);
static int hb_rr_pattern(const Inst *instance){return instance->rhythm_mode==1?0:instance->rhythm_mode==2?instance->rhythm_pattern:g_motif_rhythm;}
static void hb_rr_prepare(Inst *instance,hb_motion_route *route){
    int pattern=hb_rr_pattern(instance);double now=hb_motion_position(instance);
    double span=(instance->rhythm_mode==2?instance->rhythm_window:g_render_window)?4:1;
    if(now+1e-6<route->rhythm_now&&hb_rr_active(&route->rhythm))hb_rr_panic(&route->rhythm);
    route->rhythm_now=now;route->rhythm_enabled=pattern>=2;
    double target=hb_rr_time(now,pattern,span);
    route->rhythm_delay=(instance->rhythm_host&&instance->movy_playback)||target<now?0:target-now;
}
static void hb_motion_output(Inst *instance,hb_motion_route *route,const uint8_t message[3]);
static void hb_motion_flush_render(Inst *instance);
static double hb_motion_position(Inst *instance);
static double hb_motion_condition_position(void);
static uint16_t hb_scale_mask(hb_harmony_t harmony);
static uint16_t hb_explicit_scale_mask(int root_pc,int scale_index);
static int reference_root(Inst *instance);
static int hb_nth_held_note(const Inst *instance,int ordinal);
static int hb_held_count(const Inst *instance);
static int hb_nth_follower_note(const Inst *instance,int ordinal);
static int hb_follower_held_count(const Inst *instance);
static const char *hb_role_name_for_interval(int interval);
static const char *hb_follower_role_name(const Inst *instance,int ordinal);
static const char *hb_follower_degree_role_for_note(Inst *instance,int note);
static int hb_resolve_follower_reference_root(Inst *instance,int *root_out);
static hb_harmony_t hb_follower_scale_target(Inst *instance,hb_harmony_t harmony);
static int hb_parent_scale_index(Inst *instance,hb_harmony_t harmony);
static double hb_chord_grid_beats(void);
static double hb_quant_grid_beats_for(const Inst *instance);
static double hb_anticipation_beats(void);
static double hb_ms_to_beats(int milliseconds);
static double hb_current_beat(void);
static void hb_publish_instance_notes(Inst *instance);
static int hb_read_active_set(char *uuid,int uuid_len,char *name,int name_len);
static int hb_find_conductor_track(const char *uuid);
static char *hb_read_text_file(const char *path,long *size_out);
/* Target history follows emitted input gestures, not generated voices. The
   monotonic DSP beat keeps decay continuous over transport loops and seeks. */
static hb_harmony_t hb_render_harmony(Inst *instance);
static void hb_trail_boundary(Inst *instance);
static void hb_trail_heard(Inst *instance,unsigned tag){
    if(!tag||tag>128)return;
    hb_trail_boundary(instance);int pitch=(int)tag-1;
    instance->trail_previous_valid[pitch]=instance->trail_valid[pitch];
    instance->trail_previous_at[pitch]=instance->trail_at[pitch];
    instance->trail_previous_chord[pitch]=instance->trail_chord_at[pitch];
    instance->trail_valid[pitch]=1;instance->trail_at[pitch]=instance->motion_beat;
    instance->trail_chord_at[pitch]=instance->trail_chord;
}
static unsigned hb_trail_target(Inst *instance,int source,int target,int velocity);
static void hb_trail_output(Inst *instance,uint8_t *output,unsigned tag){
    if(!tag)return;
    if(instance->motion_output_base){
        long offset=(output-instance->motion_output_base[0])/3;
        if(offset>=0&&offset<128)instance->trail_outputs[offset]=(unsigned short)tag;
    }else hb_trail_heard(instance,tag);
}
static Inst g_pool[HB_MAX_INSTANCES];
#include "../../../src/harmony_override.h"
static int g_touch_lanes[9]={1,2,3,4,13,14,15,16,5},g_touch_restored;
static void hb_touch_publish(const int *lanes){
    memcpy(g_touch_lanes,lanes,sizeof(g_touch_lanes));g_touch_restored=1;
    for(int i=0;i<HB_MAX_INSTANCES;i++)if(g_pool[i].used)memcpy(g_pool[i].touch_lanes,g_touch_lanes,sizeof(g_touch_lanes));
}
static void hb_touch_defaults(void){
    for(int k=0;k<9;k++)g_touch_lanes[k]=k==8?5:k<4?k+1:k+9;
    g_touch_restored=0;
}
static void hb_motion_publish_settings(Inst *source){
    hb_motion_copy_settings(&g_motion_settings,&source->motion);
    g_motion_settings_ready=g_motion_settings_restored=1;
    for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used&& &g_pool[index]!=source)
        hb_motion_copy_settings(&g_pool[index].motion,&g_motion_settings);
}
static int g_conductor_block_ready;
static unsigned long long g_conductor_block_id;
static hb_movy_clip_t g_movy_clips[HB_MAX_INSTANCES];
static int g_movy_present, g_movy_running, g_movy_blocked;
static hb_tick_t g_movy_tick, g_movy_period, g_movy_origin, g_movy_revision;
static unsigned g_movy_ppqn=96;
static void hb_movy_refresh(void);

static int mod12(int value){value%=12;return value<0?value+12:value;}
static int parse_i(const char *value,int fallback){char *end;long parsed;if(!value||!*value)return fallback;end=0;parsed=strtol(value,&end,10);return end==value?fallback:(int)parsed;}
static void hb_monitor_open(void){
    if(g_monitor)return;
    int fd=shm_open(HB_MONITOR_SHM,HB_O_RDWR,0);
    if(fd<0)return;
    void *ptr=mmap(0,sizeof(hb_monitor_shared_t),HB_PROT_READ,HB_MAP_SHARED,fd,0);
    close(fd);
    if(ptr==HB_MAP_FAILED)return;
    hb_monitor_shared_t *shared=(hb_monitor_shared_t*)ptr;
    if(shared->magic!=HB_MONITOR_MAGIC||shared->version!=HB_MONITOR_VERSION)return;
    g_monitor=shared;
}
static int hb_monitor_channel(Inst *instance){
    if(!instance)return -1;
    if(instance->source_channel>=0&&instance->source_channel<16)return instance->source_channel;
    /* A MIDI-FX instance is not the parent Chain instance expected by the
       host's slot_recv_channel callback. The source-aware Chain patch tags
       the parent track's recv channel into source and we cache it here. */
    if(instance->resolved_source_channel>=0&&instance->resolved_source_channel<16)return instance->resolved_source_channel;
    /* Never discover ownership from saved files on an audio callback. The
       source-aware Chain tag or explicit source channel is authoritative. */
    return -1;
}
static int hb_monitor_snapshot_channel(Inst *instance,uint8_t velocities[128],uint32_t *generation){
    hb_monitor_open();
    if(!g_monitor)return 0;
    int channel=hb_monitor_channel(instance);
    if(channel<0)return 0;
    for(int tries=0;tries<4;tries++){
        uint32_t before=g_monitor->seq;
        if(before&1u)continue;
        for(int note=0;note<128;note++)velocities[note]=g_monitor->velocities[channel][note];
        uint32_t gen=g_monitor->generation;
        uint32_t after=g_monitor->seq;
        if(before==after&&!(after&1u)){
            if(generation)*generation=gen;
            return 1;
        }
    }
    return 0;
}
static int hb_sync_from_monitor(Inst *instance){
    if(!instance||instance->role!=1||instance->follower_input_seen)return 0;
    uint8_t velocities[128];uint32_t generation=0;
    if(!hb_monitor_snapshot_channel(instance,velocities,&generation))return 0;
    int changed=0;
    memset(instance->held_count,0,sizeof(instance->held_count));
    memset(instance->held_now,0,sizeof(instance->held_now));
    memset(instance->active,0,sizeof(instance->active));
    for(int note=0;note<128;note++){
        uint8_t next=velocities[note]?1:0;
        if(instance->follower_held[note]!=next)changed=1;
        instance->follower_held[note]=next;
        instance->follower_velocity[note]=velocities[note];
    }
    if(changed){
        int reference_root=0;
        int have_reference_root=hb_resolve_follower_reference_root(instance,&reference_root);
        for(int note=0;note<128;note++){
            instance->follower_role_interval[note]=instance->follower_held[note]&&have_reference_root
                ?(uint8_t)mod12(note-reference_root):255;
        }
        hb_publish_instance_notes(instance);
        instance->dirty=1;
        instance->frames_since_change=0;
        instance->candidate_frames=0;
    }
    (void)generation;
    return changed;
}
static int hb_sync_conductor_from_monitor(Inst *instance){
    if(!instance||instance->role!=0||instance->movy_track>=0)return 0;
    /* Generated conductor voices, rather than raw keys, are authoritative.
       A raw monitor snapshot must not replace their held-note collection. */
    if(hb_cp_enabled(&instance->player))return 0;
    uint8_t velocities[128];uint32_t generation=0;
    if(!hb_monitor_snapshot_channel(instance,velocities,&generation))return 0;
    int changed=0;
    for(int note=0;note<128;note++){
        int was_held=instance->held_count[note]>0;
        int now_held=velocities[note]>0;
        if(was_held==now_held)continue;
        changed=1;
        if(now_held){
            instance->held_count[note]=1;
            instance->conductor_note_on_pending=1;
            instance->held_now[note]=1;
            instance->pending_off_frames[note]=0;
            instance->candidate_frames=0;
            instance->dirty=1;
            instance->frames_since_change=0;
        }else{
            instance->held_count[note]=0;
            instance->held_now[note]=0;
            instance->pending_off_frames[note]=(g_bus.analysis_release_ms>0)?1:0;
            instance->dirty=1;
            instance->frames_since_change=0;
        }
    }
    if(changed)hb_publish_instance_notes(instance);
    (void)generation;
    return changed;
}
static void ensure_init(void){if(g_init)return;hb_override_reset();hb_sc_reset();memset(&g_key_context,0,sizeof(g_key_context));g_key_armed=g_parallel_on=g_parallel_latch=g_parallel_manual=g_key_lane_parallel=0;g_parallel_scale=2;g_key_preview[0]=0;g_key_scale_mode=1;g_key_conductor_travel=3;g_key_settings_restored=0;g_key_follower_travel=0;hb_touch_defaults();g_motion_settings_ready=g_motion_settings_restored=g_quant_restored=0;g_scale_restored=0;g_infer_cached_root=-1;g_scale_exceptions_restored=0;g_hb_hold_ms=350;g_hb_hold_restored=0;hb_pad_defaults();memset(g_humanize,0,sizeof(g_humanize));g_humanize_restored=0;g_conductor_block_ready=0;memset(&g_bus,0,sizeof(g_bus));g_bus.global_root_policy=2;hb_global_open();g_bus.sensor_sources=0;g_bus.chord_timescale=0;g_bus.stability=0;g_bus.chord_timing=0;g_bus.quant_timing=0;g_bus.anticipation=0;g_bus.boundary_buffer_ms=-3;g_buffer_restored=0;g_bus.analysis_release_ms=60;g_bus.follower_content_map=1;g_bus.follower_travel_map=7;g_bus.follower_scale=1;g_bus.approach_control=1;g_bus.approach_mode=0;g_bus.inference_window_ms=25;g_bus.context=0;g_bus.accidentals=0;g_bus.auto_spell_sharps=1;g_bus.auto_spell_locked=0;g_bus.clip_track=-1;g_bus.clip_slot=0;g_bus.clip_stage=0;g_bus.clip_context=1;g_bus.last_clock_status=-1;g_bus.last_clip_playhead=0.0;g_bus.have_last_clip_playhead=0;g_bus.next_predict=1;g_bus.next_lookahead=0;g_bus.next_anti_buffer_ms=25;g_lookahead_restored=0;g_bus.next_model_locked=0;g_bus.next_shift_active=0;g_bus.next_learning_count=0;g_bus.next_model_count=0;g_bus.next_last_playhead=0.0;g_bus.next_have_playhead=0;g_bus.next_learning_started=0;g_bus.next_learning_progress_beats=0.0;memset(&g_bus.observed_harmony,0,sizeof(g_bus.observed_harmony));g_bus.cache_rev=0;g_bus.sense_rev=0;g_bus.last_sense_count=0;g_bus.global_last_status=-1;g_bus.global_last_note=-1;g_bus.global_last_channel=-1;g_bus.global_last_instance=-1;g_bus.clip_loop_start=0.0;g_bus.clip_loop_end=4.0;for(int index=0;index<HB_MAX_INSTANCES;index++){memset(&g_pool[index],0,sizeof(g_pool[index]));g_pool[index].approach_pad_armed=1;for(int note=0;note<128;note++)g_pool[index].mapped[note]=-1;}g_init=1;}

static char *hb_read_text_file(const char *path,long *size_out){
    FILE *file=fopen(path,"rb");if(!file)return 0;
    if(fseek(file,0,2)!=0){fclose(file);return 0;}
    long size=ftell(file);if(size<=0||size>8*1024*1024){fclose(file);return 0;}
    if(fseek(file,0,0)!=0){fclose(file);return 0;}
    char *buffer=(char*)malloc((size_t)size+1);if(!buffer){fclose(file);return 0;}
    size_t read=fread(buffer,1,(size_t)size,file);fclose(file);
    buffer[read]='\0';if(size_out)*size_out=(long)read;return buffer;
}
static const char *hb_find_matching(const char *start,char open_ch,char close_ch){
    if(!start||*start!=open_ch)return 0;
    int depth=0,in_string=0,escape=0;
    for(const char *p=start;*p;p++){
        char ch=*p;
        if(in_string){
            if(escape)escape=0;
            else if(ch=='\\')escape=1;
            else if(ch=='"')in_string=0;
            continue;
        }
        if(ch=='"'){in_string=1;continue;}
        if(ch==open_ch)depth++;
        else if(ch==close_ch){depth--;if(depth==0)return p;}
    }
    return 0;
}
static int hb_read_active_set(char *uuid,int uuid_len,char *name,int name_len){
    FILE *file=fopen("/data/UserData/schwung/active_set.txt","r");if(!file)return 0;
    if(!fgets(uuid,uuid_len,file)){fclose(file);return 0;}
    if(!fgets(name,name_len,file)){name[0]='\0';}
    fclose(file);
    for(char *p=uuid;*p;p++)if(*p=='\n'||*p=='\r'){*p='\0';break;}
    for(char *p=name;*p;p++)if(*p=='\n'||*p=='\r'){*p='\0';break;}
    return uuid[0]!=0&&name[0]!=0;
}
static int hb_find_conductor_track(const char *uuid){
    char path[512];
    for(int track=0;track<4;track++){
        snprintf(path,sizeof(path),"/data/UserData/schwung/set_state/%s/slot_%d.json",uuid,track);
        long size=0;char *json=hb_read_text_file(path,&size);(void)size;
        if(!json)continue;
        int has_module=strstr(json,"harmonybus")!=0;
        int is_conductor=(strstr(json,"hb15,0,")||strstr(json,"hb14,0,")||strstr(json,"hb13,0,")||strstr(json,"hb12,0,")||strstr(json,"hb11,0,")||strstr(json,"hb10,0,")||strstr(json,"hb9,0,")||strstr(json,"hb8,0,")||strstr(json,"hb7,0,")||strstr(json,"hb6,0,")||strstr(json,"hb5,0,")||strstr(json,"hb4,0,")||strstr(json,"hb3,0,")||strstr(json,"hb2,0,")||strstr(json,"hb1,0,"));
        free(json);
        if(has_module&&is_conductor)return track;
    }
    return -1;
}
static int hb_find_selected_track(const char *json){
    if(!json)return -1;
    const char *tracks=strstr(json,"\"tracks\"");if(!tracks)return -1;
    const char *array=strstr(tracks,"[");if(!array)return -1;
    const char *p=array+1;
    for(int track=0;track<4;track++){
        while(*p&&*p!='{')p++;
        if(!*p)return -1;
        const char *end=hb_find_matching(p,'{','}');if(!end)return -1;
        const char *selected=strstr(p,"\"isSelected\"");
        if(selected&&selected<end){
            const char *colon=strstr(selected,":");
            if(colon&&colon<end){
                const char *truth=colon+1;
                while(*truth==' '||*truth=='\t'||*truth=='\r'||*truth=='\n')truth++;
                if(!strncmp(truth,"true",4))return track;
            }
        }
        p=end+1;
    }
    return -1;
}
static const char *hb_nth_track_object(const char *json,int index,const char **end_out){
    const char *tracks=strstr(json,"\"tracks\"");if(!tracks)return 0;
    const char *array=strstr(tracks,"[");if(!array)return 0;
    const char *p=array+1;
    for(int current=0;current<=index;current++){
        while(*p&&*p!='{')p++;
        if(!*p)return 0;
        const char *end=hb_find_matching(p,'{','}');if(!end)return 0;
        if(current==index){if(end_out)*end_out=end;return p;}
        p=end+1;
    }
    return 0;
}
static int hb_parse_double_after(const char *start,const char *limit,const char *key,double *value){
    const char *p=strstr(start,key);if(!p||p>=limit)return 0;
    p=strstr(p,":");if(!p||p>=limit)return 0;p++;
    *value=strtod(p,0);return 1;
}
static int hb_parse_int_after(const char *start,const char *limit,const char *key,int *value){
    double numeric=0.0;if(!hb_parse_double_after(start,limit,key,&numeric))return 0;*value=(int)numeric;return 1;
}
static int hb_load_clip_cache(void){
    char uuid[96],name[192],song_path[768];
    g_bus.clip_stage=0;
    if(!hb_read_active_set(uuid,sizeof(uuid),name,sizeof(name))){g_bus.clip_stage=1;return 0;}
    g_bus.clip_stage=2;

    snprintf(song_path,sizeof(song_path),"/data/UserData/UserLibrary/Sets/%s/%s/Song.abl",uuid,name);
    long size=0;char *json=hb_read_text_file(song_path,&size);(void)size;
    if(!json){g_bus.clip_stage=5;return 0;}
    g_bus.clip_stage=6;

    int track=hb_find_conductor_track(uuid);
    if(track<0){
        track=hb_find_selected_track(json);
        if(track<0){free(json);g_bus.clip_stage=3;return 0;}
        /* Selected-track fallback: state discovery failed, but we still have
           an unambiguous track while the user is inspecting Harmony Bus. */
        g_bus.clip_stage=4;
    }else{
        g_bus.clip_stage=4;
    }

    const char *track_end=0;const char *track_obj=hb_nth_track_object(json,track,&track_end);
    if(!track_obj){free(json);g_bus.clip_stage=7;return 0;}
    g_bus.clip_stage=8;

    const char *slots=strstr(track_obj,"\"clipSlots\"");
    if(!slots||slots>=track_end){free(json);g_bus.clip_stage=9;return 0;}
    const char *slots_array=strstr(slots,"[");
    if(!slots_array||slots_array>=track_end){free(json);g_bus.clip_stage=9;return 0;}
    const char *slots_end=hb_find_matching(slots_array,'[',']');
    if(!slots_end){free(json);g_bus.clip_stage=9;return 0;}

    const char *chosen=0,*chosen_end=0;
    const char *scan=slots_array+1;
    int slot_index=0;
    while(scan<slots_end && slot_index<8){
        const char *slot_obj=strstr(scan,"{");
        if(!slot_obj||slot_obj>=slots_end)break;
        const char *slot_end=hb_find_matching(slot_obj,'{','}');
        if(!slot_end||slot_end>slots_end)break;

        const char *clip=strstr(slot_obj,"\"clip\"");
        const char *obj=0,*obj_end=0;
        if(clip&&clip<slot_end){
            obj=strstr(clip,"{");
            if(obj&&obj<slot_end)obj_end=hb_find_matching(obj,'{','}');
            if(obj_end&&obj_end>slot_end){obj=0;obj_end=0;}
        }

        if(g_bus.clip_slot>0){
            if(slot_index==(g_bus.clip_slot-1)){
                chosen=obj;
                chosen_end=obj_end;
                break;
            }
        }else if(obj&&obj_end){
            const char *playing=strstr(obj,"\"isPlaying\"");
            if(playing&&playing<obj_end){
                const char *colon=strstr(playing,":");
                if(colon&&colon<obj_end){
                    const char *truth=colon+1;
                    while(*truth==' '||*truth=='\t'||*truth=='\r'||*truth=='\n')truth++;
                    if(!strncmp(truth,"true",4)){chosen=obj;chosen_end=obj_end;break;}
                }
            }
        }
        scan=slot_end+1;
        slot_index++;
    }

    /* In Auto, unknown current slot means zero notes. With an explicit slot,
       a null clip also means zero notes. Never substitute historical material. */
    if(!chosen){
        __atomic_add_fetch(&g_bus.seq,1,__ATOMIC_RELEASE);
        int cache_changed=(g_bus.clip_track!=track||g_bus.clip_note_count!=0||g_bus.clip_valid!=1);
        g_bus.clip_track=track;
        g_bus.clip_note_count=0;
        g_bus.clip_loop_start=0.0;
        g_bus.clip_loop_end=4.0;
        g_bus.clip_valid=1;
        g_bus.clip_stage=10;
        if(cache_changed)g_bus.cache_rev++;
        __atomic_add_fetch(&g_bus.seq,1,__ATOMIC_RELEASE);
        free(json);
        return 1;
    }
    g_bus.clip_stage=11;

    double loop_start=0.0,loop_end=4.0;
    const char *loop=strstr(chosen,"\"loop\"");
    if(loop&&loop<chosen_end){
        const char *loop_obj=strstr(loop,"{");
        const char *loop_end_obj=loop_obj?hb_find_matching(loop_obj,'{','}'):0;
        if(loop_obj&&loop_end_obj&&loop_end_obj<chosen_end){
            hb_parse_double_after(loop_obj,loop_end_obj,"\"start\"",&loop_start);
            hb_parse_double_after(loop_obj,loop_end_obj,"\"end\"",&loop_end);
        }
    }

    const char *notes_key=strstr(chosen,"\"notes\"");
    if(!notes_key||notes_key>=chosen_end){free(json);g_bus.clip_stage=12;return 0;}
    const char *notes_array=strstr(notes_key,"[");
    if(!notes_array||notes_array>=chosen_end){free(json);g_bus.clip_stage=12;return 0;}
    const char *notes_end=hb_find_matching(notes_array,'[',']');
    if(!notes_end){free(json);g_bus.clip_stage=12;return 0;}

    hb_clip_note_t temp[HB_MAX_CLIP_NOTES];int count=0;
    scan=notes_array+1;
    while(scan<notes_end&&count<HB_MAX_CLIP_NOTES){
        const char *obj=strstr(scan,"{");if(!obj||obj>=notes_end)break;
        const char *obj_end=hb_find_matching(obj,'{','}');if(!obj_end||obj_end>notes_end)break;
        int note=-1;double note_start=0.0,duration=0.0;
        if(hb_parse_int_after(obj,obj_end,"\"noteNumber\"",&note)&&
           hb_parse_double_after(obj,obj_end,"\"startTime\"",&note_start)&&
           hb_parse_double_after(obj,obj_end,"\"duration\"",&duration)&&
           note>=0&&note<128&&duration>0.0){
            temp[count].note=note;temp[count].start=note_start;temp[count].duration=duration;count++;
        }
        scan=obj_end+1;
    }
    free(json);

    int cache_changed=(g_bus.clip_track!=track||g_bus.clip_note_count!=count||
                       g_bus.clip_loop_start!=loop_start||g_bus.clip_loop_end!=loop_end);
    if(!cache_changed){
        for(int i=0;i<count;i++){
            if(g_bus.clip_notes[i].note!=temp[i].note||
               g_bus.clip_notes[i].start!=temp[i].start||
               g_bus.clip_notes[i].duration!=temp[i].duration){
                cache_changed=1;break;
            }
        }
    }
    __atomic_add_fetch(&g_bus.seq,1,__ATOMIC_RELEASE);
    g_bus.clip_track=track;g_bus.clip_note_count=count;g_bus.clip_loop_start=loop_start;g_bus.clip_loop_end=loop_end;
    for(int i=0;i<count;i++)g_bus.clip_notes[i]=temp[i];
    g_bus.clip_valid=1;
    g_bus.clip_stage=count>0?14:13;
    if(cache_changed)g_bus.cache_rev++;
    __atomic_add_fetch(&g_bus.seq,1,__ATOMIC_RELEASE);
    return 1;
}
static void hb_clear_clip_cache(void){
    __atomic_add_fetch(&g_bus.seq,1,__ATOMIC_RELEASE);
    g_bus.clip_track=-1;g_bus.clip_valid=0;g_bus.clip_note_count=0;
    g_bus.clip_loop_start=0.0;g_bus.clip_loop_end=4.0;
    __atomic_add_fetch(&g_bus.seq,1,__ATOMIC_RELEASE);
}
static int hb_clock_status(void){
    if(g_movy_present)return g_movy_running?MOVE_CLOCK_STATUS_RUNNING:MOVE_CLOCK_STATUS_STOPPED;
    /* The shared beat callback is authoritative when present: negative means
       stopped. A private Movy chain may never receive local realtime bytes,
       so its get_clock_status can disagree with the active shared transport. */
    if(g_host&&g_host->get_beat_position)
        return g_host->get_beat_position()>=0.0?MOVE_CLOCK_STATUS_RUNNING:MOVE_CLOCK_STATUS_STOPPED;
    return (g_host&&g_host->get_clock_status)?g_host->get_clock_status():MOVE_CLOCK_STATUS_UNAVAILABLE;
}
static double hb_clip_playhead(void){
    if(g_movy_present){
        if(!g_movy_period)return 0.0;
        hb_tick_t phase=(g_movy_tick%g_movy_period+g_movy_period-g_movy_origin%g_movy_period)%g_movy_period;
        return (double)phase/g_movy_ppqn;
    }
    double beat=(g_host&&g_host->get_beat_position)?g_host->get_beat_position():(double)g_bus.clip_clock_ticks/24.0;
    if(beat<0.0)beat=(double)g_bus.clip_clock_ticks/24.0;
    double length=g_bus.clip_loop_end-g_bus.clip_loop_start;
    if(length>0.0){while(beat>=length)beat-=length;while(beat<0.0)beat+=length;beat+=g_bus.clip_loop_start;}
    return beat;
}
static int hb_clip_active_notes(uint8_t *output,int max_notes){
    if(!g_bus.clip_valid||!output||max_notes<=0)return 0;
    double playhead=hb_clip_playhead();int count=0;
    for(int i=0;i<g_bus.clip_note_count&&count<max_notes;i++){
        hb_clip_note_t note=g_bus.clip_notes[i];
        if(playhead+1e-6>=note.start&&playhead<note.start+note.duration-1e-6)output[count++]=(uint8_t)note.note;
    }
    return count;
}
static int hb_observed_notes(const Inst *instance,uint8_t *output,int max_notes){
    (void)instance;
    if(!output||max_notes<=0)return 0;
    uint8_t seen[128];memset(seen,0,sizeof(seen));int count=0;
    /* Conductor sensing is set-wide: merge realtime-held notes from every
       active Conductor instance. Clip contents remain excluded. */
    for(int instance_index=0;instance_index<HB_MAX_INSTANCES;instance_index++){
        const Inst *conductor=&g_pool[instance_index];
        if(!conductor->used||conductor->role!=0)continue;
        for(int note=0;note<128;note++){
            if(conductor->held_count[note]>0)seen[note]=1;
        }
        for(int event=0;event<HB_MT_SCHEDULE;event++)if(conductor->motif.events[event].used&&conductor->motif.events[event].started){int note=conductor->motif.events[event].pitch-g_bus.global_transpose;if(note>=0&&note<128)seen[note]=1;}
    }
    for(int note=0;note<128&&count<max_notes;note++){
        if(seen[note])output[count++]=(uint8_t)note;
    }
    return count;
}
static int hb_nth_observed_note(const Inst *instance,int ordinal){
    uint8_t notes[64];int count=hb_observed_notes(instance,notes,64);
    return ordinal>=0&&ordinal<count?notes[ordinal]:-1;
}
static int hb_nth_shared_conductor_note(int ordinal){
    return hb_nth_observed_note(0,ordinal);
}
static int hb_nth_conductor_analysis_note(const Inst *instance,int ordinal){
    if(!instance||ordinal<0)return -1;
    if(instance->role==0){int seen=0;for(int note=0;note<128;note++)if(instance->held_count[note]>0){if(seen==ordinal)return note;seen++;}return -1;}
    uint8_t notes[64];int count=hb_observed_notes(instance,notes,64);
    return ordinal<count?notes[ordinal]:-1;
}
static int hb_conductor_analysis_count(const Inst *instance){
    if(!instance)return 0;
    if(instance->role==0){int count=0;for(int note=0;note<128;note++)if(instance->held_count[note]>0)count++;return count;}
    uint8_t notes[64];return hb_observed_notes(instance,notes,64);
}
static int hb_nth_aggregate_follower_observation(int ordinal,const Inst **follower_out){
    if(follower_out)*follower_out=0;
    if(ordinal<0)return -1;
    int seen=0;
    for(int instance_index=0;instance_index<HB_MAX_INSTANCES;instance_index++){
        const Inst *follower=&g_pool[instance_index];
        if(!follower->used||follower->role!=1)continue;
        for(int note=0;note<128;note++){
            if(!follower->published_follower[note])continue;
            if(seen==ordinal){
                if(follower_out)*follower_out=follower;
                return note;
            }
            seen++;
        }
    }
    return -1;
}
static int hb_nth_aggregate_follower_unique_note(int ordinal){
    if(ordinal<0)return -1;
    uint8_t seen[128];memset(seen,0,sizeof(seen));
    for(int instance_index=0;instance_index<HB_MAX_INSTANCES;instance_index++){
        const Inst *follower=&g_pool[instance_index];
        if(!follower->used||follower->role!=1)continue;
        for(int note=0;note<128;note++)if(follower->published_follower[note])seen[note]=1;
    }
    int count=0;
    for(int note=0;note<128;note++)if(seen[note]){
        if(count==ordinal)return note;
        count++;
    }
    return -1;
}
static int hb_aggregate_follower_unique_count(void){
    uint8_t seen[128];memset(seen,0,sizeof(seen));
    for(int instance_index=0;instance_index<HB_MAX_INSTANCES;instance_index++){
        const Inst *follower=&g_pool[instance_index];
        if(!follower->used||follower->role!=1)continue;
        for(int note=0;note<128;note++)if(follower->published_follower[note])seen[note]=1;
    }
    int count=0;for(int note=0;note<128;note++)if(seen[note])count++;
    return count;
}
static int hb_nth_local_follower_note(const Inst *instance,int ordinal){
    if(!instance||instance->role!=1||ordinal<0)return -1;
    int count=0;for(int note=0;note<128;note++)if(instance->published_follower[note]){
        if(count==ordinal)return note;
        count++;
    }
    return -1;
}
static int hb_local_follower_count(const Inst *instance){
    if(!instance||instance->role!=1)return 0;
    int count=0;for(int note=0;note<128;note++)if(instance->published_follower[note])count++;
    return count;
}
static int hb_nth_local_conductor_note(const Inst *instance,int ordinal){
    if(!instance||instance->role!=0||ordinal<0)return -1;
    int count=0;for(int note=0;note<128;note++)if(instance->held_count[note]>0){
        if(count==ordinal)return note;
        count++;
    }
    return -1;
}
static int hb_local_conductor_count(const Inst *instance){
    if(!instance||instance->role!=0)return 0;
    int count=0;for(int note=0;note<128;note++)if(instance->held_count[note]>0)count++;
    return count;
}
static int hb_aggregate_follower_count(void){
    int count=0;
    for(int instance_index=0;instance_index<HB_MAX_INSTANCES;instance_index++){
        const Inst *follower=&g_pool[instance_index];
        if(!follower->used||follower->role!=1)continue;
        for(int note=0;note<128;note++)if(follower->published_follower[note])count++;
    }
    return count;
}
static int hb_nth_follower_analysis_note(const Inst *instance,int ordinal){
    if(!instance||ordinal<0)return -1;
    if(instance->role==1){int seen=0;for(int note=0;note<128;note++)if(instance->published_follower[note]){if(seen==ordinal)return note;seen++;}return -1;}
    if(instance->role==0)return hb_nth_aggregate_follower_observation(ordinal,0);
    return -1;
}
static int hb_follower_analysis_count(const Inst *instance){
    if(!instance)return 0;
    if(instance->role==1){int count=0;for(int note=0;note<128;note++)if(instance->published_follower[note])count++;return count;}
    if(instance->role==0)return hb_aggregate_follower_count();
    return 0;
}
static int hb_infer_follower_root(Inst *instance,int *root_out){
    (void)instance;
    if(!root_out)return 0;
    if(hb_global_root_policy()==0){
        hb_monitor_open();
        if(!g_monitor)return 0;
        int have_root=0;
        int root=-1;
        for(int channel=0;channel<16;channel++){
            if(!g_monitor->root_valid[channel])continue;
            int channel_root=mod12((int)g_monitor->root_pc[channel]);
            if(!have_root){root=channel_root;have_root=1;}
            else if(root!=channel_root)return 0; /* conflicting observations */
        }
        if(!have_root)return 0;
        *root_out=root;
        return 1;
    }
    if(hb_global_root_policy()==1){
        uint8_t seen[128];memset(seen,0,sizeof(seen));
        uint8_t notes[128];int count=0;
        for(int instance_index=0;instance_index<HB_MAX_INSTANCES;instance_index++){
            Inst *follower=&g_pool[instance_index];
            if(!follower->used||follower->role!=1)continue;
            for(int note=0;note<128;note++){
                if(follower->follower_held[note]&&!seen[note]){
                    seen[note]=1;
                    notes[count++]=(uint8_t)note;
                }
            }
        }
        if(count<2)return 0;
        hb_harmony_t inferred=hb_infer_harmony(notes,count);
        if(!inferred.valid)return 0;
        *root_out=mod12(inferred.root_pc);
        return 1;
    }
    return 0;
}
static int hb_resolve_follower_reference_root(Inst *instance,int *root_out){
    (void)instance;
    if(!root_out)return 0;
    if(hb_global_root_policy()==2){
        *root_out=mod12(hb_global_explicit_root());
        return 1;
    }
    return hb_infer_follower_root(instance,root_out);
}
static int hb_map_follower_note_unoperated(Inst *instance,int source_note);
static hb_key_context hb_key_for(Inst *instance){
    hb_key_context context=instance->key_scope?instance->key_onset:g_key_context;
    /* Freeze the destination choice with delayed onsets; previews retain their
       real owner through the private Inst copy. Never borrow a live arm for replay. */
    if(!instance->key_scope&&!instance->movy_playback&&g_key_armed&&g_key_scale_mode==2&&
       (g_sc_arm_owner<0||g_sc_arm_owner==instance->key_owner))
        context.approach_scale=g_parallel_scale==18?-1:HB_PARALLEL_SCALE_IDS[g_parallel_scale];
    if(!context.active&&HB_SCALE_DOMINANT[hb_shared_follower_scale()]){
        int root=0;hb_resolve_follower_reference_root(instance,&root);
        context.active=context.blues=1;context.source_root=context.target_root=mod12(root+g_bus.global_transpose);
        context.source_mask=context.target_mask=hb_explicit_scale_mask(context.target_root,hb_shared_follower_scale());
    }
    return context;
}
static int hb_key_pitch(Inst *instance,int pitch){return hb_key_map(hb_key_for(instance),pitch);}
static unsigned hb_simplified_target_scale(Inst *instance,int target,unsigned parent);
static unsigned hb_function_family(Inst *instance,int target,unsigned parent,int minor,int kind);
static int hb_nth_scale_interval_from_root(uint16_t scale_mask,int target_root,int degree);
static int hb_target_minor(unsigned parent,int target);
static uint16_t hb_dominant_scale_mask(Inst *instance,hb_harmony_t harmony,int tonic);
static uint16_t hb_follower_input_scale(Inst *instance,int root);
static uint16_t hb_transpose_mask(uint16_t mask,int semitones);
/* A prescribed destination collection keeps its intervals while the target
   itself follows the current key map. Do not flatten its major third through
   the old minor key's degree map. */
static hb_key_context hb_key_collection_context(hb_key_context context,int target,unsigned collection){
    int destination=mod12(hb_key_map(context,60+mod12(target)));
    context.source_root=mod12(target);context.target_root=destination;
    context.source_mask=collection;
    context.target_mask=hb_transpose_mask(collection,destination-context.source_root);
    return context;
}
static hb_harmony_t hb_key_harmony(Inst *instance,hb_harmony_t harmony){
    hb_harmony_t override;
    if(hb_override_read(instance,&override)&&!memcmp(&override,&harmony,sizeof(harmony)))return harmony;
    hb_key_context context=hb_key_for(instance);
    if(((harmony.intent_kind>0&&harmony.intent_kind<4)||(harmony.intent_kind>=8&&harmony.intent_kind<=10))&&harmony.intent_scale)
        context=hb_key_collection_context(context,harmony.intent_target,harmony.intent_scale);
    if(!context.active||!harmony.valid||
        (!context.blues&&context.source_root==context.target_root&&context.source_mask==context.target_mask))return harmony;
    for(int slot=0;slot<4;slot++){
        hb_key_harmony_cache *cached=&instance->key_cache[slot];
        if(cached->valid&&cached->major_choice==hb_policy_value(instance,HB_P_DOMINANT)&&cached->minor_choice==hb_policy_value(instance,HB_P_DOMINANT_MINOR)&&!memcmp(&cached->context,&context,sizeof(context))&&!memcmp(&cached->input,&harmony,sizeof(harmony)))return cached->output;
    }
    unsigned mask=hb_key_mask(context,hb_harmony_chord_mask(harmony));
    uint8_t notes[12];int count=0,root=mod12(hb_key_map(context,60+harmony.root_pc));
    unsigned original=hb_harmony_chord_mask(harmony);
    int has_intent=((harmony.intent_kind>0&&harmony.intent_kind<4)||(harmony.intent_kind>=8&&harmony.intent_kind<=10));
    int old_target=has_intent?harmony.intent_target:context.source_root;
    int target=has_intent?mod12(hb_key_map(context,60+old_target)):context.target_root;
    int relative=mod12(harmony.root_pc-old_target);
    int functional=harmony.intent_kind==2||(harmony.intent_kind!=7&&relative==7&&(original&(1u<<mod12(harmony.root_pc+4)))&&!(original&(1u<<mod12(harmony.root_pc+11))));
    int leading=harmony.intent_kind!=7&&relative==11&&(original&(1u<<mod12(harmony.root_pc+3)))&&(original&(1u<<mod12(harmony.root_pc+6)));
    /* A modal bVII is not a leading tone merely because the source had vii.
       Explicit leading-tone approaches retain their chromatic function. */
    leading=leading&&(harmony.intent_kind==3||(context.target_mask&(1u<<mod12(target+11))));
    int family=hb_policy_value(instance,hb_target_minor(context.target_mask,target)?HB_P_DOMINANT_MINOR:HB_P_DOMINANT);
    if(!context.blues&&family&&(functional||leading)){
        root=mod12(target+(functional?7:11));
        /* Keep the functional core. In particular, do not turn V into v or
           a leading-tone chord into a chord rooted on the lowered seventh. */
        mask&=~((1u<<mod12(root+3))|(1u<<mod12(root+4))|(1u<<mod12(root+6))|(1u<<mod12(root+7)));
        mask|=(1u<<root)|(1u<<mod12(root+(functional?4:3)))|(1u<<mod12(root+(functional?7:6)));
        if(original&(1u<<mod12(harmony.root_pc+(functional?10:9)))){
            mask&=~((1u<<mod12(root+9))|(1u<<mod12(root+10))|(1u<<mod12(root+11)));
            mask|=1u<<mod12(root+(functional?10:9));
        }
        if(leading){int mapped_old=mod12(hb_key_map(context,60+harmony.root_pc));if(mapped_old!=root)mask&=~(1u<<mapped_old);}
    }
    unsigned detected=hb_key_mask(context,hb_harmony_detected_mask(harmony));
    if(!context.blues&&family&&(functional||leading)){
        /* Detected-form colors and Follow Detected voicings must retain the
           same functional core as the corrected destination harmony. Keep
           sparse input sparse: only move core tones that were actually heard. */
        unsigned heard=hb_harmony_detected_mask(harmony);detected=0;
        for(int pc=0;pc<12;pc++)if(heard&(1u<<pc)){
            int interval=mod12(pc-harmony.root_pc),mapped=mod12(hb_key_map(context,60+pc));
            if(interval==0||interval==(functional?4:3)||interval==(functional?7:6)||interval==(functional?10:9))
                mapped=mod12(root+interval);
            detected|=1u<<mapped;
        }
    }
    if(!context.blues&&harmony.intent_kind>=8&&harmony.intent_kind<=10){
        unsigned collection=hb_function_family(instance,target,context.target_mask,hb_target_minor(context.target_mask,target),0);
        root=mod12(target+hb_nth_scale_interval_from_root(collection,target,1+2*(harmony.intent_kind-8)));
        hb_cp_config identity={0};identity.mode=1;identity.size=3;
        int pitches[HB_CP_VOICES];
        hb_cp_voice_semantic(identity,60+root,harmony.root_pc,original,collection,pitches,&mask);
        identity.size=HB_CP_FOLLOW_DETECTED;
        int voices=hb_cp_voice_semantic(identity,60+root,harmony.root_pc,hb_harmony_detected_mask(harmony),collection,pitches,0);
        detected=0;for(int voice=0;voice<voices;voice++)detected|=1u<<mod12(pitches[voice]);
    }
    if(context.blues)mask=(1u<<root)|(1u<<mod12(root+4))|(1u<<mod12(root+7))|(1u<<mod12(root+10));
    for(int interval=0;interval<12;interval++)if(mask&(1u<<mod12(root+interval)))notes[count++]=(uint8_t)(48+root+interval);
    hb_harmony_t transformed=hb_infer_harmony(notes,count);
    transformed.root_pc=root;transformed.bass_pc=mod12(hb_key_map(context,60+harmony.bass_pc));
    transformed.pitch_mask=(uint16_t)mask;
    transformed.detected_mask=(uint16_t)(context.blues?mask:detected);
    if(transformed.chord_index>=0)transformed.chord_index|=HB_HARMONY_EXPLICIT_TONES;
    transformed.intent_kind=harmony.intent_kind;transformed.intent_target=mod12(hb_key_map(context,60+harmony.intent_target));
    transformed.intent_scale=(uint16_t)hb_key_mask(context,harmony.intent_scale);
    transformed.intent_minor=hb_target_minor(context.target_mask,transformed.intent_target);
    hb_key_harmony_cache *cached=&instance->key_cache[instance->key_cache_cursor++%4];
    cached->major_choice=hb_policy_value(instance,HB_P_DOMINANT);cached->minor_choice=hb_policy_value(instance,HB_P_DOMINANT_MINOR);
    cached->context=context;cached->input=harmony;cached->output=transformed;cached->valid=1;
    return transformed;
}
static int hb_follower_input_scale_index(Inst *instance,int source_root);
static hb_key_context hb_key_baseline(Inst *instance){
    hb_key_context context=g_key_context;
    if(!context.active){
        int root=0;hb_resolve_follower_reference_root(instance,&root);
        context.source_root=context.target_root=mod12(root+g_bus.global_transpose);
        int scale=hb_follower_input_scale_index(instance,root);if(scale<1)scale=1;
        context.source_mask=context.target_mask=hb_explicit_scale_mask(context.source_root,scale);
        context.blues=HB_SCALE_DOMINANT[scale];
    }
    context.active=1;return context;
}
/* Relative mode selects the other tonic of the same major/minor family.
   Always derive from the saved context, never repeatedly shift an active one. */
static void hb_key_relative(hb_key_context *context){
    int minor=(context->target_mask&(1u<<mod12(context->target_root+3)))&&
        !(context->target_mask&(1u<<mod12(context->target_root+4)));
    context->target_root=mod12(context->target_root+(minor?3:9));
    context->target_mask=hb_explicit_scale_mask(context->target_root,minor?1:2);
}
static void hb_sc_set_live(Inst *instance,int kind,int on,int a,int b,int c);
static void hb_sc_resolve(Inst *instance);
static void hb_key_parallel(Inst *instance,int on){
    g_key_preview[0]=0;
    int owner=(int)(instance-g_pool);
    hb_sc_set_live(instance,HB_SC_PARALLEL,on,g_parallel_scale,0,g_sc_latch[owner]?2:g_sc_manual[owner]?1:3);
}
static hb_key_context hb_key_destination(Inst *instance,int source){
    hb_key_context context=g_parallel_on?g_parallel_previous:hb_key_baseline(instance);context.active=1;
    int target=instance->role==1?hb_map_follower_note_unoperated(instance,source):source+g_bus.global_transpose;
    unsigned inherited_mask=g_parallel_on?g_parallel_previous.active?g_parallel_previous.target_mask:context.source_mask:context.target_mask;
    context.target_root=mod12(instance->role==1?target:hb_key_map(context,target));
    /* Retaining the pitch collection selects its mode at the landing degree. */
    if(g_key_scale_mode==2){
        context.blues=HB_SCALE_DOMINANT[HB_PARALLEL_SCALE_IDS[g_parallel_scale]];
        if(g_parallel_scale==18){
            /* A pressed destination is explicit: relative choice supplies its
               major/minor quality without moving away from the chosen pad. */
            int minor=(inherited_mask&(1u<<mod12(context.target_root+3)))&&!(inherited_mask&(1u<<mod12(context.target_root+4)));
            context.target_mask=hb_explicit_scale_mask(context.target_root,minor?2:1);
        }else context.target_mask=hb_explicit_scale_mask(context.target_root,HB_PARALLEL_SCALE_IDS[g_parallel_scale]);
    }
    else if(g_key_scale_mode==0){
        int minor=(context.target_mask&(1u<<mod12(context.target_root+3)))&&!(context.target_mask&(1u<<mod12(context.target_root+4)));
        context.target_mask=hb_explicit_scale_mask(context.target_root,minor?2:1);
    }
    (void)inherited_mask;return context;
}
/* Only an armed destination supplies a prospective scale. Ordinary secondary
   targets and explicit nested cadence paths retain their own local thirds. */
static unsigned hb_key_approach_scale(Inst *instance,int target,unsigned parent){
    int scale=hb_key_for(instance).approach_scale;
    if(!scale)return parent;
    if(scale<0)scale=hb_target_minor(parent,target)?2:1;
    return hb_explicit_scale_mask(mod12(target),scale);
}
static void hb_sc_key_action(Inst *instance,hb_sc_value event);
static void hb_key_commit(Inst *instance,hb_key_context context){
    g_key_armed=0;g_sc_arm_owner=-1;g_key_preview[0]=0;
    int mask=hb_transpose_mask(context.target_mask,-g_bus.global_transpose),detail=context.blues&1;
    if(instance->sc_landing_pitch||instance->key_action){
        /* Only an explicit landing captures its preceding key context. This
           prevents a repeated loop or later humanization from mapping that
           target twice; ordinary notes/settings remain freely rerenderable. */
        hb_key_context before=hb_key_baseline(instance);
        mask|=mod12(before.source_root-g_bus.global_transpose)<<12;
        mask|=(int)hb_transpose_mask(before.source_mask,-g_bus.global_transpose)<<16;
        detail|=instance->sc_landing_pitch<<1;
        detail|=mod12(before.target_root-g_bus.global_transpose)<<9;
        detail|=(int)hb_transpose_mask(before.target_mask,-g_bus.global_transpose)<<13;
        detail|=(before.blues&1)<<25;
    }
    int root=mod12(context.target_root-g_bus.global_transpose);
    if(instance->key_action){
        int action=instance->key_action-1;
        if(action==1)root=(hb_mo_clamp(instance->key_shift,-12,12)+12)|(hb_mo_clamp(instance->key_return_after,0,64)<<5);
        if(action>=2)root=0;
        hb_sc_key_action(instance,(hb_sc_value){1,root,mask,detail|(action<<26),0});
    }else hb_sc_set_live(instance,HB_SC_KEY,1,root,mask,detail);
}
static void hb_key_commit_pending(Inst *instance){
    int action=instance->key_action;instance->key_action=instance->key_pending_action;
    hb_key_commit(instance,instance->key_pending_context);instance->key_action=action;instance->key_pending=0;
}
static const char *HB_KEY_NAMES[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
static const char *hb_key_quality(hb_key_context context){
    if(context.blues)return " Blues";
    return (context.target_mask&(1u<<mod12(context.target_root+3)))&&!(context.target_mask&(1u<<mod12(context.target_root+4)))?"m":"";
}
static void hb_key_land(Inst *instance,int source){instance->sc_landing_pitch=source+1;hb_key_commit(instance,hb_key_destination(instance,source));instance->sc_landing_pitch=0;}
static hb_harmony_t hb_root_only_reference_harmony(int root_pc){
    hb_harmony_t harmony;memset(&harmony,0,sizeof(harmony));
    harmony.valid=1;
    harmony.root_pc=mod12(root_pc);
    harmony.bass_pc=harmony.root_pc;
    harmony.pitch_mask=0x0FFFu;
    harmony.chord_index=-1;
    harmony.confidence=100;
    return harmony;
}
static const char *hb_current_role_name_for_note(const Inst *instance,int note){
    if(!instance||note<0||note>127||!instance->published_follower[note])return "--";
    return hb_follower_degree_role_for_note((Inst*)instance,note);
}
static const char *hb_follower_analysis_role_name(const Inst *instance,int ordinal){
    if(!instance)return "--";
    if(instance->role==1){
        int note=hb_nth_follower_note(instance,ordinal);
        return hb_current_role_name_for_note(instance,note);
    }
    if(instance->role!=0)return "--";
    const Inst *follower=0;
    int note=hb_nth_aggregate_follower_observation(ordinal,&follower);
    return hb_current_role_name_for_note(follower,note);
}
static int hb_nth_clip_active_note(int ordinal){
    uint8_t notes[32];int count=hb_clip_active_notes(notes,32);
    if(ordinal<0||ordinal>=count)return -1;
    for(int i=0;i<count;i++)for(int j=i+1;j<count;j++)if(notes[j]<notes[i]){uint8_t t=notes[i];notes[i]=notes[j];notes[j]=t;}
    return notes[ordinal];
}
static int hb_nth_aggregate_conductor_note(int ordinal){
    uint8_t notes[64];int count=hb_observed_notes(0,notes,64);
    return ordinal>=0&&ordinal<count?notes[ordinal]:-1;
}
static int hb_format_conductor_source(int note,char *buffer,int length){
    if(!buffer||length<=0||note<0||note>127)return snprintf(buffer,(size_t)length,"%s","--");
    int contributors=0,first_index=-1,first_channel=-1;
    for(int instance_index=0;instance_index<HB_MAX_INSTANCES;instance_index++){
        Inst *conductor=&g_pool[instance_index];
        if(!conductor->used||conductor->role!=0||conductor->held_count[note]==0)continue;
        contributors++;
        if(first_index<0){
            first_index=instance_index+1;
            int channel=conductor->source_channel>=0?conductor->source_channel:conductor->resolved_source_channel;
            first_channel=channel;
        }
    }
    if(contributors<=0)return snprintf(buffer,(size_t)length,"%s","--");
    if(contributors>1)return snprintf(buffer,(size_t)length,"%s","multi");
    if(first_channel>=0&&first_channel<16)return snprintf(buffer,(size_t)length,"I%d Ch%d",first_index,first_channel+1);
    return snprintf(buffer,(size_t)length,"I%d Ch?",first_index);
}
static void bus_write(hb_harmony_t harmony){unsigned sequence=__atomic_load_n(&g_bus.seq,__ATOMIC_RELAXED);__atomic_store_n(&g_bus.seq,sequence+1,__ATOMIC_RELEASE);g_bus.harmony=harmony;__atomic_store_n(&g_bus.seq,sequence+2,__ATOMIC_RELEASE);}
static hb_harmony_t bus_read(void){hb_harmony_t harmony;memset(&harmony,0,sizeof(harmony));for(int tries=0;tries<3;tries++){unsigned before=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE),after;if(before&1u)continue;harmony=g_bus.harmony;after=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);if(before==after&&!(after&1u))return harmony;}return harmony;}

static hb_harmony_t hb_observed_read(void){return g_bus.observed_harmony;}
static int hb_harmony_equal_effective(hb_harmony_t left,hb_harmony_t right){
    if(left.valid!=right.valid)return 0;
    if(!left.valid)return 1;
    return left.root_pc==right.root_pc&&left.chord_index==right.chord_index&&left.bass_pc==right.bass_pc&&left.pitch_mask==right.pitch_mask&&hb_harmony_detected_mask(left)==hb_harmony_detected_mask(right)&&left.intent_kind==right.intent_kind&&(!left.intent_kind||(left.intent_target==right.intent_target&&left.intent_minor==right.intent_minor&&left.intent_scale==right.intent_scale));
}
static void hb_effective_write(hb_harmony_t harmony){
    if(!hb_harmony_equal_effective(bus_read(),harmony))bus_write(harmony);
}

enum { HB_LOOKAHEAD_IMMEDIATE=25, HB_LOOKAHEAD_AFTER=26, HB_LOOKAHEAD_COUNT=38 };
static double hb_next_after_beats_for(const Inst *instance){
    static const double delays[12]={0.125,0.25,0.5,1,1.5,2,3,4,6,8,12,16};
    int index=(instance?instance->next_lookahead:g_bus.next_lookahead)-HB_LOOKAHEAD_AFTER;
    return index>=0&&index<12?delays[index]:-1.0;
}
static int hb_next_elapsed_for(const Inst *instance){return (instance?instance->next_lookahead:g_bus.next_lookahead)>=HB_LOOKAHEAD_IMMEDIATE;}
static int hb_next_immediate_for(const Inst *instance){return (instance?instance->next_lookahead:g_bus.next_lookahead)==HB_LOOKAHEAD_IMMEDIATE;}
static int hb_next_enabled_for(const Inst *instance){return (instance?instance->next_lookahead:g_bus.next_lookahead)!=0;}
static double hb_next_lookahead_beats_for(const Inst *instance){
    static const double beats[25]={0.0,0.125,0.25,0.5,1.0,2.0,4.0,-0.125,-0.25,-0.5,-1.0,-2.0,-4.0,1.5,3.0,6.0,-1.5,-3.0,-6.0,8.0,12.0,-8.0,-12.0,16.0,-16.0};
    int index=(instance?instance->next_lookahead:g_bus.next_lookahead);
    return (index>=0&&index<25)?beats[index]:0.0;
}
static double hb_next_lookahead_beats(void){return hb_next_lookahead_beats_for(0);}

/* Positive lookahead starts AFTER its nominal boundary. Negative offsets
   retain their existing late-harmony semantics; anti-buffer cannot turn an
   advance into a delay. The setting remains independent of Follower Buffer. */

static double hb_next_shift_beats_for(const Inst *instance){
    double lookahead=hb_next_lookahead_beats_for(instance);
    if(lookahead<=0.0)return lookahead;
    int setting=(instance?instance->next_anti_buffer_ms:g_bus.next_anti_buffer_ms);
    double guard=setting<0?0.0625*(1u<<(-setting-1)):hb_ms_to_beats(setting);
    return guard<lookahead?lookahead-guard:0.0;
}
static double hb_next_shift_beats(void){return hb_next_shift_beats_for(0);}


static int hb_next_allows_precapture_for(const Inst *instance){
    if(hb_next_elapsed_for(instance))return 0;
    return hb_next_lookahead_beats_for(instance)<=0.0||(instance?instance->next_anti_buffer_ms:g_bus.next_anti_buffer_ms)==0;
}
static int hb_next_allows_precapture(void){return hb_next_allows_precapture_for(0);}

/* Keep the configured value in saved state. Enabling lookahead with a guard
   disables follower capture even while the prediction model is learning. */

static int hb_effective_follower_buffer_for(const Inst *instance){
    if((instance?instance->next_predict:g_bus.next_predict)&&hb_next_elapsed_for(instance))return 0;
    return (instance?instance->next_predict:g_bus.next_predict) && hb_next_enabled_for(instance) &&
        (instance?instance->next_anti_buffer_ms:g_bus.next_anti_buffer_ms)!=0 ? 0 : (instance?instance->boundary_buffer_ms:g_bus.boundary_buffer_ms);
}
static int hb_effective_follower_buffer(void){return hb_effective_follower_buffer_for(0);}


static double hb_follower_capture_beats_for(const Inst *instance){
    int setting=hb_effective_follower_buffer_for(instance);
    double capture=setting<0?0.0625*(1u<<(-setting-1))-hb_ms_to_beats(1):hb_ms_to_beats(setting);
    return capture>0.0?capture:0.0;
}
static double hb_follower_capture_beats(void){return hb_follower_capture_beats_for(0);}

static double hb_next_loop_length(void){
    if(g_movy_present)return (double)g_movy_period/g_movy_ppqn;
    double length=g_bus.clip_loop_end-g_bus.clip_loop_start;
    return length>1e-6?length:0.0;
}
static double hb_next_phase(double playhead){
    double length=hb_next_loop_length();
    if(length<=0.0)return 0.0;
    double phase=playhead-(g_movy_present?0.0:g_bus.clip_loop_start);
    while(phase>=length)phase-=length;
    while(phase<0.0)phase+=length;
    return phase;
}
static double hb_next_normalize_phase(double phase){
    double length=hb_next_loop_length();
    if(length<=0.0)return phase;
    while(phase>=length)phase-=length;
    while(phase<0.0)phase+=length;
    return phase;
}
static double hb_next_record_phase(void){
    double phase=hb_next_phase(hb_clip_playhead());
    double grid=hb_chord_grid_beats();
    if(grid<=0.0)return phase;
    double anticipation=hb_anticipation_beats();
    double shifted=(phase+anticipation)/grid;
    long nearest=(long)(shifted+0.5);
    return hb_next_normalize_phase((double)nearest*grid-anticipation);
}
/* Predictor bookkeeping follows the shared transport, never instance ticks. */
static unsigned next_cache_revision, next_configuration;
static unsigned hb_next_configuration(void){
    /* Controls that alter observed harmonies or their recorded phase. */
    unsigned signature=0;
    int controls[]={g_bus.context,g_bus.sensor_sources,g_bus.clip_context,g_bus.clip_slot,
        g_bus.global_transpose,g_bus.chord_timing,g_bus.anticipation,
        g_bus.chord_timescale,g_bus.stability,g_bus.inference_window_ms};
    for(unsigned index=0;index<sizeof(controls)/sizeof(controls[0]);index++)
        signature=signature*31u+(unsigned)controls[index];
    return signature;
}
static double next_loop_start, next_loop_end;
static hb_harmony_t next_pending;
static double next_pending_beat, next_pending_phase;
static int next_pending_active;
/* Confirmation receipt: an initial harmony establishes a baseline, not a change. */
static hb_harmony_t timing_last_harmony;
static double timing_last_phase;
static int timing_have_baseline, timing_have_transition;
static double hb_next_transport_beat(void){
    if(g_movy_present)return (double)g_movy_tick/g_movy_ppqn;
    double beat=(g_host&&g_host->get_beat_position)?g_host->get_beat_position():-1.0;
    return beat>=0.0?beat:(double)g_bus.clip_clock_ticks/24.0;
}
static int hb_next_is_harmony(hb_harmony_t harmony){
    return harmony.valid&&harmony.chord_index>=0;
}
static void hb_next_reset_knowledge(void){
    g_infer_revision++;g_infer_cached_root=-1;
    next_pending_active=0;
    timing_have_baseline=timing_have_transition=0;
    next_cache_revision=g_bus.cache_rev;
    next_configuration=hb_next_configuration();
    next_loop_start=g_bus.clip_loop_start;
    next_loop_end=g_bus.clip_loop_end;
    g_bus.next_model_locked=0;
    g_bus.next_shift_active=0;
    g_bus.next_learning_count=0;
    g_bus.next_model_count=0;
    g_bus.next_have_playhead=0;
    g_bus.next_last_playhead=0.0;
    g_bus.next_learning_started=0;
    g_bus.next_learning_progress_beats=0.0;
}
/* Retain proven schedules across launches. Keys exclude absolute launch time:
   phases are relative to the first conductor; additional lanes retain their
   relative phase. Fixed storage bounds both memory and callback work. */
#define HB_CLIP_CACHE_CAPACITY 32
typedef struct {
    hb_tick_t revision,period,relative_origin;
    int track,slot;
} hb_clip_cache_lane_t;
typedef struct {
    int count;
    unsigned configuration;
    hb_tick_t rendering;
    hb_clip_cache_lane_t lanes[HB_MAX_INSTANCES];
} hb_clip_cache_key_t;
typedef struct {
    int used,count;
    unsigned long long age;
    hb_clip_cache_key_t key;
    hb_loop_harmony_event_t events[HB_MAX_LOOP_HARMONIES];
} hb_clip_cache_entry_t;
static hb_clip_cache_entry_t g_clip_cache[HB_CLIP_CACHE_CAPACITY];
static hb_clip_cache_key_t g_clip_cache_key;
static unsigned long long g_clip_cache_age;
static int g_clip_cache_key_valid;
static double g_clip_cache_activated;
static hb_tick_t hb_clip_hash(hb_tick_t hash,hb_tick_t value){return (hash^value)*1099511628211ULL;}
/* Chord Form conditions repeat over their full operation cycle, which can
   exceed the clip loop. Event-advanced/evolving forms are not a fixed clock. */
static hb_tick_t hb_conductor_prediction_period(const Inst *instance,const hb_movy_clip_t *clip){
    hb_tick_t period=clip->period;
    for(int index=0;index<HB_MOTION_LANES;index++){
        const hb_motion_lane *lane=&instance->motion.lanes[index];
        if(lane->operation!=HB_MO_CHORD_FORM||!hb_mo_lane_active(&instance->motion,index))continue;
        if(lane->advance||lane->evolve)return 0;
        hb_tick_t cycle=(hb_tick_t)(hb_mo_cycle(lane->cycle)*clip->ppqn)*hb_mo_clamp(lane->every,1,16);
        if(!hb_tick_lcm(period,cycle,&period))return 0;
    }
    return period;
}
static hb_clip_cache_key_t hb_clip_cache_current_key(void){
    hb_clip_cache_key_t key;memset(&key,0,sizeof(key));
    key.configuration=hb_next_configuration();
    key.rendering=14695981039346656037ULL;
    int globals[]={hb_shared_follower_scale(),hb_shared_dominant_scale(),hb_shared_borrowed_scale(),
        hb_global_root_policy(),hb_global_explicit_root(),g_bus.global_input_root};
    for(unsigned index=0;index<sizeof(globals)/sizeof(globals[0]);index++)
        key.rendering=hb_clip_hash(key.rendering,(unsigned)globals[index]);
    hb_tick_t origin=0;
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *instance=&g_pool[index];hb_movy_clip_t *clip=&g_movy_clips[index];
        if(!instance->used||instance->role!=0||!clip->present||!clip->active)continue;
        if(!key.count)origin=clip->origin;
        hb_clip_cache_lane_t *lane=&key.lanes[key.count++];
        lane->track=instance->movy_track>=0?instance->movy_track:index;
        lane->slot=clip->slot;lane->revision=clip->revision;lane->period=hb_conductor_prediction_period(instance,clip);
        if(!lane->period)lane->period=clip->period;
        lane->relative_origin=(clip->origin+lane->period-origin%lane->period)%lane->period;
        for(int operation=0;operation<HB_MOTION_LANES;operation++)
            if(instance->motion.lanes[operation].operation==HB_MO_CHORD_FORM&&hb_mo_lane_active(&instance->motion,operation))
                key.rendering=hb_clip_hash(key.rendering,clip->origin%lane->period);
        for(int field=0;field<HB_POLICY_FIELDS;field++)key.rendering=hb_clip_hash(key.rendering,(unsigned)hb_policy_value(instance,field));
        const unsigned char *config=(const unsigned char *)hb_cp_settings(&instance->player);
        for(unsigned byte=0;byte<sizeof(instance->player.config);byte++)
            key.rendering=hb_clip_hash(key.rendering,config[byte]);
        config=(const unsigned char *)&instance->play;
        for(unsigned byte=0;byte<sizeof(instance->play);byte++)key.rendering=hb_clip_hash(key.rendering,config[byte]);
        for(int lane=0;lane<HB_MOTION_LANES;lane++)key.rendering=hb_clip_hash(key.rendering,instance->motion.revision[lane]);
        key.rendering=hb_clip_hash(key.rendering,instance->motion.held);
        key.rendering=hb_clip_hash(key.rendering,instance->motion.bypass);
    }
    return key;
}
static int hb_clip_cache_equal(const hb_clip_cache_key_t *left,const hb_clip_cache_key_t *right){
    return !memcmp(left,right,sizeof(*left));
}
static void hb_clip_cache_evict_active(void){
    if(!g_clip_cache_key_valid)return;
    for(int index=0;index<HB_CLIP_CACHE_CAPACITY;index++)
        if(g_clip_cache[index].used&&hb_clip_cache_equal(&g_clip_cache[index].key,&g_clip_cache_key))
            g_clip_cache[index].used=0;
}
static void hb_clip_cache_store(void){
    if(!g_clip_cache_key_valid||g_movy_blocked||!g_bus.next_model_locked||!g_bus.next_model_count)return;
    int target=0;
    for(int index=0;index<HB_CLIP_CACHE_CAPACITY;index++){
        hb_clip_cache_entry_t *entry=&g_clip_cache[index];
        if(!entry->used||hb_clip_cache_equal(&entry->key,&g_clip_cache_key)){target=index;break;}
        if(entry->age<g_clip_cache[target].age)target=index;
    }
    hb_clip_cache_entry_t *entry=&g_clip_cache[target];
    entry->key=g_clip_cache_key;entry->used=1;entry->age=++g_clip_cache_age;
    entry->count=g_bus.next_model_count;
    memcpy(entry->events,g_bus.next_model,entry->count*sizeof(entry->events[0]));
}
static int hb_clip_cache_restore(hb_clip_cache_key_t key){
    /* Editing/replacing a known slot invalidates every retained combination
       containing that old slot content, including inactive combinations. */
    for(int index=0;index<HB_CLIP_CACHE_CAPACITY;index++){
        hb_clip_cache_entry_t *entry=&g_clip_cache[index];
        if(!entry->used)continue;
        for(int current=0;current<key.count;current++)for(int old=0;old<entry->key.count;old++){
            hb_clip_cache_lane_t *now=&key.lanes[current],*before=&entry->key.lanes[old];
            if(now->slot>=0&&now->track==before->track&&now->slot==before->slot&&now->revision!=before->revision)
                entry->used=0;
        }
    }
    g_clip_cache_key=key;g_clip_cache_key_valid=key.count>0&&!g_movy_blocked;
    g_clip_cache_activated=hb_next_transport_beat();
    if(!g_clip_cache_key_valid)return 0;
    for(int index=0;index<HB_CLIP_CACHE_CAPACITY;index++){
        hb_clip_cache_entry_t *entry=&g_clip_cache[index];
        if(!entry->used||!hb_clip_cache_equal(&entry->key,&key))continue;
        entry->age=++g_clip_cache_age;
        g_bus.next_model_count=entry->count;
        memcpy(g_bus.next_model,entry->events,entry->count*sizeof(entry->events[0]));
        g_bus.next_model_locked=1;g_infer_revision++;
        return 1;
    }
    return 0;
}
#include "../../../src/clip_timeline_cache.h"
static void hb_commit_observed_harmony(hb_harmony_t harmony);
static void hb_movy_refresh(void){
    int present=0,running=0,blocked=0,count=0;
    hb_tick_t tick=0,period=0,origin=0,revision=14695981039346656037ULL;
    unsigned ppqn=96;
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        hb_movy_clip_t *clip=&g_movy_clips[index];
        if(!g_pool[index].used||!clip->present)continue;
        present=1;tick=clip->tick;running=clip->running;ppqn=clip->ppqn;
        if(g_pool[index].role!=0||!clip->active)continue;
        hb_tick_t prediction_period=hb_conductor_prediction_period(&g_pool[index],clip);
        if(!prediction_period){prediction_period=clip->period;blocked=3;}
        if(!count){period=prediction_period;origin=clip->origin;}
        else if(!hb_tick_lcm(period,prediction_period,&period))blocked=2;
        if(clip->active==2||g_pool[index].dominant_color_held||g_pool[index].dominant_color_latched)blocked=3;
        count++;
        hb_tick_t values[]={(hb_tick_t)index,clip->period,clip->origin,clip->revision,clip->active};
        for(unsigned value=0;value<sizeof(values)/sizeof(values[0]);value++)revision=(revision^values[value])*1099511628211ULL;
    }
    if(present&&!count)blocked=1;
    if(revision==g_movy_revision&&g_movy_blocked==4)blocked=4;
    int restart=present&&running&&(!g_movy_running||tick<g_movy_tick);
    g_movy_tick=tick;g_movy_running=running;
    if(restart)g_clip_cache_activated=(double)tick/ppqn;
    hb_clip_cache_key_t cache_key=hb_clip_cache_current_key();
    int configuration_changed=0;
    if(present!=g_movy_present||revision!=g_movy_revision||blocked!=g_movy_blocked||
       (present&&!hb_clip_cache_equal(&cache_key,&g_clip_cache_key))){
        g_movy_present=present;g_movy_period=period;g_movy_origin=origin;g_movy_ppqn=ppqn;
        g_movy_revision=revision;g_movy_blocked=blocked;
        configuration_changed=1;
        hb_next_reset_knowledge();
        hb_clip_cache_restore(cache_key);
        if(present)hb_effective_write(g_bus.observed_harmony);
        /* Do not seed a newly launched clip with the previous clip's chord. */
    }
    hb_timeline_refresh(configuration_changed,restart);
}
static void hb_next_record_observed(hb_harmony_t harmony){
    if(g_movy_present&&g_movy_blocked)return;
    if(!hb_next_is_harmony(harmony)||g_bus.next_model_locked)return;
    double phase=next_pending_phase;
    if(g_bus.next_learning_count>0){
        hb_loop_harmony_event_t *last=&g_bus.next_learning[g_bus.next_learning_count-1];
        if(hb_harmony_equal_effective(last->harmony,harmony))return;
    }
    if(g_bus.next_learning_count>=HB_MAX_LOOP_HARMONIES){g_movy_blocked=4;return;}
    hb_loop_harmony_event_t *event=&g_bus.next_learning[g_bus.next_learning_count++];
    event->phase=phase;
    event->harmony=harmony;
    g_infer_revision++;
}
static int hb_next_model_accepts_observed(hb_harmony_t harmony,double phase){
    if(!g_bus.next_model_locked||g_bus.next_model_count<=0||!harmony.valid)return 1;
    double length=hb_next_loop_length();
    if(length<=0.0)return 1;

    /* Compare against the UNSHIFTED learned schedule. Lookahead is an output
       transform and must never make the predictor disagree with itself. */
    int current=-1;double best_age=1e99;
    for(int index=0;index<g_bus.next_model_count;index++){
        double age=phase-g_bus.next_model[index].phase;
        if(age<0.0)age+=length;
        if(age<best_age){best_age=age;current=index;}
    }
    if(current>=0&&hb_harmony_equal_effective(g_bus.next_model[current].harmony,harmony))return 1;

    /* With Free chord timing, realtime inference can commit a few milliseconds
       before the learned transition. Accept the immediately upcoming learned
       harmony within a small musical tolerance instead of falsely relearning. */
    double tolerance=hb_chord_grid_beats()>0.0?1e-4:0.125;
    for(int index=0;index<g_bus.next_model_count;index++){
        double distance=g_bus.next_model[index].phase-phase;
        if(distance<0.0)distance+=length;
        if(distance<=tolerance+1e-6&&
           hb_harmony_equal_effective(g_bus.next_model[index].harmony,harmony))return 1;
    }
    return 0;
}
static void hb_next_begin_relearning(void){
    hb_clip_cache_evict_active();
    hb_next_reset_knowledge();
}
static void hb_commit_observed_harmony(hb_harmony_t harmony){
    /* Unknown pitches and gaps cannot replace the last established harmony. */
    if(!hb_next_is_harmony(harmony))return;
    /* Observed commits alone clear released owners; prediction and master
       transpose never enter this path as a new chord change. */
    if(hb_next_is_harmony(g_bus.observed_harmony)&&
       (g_bus.observed_harmony.root_pc!=harmony.root_pc||hb_harmony_chord_mask(g_bus.observed_harmony)!=hb_harmony_chord_mask(harmony))){
        for(int owner=0;owner<HB_MAX_INSTANCES;owner++)if(g_pool[owner].used){
            hb_chord_player *player=&g_pool[owner].player;
            if(!hb_cp_settings(player)->clear_harmony||!hb_cp_settings(player)->latch)continue;
            for(int key=0;key<HB_CP_KEYS;key++)if(player->keys[key].used&&!player->keys[key].held&&!player->keys[key].release_end)
                memset(&player->keys[key],0,sizeof(player->keys[key]));
        }
    }
    g_bus.observed_harmony=harmony;
    if(!next_pending_active||!hb_harmony_equal_effective(next_pending,harmony)){
        next_pending=harmony;
        next_pending_beat=hb_next_transport_beat();
        next_pending_phase=hb_next_record_phase();
        next_pending_active=1;
    }
    if(!g_bus.next_predict||!hb_next_enabled_for(0)||!g_bus.next_model_locked)hb_effective_write(harmony);
}
static void hb_next_confirm_observed(double beat){
    if(!next_pending_active)return;
    double bpm=(g_host&&g_host->get_bpm)?g_host->get_bpm():120.0;
    if(bpm<=0.0)bpm=120.0;
    /* Confirm for at least 25ms; retain the original transition phase. */
    double confirmation=(g_bus.inference_window_ms>25?g_bus.inference_window_ms:25)*bpm/60000.0;
    if(beat-next_pending_beat+1e-9<confirmation)return;
    hb_harmony_t harmony=next_pending;
    double phase=next_pending_phase;
    int observed_change=timing_have_baseline&&!hb_harmony_equal_effective(timing_last_harmony,harmony);
    if(g_bus.next_model_locked&&!hb_next_model_accepts_observed(harmony,phase)){
        hb_next_begin_relearning();
        next_pending_phase=phase;
    }
    hb_next_record_observed(harmony);
    if(!g_movy_blocked){
        if(observed_change){
            timing_last_phase=phase;
            timing_have_transition=1;
        }
        timing_last_harmony=harmony;
        timing_have_baseline=1;
    }
    next_pending_active=0;
}

static int hb_next_upcoming_event(double phase);
static int hb_next_model_event_for_phase_for(const Inst *instance,double phase,int shifted){
    if(shifted&&hb_next_immediate_for(instance))return hb_next_upcoming_event(phase);
    double delay=hb_next_after_beats_for(instance);
    if(shifted&&delay>=0.0){
        int current=hb_next_model_event_for_phase_for(instance,phase,0);
        if(current<0)return current;
        double age=phase-g_bus.next_model[current].phase;
        if(age<0.0)age+=hb_next_loop_length();
        return age>=delay?hb_next_upcoming_event(phase):current;
    }
    if(!g_bus.next_model_locked||g_bus.next_model_count<=0)return -1;
    double length=hb_next_loop_length();
    if(length<=0.0)return -1;
    double lookahead=shifted?hb_next_shift_beats_for(instance):0.0;
    int best=-1;double best_age=1e99;
    for(int index=0;index<g_bus.next_model_count;index++){
        double event_phase=hb_next_normalize_phase(g_bus.next_model[index].phase-lookahead);
        double age=phase-event_phase;
        if(age<0.0)age+=length;
        if(age<best_age){best_age=age;best=index;}
    }
    if(best>=0&&g_movy_present&&g_clip_cache_key_valid){
        /* Before the new clip's first event, keep the actual previous harmony.
           A cached wraparound chord has not sounded in this launch yet. Query
           phases ahead of the playhead (buffered followers) remain usable. */
        double now=hb_next_transport_beat();
        double delta=phase-hb_next_phase(hb_clip_playhead());
        if(delta< -1e-6)delta+=length;
        if(now+delta-best_age+lookahead+1e-6<g_clip_cache_activated)return -1;
    }
    return best;
}
static int hb_next_model_event_for_phase(double phase,int shifted){return hb_next_model_event_for_phase_for(0,phase,shifted);}

static int hb_next_upcoming_event(double phase){
    if(!g_bus.next_model_locked||g_bus.next_model_count<=0)return -1;
    double length=hb_next_loop_length();
    if(length<=0.0)return -1;
    int best=-1;double best_distance=1e99;
    for(int index=0;index<g_bus.next_model_count;index++){
        double distance=g_bus.next_model[index].phase-phase;
        if(distance<=0.0)distance+=length;
        if(distance<best_distance){best_distance=distance;best=index;}
    }
    return best;
}
static void hb_next_promote_learning(void){
    /* A full loop LENGTH of observation covers every circular clip phase even
       when learning started mid-loop. No explicit playhead-wrap event needed. */
    if(g_bus.next_model_locked||g_movy_blocked)return;
    if(g_bus.next_learning_count<=0&&hb_next_is_harmony(g_bus.observed_harmony)){
        g_bus.next_learning[0].phase=0.0;
        g_bus.next_learning[0].harmony=g_bus.observed_harmony;
        g_bus.next_learning_count=1;
    }
    if(g_bus.next_learning_count<=0)return;
    g_bus.next_model_count=g_bus.next_learning_count;
    for(int index=0;index<g_bus.next_model_count;index++)g_bus.next_model[index]=g_bus.next_learning[index];
    g_bus.next_learning_count=0;
    g_bus.next_model_locked=1;
    g_infer_revision++;
    g_bus.next_learning_started=1;
    hb_clip_cache_store();
}
static void hb_next_apply_effective(double playhead){
    if(!g_bus.next_predict||!hb_next_enabled_for(0)||!g_bus.next_model_locked||g_bus.next_model_count<=0){
        g_bus.next_shift_active=0;
        hb_effective_write(g_bus.observed_harmony);
        return;
    }
    double phase=hb_next_phase(playhead);
    int index=hb_next_model_event_for_phase(phase,1);
    if(index<0){g_bus.next_shift_active=0;hb_effective_write(g_bus.observed_harmony);return;}
    hb_loop_harmony_event_t *event=&g_bus.next_model[index];
    hb_effective_write(event->harmony);
    /* Report a shift whenever the signed offset selects another event. */
    g_bus.next_shift_active=index!=hb_next_model_event_for_phase(phase,0);
}
static void hb_next_update_playhead(int frames,int sample_rate){
    (void)frames;(void)sample_rate;
    if(g_movy_present&&g_movy_blocked){hb_effective_write(g_bus.observed_harmony);return;}
    double beat=hb_next_transport_beat();
    double length=hb_next_loop_length();
    if(next_configuration!=hb_next_configuration()||(!g_movy_present&&(next_cache_revision!=g_bus.cache_rev||next_loop_start!=g_bus.clip_loop_start||next_loop_end!=g_bus.clip_loop_end))){
        hb_next_reset_knowledge();
        if(hb_next_is_harmony(g_bus.observed_harmony))hb_commit_observed_harmony(g_bus.observed_harmony);
    }
    double delta=g_bus.next_have_playhead?beat-g_bus.next_last_playhead:0.0;
    /* A backward seek/restart is not elapsed playback. Keep a locked model,
       but discard an incomplete pass rather than promoting missing phases. */
    if(delta<0.0||delta>length){
        if(!g_bus.next_model_locked)hb_next_reset_knowledge();
        next_pending_active=0;
        delta=0.0;
    }
    hb_next_confirm_observed(beat);
    /* A missing transition produces no commit to compare. Validate the held
       observed harmony too, after the complete-event confirmation window.
       Never treat a pre-launch cached event as a missed live observation. */
    if(g_movy_present&&g_clip_cache_key_valid&&g_bus.next_model_locked){
        double phase=hb_next_phase(hb_clip_playhead());
        int expected=hb_next_model_event_for_phase(phase,0);
        if(expected>=0){
            double age=phase-g_bus.next_model[expected].phase;if(age<0)age+=length;
            double bpm=(g_host&&g_host->get_bpm)?g_host->get_bpm():120.0;
            double grace=(g_bus.inference_window_ms+25)*(bpm>0?bpm:120.0)/60000.0;
            if(grace<0.125)grace=0.125;
            if(age>grace&&beat-age+1e-6>=g_clip_cache_activated&&
               !hb_harmony_equal_effective(g_bus.next_model[expected].harmony,g_bus.observed_harmony)){
                hb_next_begin_relearning();
                if(hb_next_is_harmony(g_bus.observed_harmony))hb_commit_observed_harmony(g_bus.observed_harmony);
            }
        }
    }
    if(!g_bus.next_model_locked&&length>0.0&&g_bus.next_learning_count>0){
        if(g_bus.next_learning_started)g_bus.next_learning_progress_beats+=delta;
        g_bus.next_learning_started=1;
        if(g_bus.next_learning_progress_beats+1e-6>=length)hb_next_promote_learning();
    }
    g_bus.next_last_playhead=beat;
    g_bus.next_have_playhead=1;
    hb_next_apply_effective(hb_clip_playhead());
}

static double hb_next_effective_boundary_for(const Inst *instance,double absolute_beat){
    if(g_movy_blocked||!(instance?instance->next_predict:g_bus.next_predict)||!hb_next_enabled_for(instance)||!g_bus.next_model_locked||g_bus.next_model_count<=0)return -1.0;
    double length=hb_next_loop_length();
    if(length<=0.0)return -1.0;
    double phase=hb_next_phase(hb_clip_playhead());
    double delay=hb_next_after_beats_for(instance);
    if(delay>=0.0){
        int current=hb_next_model_event_for_phase_for(instance,phase,0),next=hb_next_upcoming_event(phase);
        if(current<0||next<0)return -1.0;
        double age=phase-g_bus.next_model[current].phase;if(age<0.0)age+=length;
        double remaining=g_bus.next_model[next].phase-phase;if(remaining<=0.0)remaining+=length;
        double wait=delay-age;
        if(wait>=0.0&&wait<remaining)remaining=wait;
        return absolute_beat+remaining;
    }
    double lookahead=hb_next_shift_beats_for(instance);
    double best=1e99;
    for(int index=0;index<g_bus.next_model_count;index++){
        double shifted=hb_next_normalize_phase(g_bus.next_model[index].phase-lookahead);
        double distance=shifted-phase;
        if(distance< -1e-6)distance+=length;
        else if(distance<0.0)distance=0.0;
        if(distance<best)best=distance;
    }
    return best<1e98?absolute_beat+best:-1.0;
}
static double hb_next_effective_boundary(double absolute_beat){return hb_next_effective_boundary_for(0,absolute_beat);}


/* A predicted note uses a private harmony context, never a speculative bus write. */

static int hb_harmony_knowledge_ready_for(const Inst *instance){
    return (instance?instance->next_predict:g_bus.next_predict) &&
        g_bus.next_model_locked && g_bus.next_model_count>0 &&
        !g_movy_blocked && hb_next_loop_length()>0.0;
}
static int hb_render_shift_ready_for(const Inst *instance){
    /* Prediction knowledge remains usable at Lookahead Off. This narrower
       gate is only for behavior that moves effective/rendering time. */
    return hb_harmony_knowledge_ready_for(instance) &&
        hb_next_enabled_for(instance);
}

static double hb_follower_playback_target(Inst *instance,int slot){
    if(instance->follower_queue_harmony_beat[slot]>=0.0 && hb_render_shift_ready_for(instance))
        return instance->follower_queue_quant_beat[slot];
    return instance->follower_queue_target_beat[slot];
}
/* A manually selected Next lasts until one actual chord boundary. Reads only
   test expiry; MIDI/tick/gesture edges clear ownership before processing. */
static int hb_next_touch_expired(const Inst *instance){
    if(!instance->next_touch_mask)return 0;
    double now=hb_current_beat();
    if(now<instance->next_touch_started)return 1;
    if(instance->next_touch_until>=0&&now>=instance->next_touch_until)return 1;
    if((instance->next_touch_until<0||!hb_harmony_knowledge_ready_for(instance))&&g_bus.observed_harmony.valid&&
       instance->next_touch_observed.valid&&!hb_harmony_equal_effective(instance->next_touch_observed,g_bus.observed_harmony))return 1;
    return 0;
}
static void hb_next_touch_clear_expired(Inst *instance){
    if(!hb_next_touch_expired(instance))return;
    unsigned long long mask=instance->next_touch_mask&~instance->motion.gesture_persistent;
    hb_mo_end_lanes(&instance->motion,mask);
    instance->next_touch_mask=0;
}
static void hb_next_touch_arm(Inst *instance,int lane){
    instance->next_touch_mask|=1ULL<<lane;
    instance->next_touch_started=hb_current_beat();instance->next_touch_until=-1;
    instance->next_touch_observed=g_bus.observed_harmony;
    if(hb_harmony_knowledge_ready_for(instance)){
        double phase=hb_next_phase(hb_clip_playhead()),length=hb_next_loop_length(),nearest=1e99;
        int current=hb_next_model_event_for_phase_for(instance,phase,0);
        hb_harmony_t harmony=current>=0?g_bus.next_model[current].harmony:g_bus.observed_harmony;
        for(int e=0;e<g_bus.next_model_count;e++){
            if(hb_harmony_equal_effective(harmony,g_bus.next_model[e].harmony))continue;
            double distance=g_bus.next_model[e].phase-phase;if(distance<=0)distance+=length;
            if(distance<nearest)nearest=distance;
        }
        if(nearest<1e98)instance->next_touch_until=instance->next_touch_started+nearest;
    }
}
static hb_harmony_t hb_render_harmony(Inst *instance){
    hb_harmony_t override;if(hb_override_read(instance,&override))return override;
    hb_harmony_t harmony=instance->render_harmony_active?instance->render_harmony:bus_read();
    if(!instance->render_harmony_active&&hb_render_shift_ready_for(instance)){
        int event=hb_next_model_event_for_phase_for(instance,hb_next_phase(hb_clip_playhead()),1);
        if(event>=0)harmony=g_bus.next_model[event].harmony;
    }
    for(int lane=0;lane<HB_MOTION_LANES;lane++){
        if(hb_mo_operation(&instance->motion,lane)!=HB_MO_HARMONY||!hb_mo_lane_active(&instance->motion,lane))continue;
        if((instance->next_touch_mask&(1ULL<<lane))&&!(instance->motion.gesture_persistent&(1ULL<<lane))&&hb_next_touch_expired(instance))continue;
        double choice;
        if(!hb_mo_value_at(&instance->motion,lane,hb_motion_position(instance),hb_motion_condition_position(),0,&choice))continue;
        harmony=g_bus.observed_harmony;
        if(hb_harmony_knowledge_ready_for(instance)){
            double phase=hb_next_phase(hb_clip_playhead());
            int wants_next=choice>=50.0;
            /* An explicit Next operation selects the next observed transition,
               independently of the automatic lookahead/anticipation window. */
            int event=wants_next?hb_next_upcoming_event(phase):hb_next_model_event_for_phase_for(instance,phase,0);
            if(event>=0)harmony=g_bus.next_model[event].harmony;
        }
    }
    return harmony;
}

static void hb_trail_boundary(Inst *instance){
    hb_harmony_t harmony=hb_render_harmony(instance);
    /* A render anticipation remaps visible history but does not end its
       current-chord window. The unshifted timeline owns the clear boundary. */
    if(hb_harmony_knowledge_ready_for(instance)){
        int event=hb_next_model_event_for_phase_for(instance,hb_next_phase(hb_clip_playhead()),0);
        if(event>=0)harmony=g_bus.next_model[event].harmony;
    }
    unsigned signature=hb_harmony_chord_mask(harmony)|((unsigned)(harmony.root_pc+1)<<12);
    if(signature!=instance->trail_signature){instance->trail_signature=signature;instance->trail_chord++;}
}

/* Internal subscribers augment the existing stock-track MIDI broadcast.
   Hosted HB instances share this module, like the existing harmony bus. */
static void hb_receiver_reset(Inst *receiver){
    for(int pitch=0;pitch<128;pitch++)if(receiver->receiver_sounding[pitch])receiver->role_flush_pending[pitch]=1;
    receiver->role_flush_cursor=0;receiver->receiver_count=0;
    memset(receiver->receiver_refs,0,sizeof(receiver->receiver_refs));
    memset(receiver->receiver_sounding,0,sizeof(receiver->receiver_sounding));
}
static void hb_receiver_remove_source(Inst *source){
    int id=(int)(source-g_pool);
    if(id<0||id>=HB_MAX_INSTANCES)return;
    for(int i=0;i<HB_MAX_INSTANCES;i++){
        Inst *r=&g_pool[i];if(!r->used||r->role!=3)continue;
        int write=0;
        for(int q=0;q<r->receiver_count;q++)if(r->receiver_queue[q].source!=id)r->receiver_queue[write++]=r->receiver_queue[q];
        r->receiver_count=write;
        for(int pitch=0;pitch<128;pitch++){
            if(!r->receiver_refs[id][pitch])continue;
            r->receiver_refs[id][pitch]=0;
            int held=0;for(int j=0;j<HB_MAX_INSTANCES;j++)held+=r->receiver_refs[j][pitch];
            if(!held&&r->receiver_sounding[pitch]){
                r->role_flush_pending[pitch]=1;r->role_flush_cursor=0;r->receiver_sounding[pitch]=0;
            }
        }
    }
}
static int hb_send_render_raw(Inst *source,const uint8_t input_packet[4],int suppress_external){
    if(source->movy_track>=0)suppress_external=0; /* Private input is not a destination-track echo. */
    /* Track fader affects routed note attacks only. Local audio already has
       its own mixer gain; recorded inputs and note-off ownership stay intact. */
    uint8_t scaled_packet[4];memcpy(scaled_packet,input_packet,4);
    const uint8_t *packet=scaled_packet;
    if((packet[1]&0xf0)==0x90&&packet[3]){
        if(source->render_velocity_gain==0)return suppress_external?0:4;
        int velocity=(packet[3]*source->render_velocity_gain+5000)/10000;
        scaled_packet[3]=(uint8_t)(velocity<1?1:velocity>127?127:velocity);
    }
    int id=(int)(source-g_pool),channel=packet[1]&15;
    if(source->role!=3&&id>=0&&id<HB_MAX_INSTANCES){
        for(int i=0;i<HB_MAX_INSTANCES;i++){
            Inst *r=&g_pool[i];
            if(!r->used||r->role!=3||r->source_channel!=channel)continue;
            if(r->receiver_count>=256){hb_receiver_reset(r);continue;} // fail closed, no hanging voices
            r->receiver_queue[r->receiver_count++]=(hb_rx_event){(uint8_t)id,packet[2],packet[3],(uint8_t)((packet[1]&0xf0)==0xb0?2:((packet[1]&0xf0)==0x90&&packet[3]>0))};
        }
    }
    if(suppress_external)return 0; // preserve stock-channel self-echo guard
    return g_host&&g_host->midi_inject_to_move?g_host->midi_inject_to_move(packet,4):0;
}
static void hb_motion_flush_render(Inst *instance){
    uint8_t message[3];
    while(hb_mo_pop(&instance->motion_render,message)){
        uint8_t packet[4]={(uint8_t)(0x20|(message[0]>>4)),message[0],message[1],message[2]};
        hb_send_render_raw(instance,packet,instance->motion_render_suppress[message[0]&15]);
    }
}
static int hb_send_render(Inst *source,const uint8_t packet[4],int suppress_external){
    if(source->movy_track>=0)suppress_external=0;
    if(!source->motion_render.strict_ownership&&!source->motion_event_owner&&hb_rr_pattern(source)<2&&!hb_rr_active(&source->motion_render.rhythm)&&!hb_mo_enabled(&source->motion)&&!source->motion_render.owned&&!source->motion_render.count)
        return hb_send_render_raw(source,packet,suppress_external);
    source->motion_render_suppress[packet[1]&15]=suppress_external;
    hb_motion_output(source,&source->motion_render,packet+1);
    hb_motion_flush_render(source);
    return suppress_external?0:4;
}
static int hb_receiver_tick(Inst *r,uint8_t output[][3],int lengths[],int capacity){
    int consumed=0,emitted=0;
    while(consumed<r->receiver_count&&emitted<capacity){
        hb_rx_event e=r->receiver_queue[consumed++];
        if(e.on==2){output[emitted][0]=0xb0;output[emitted][1]=e.pitch;output[emitted][2]=e.velocity;lengths[emitted++]=3;continue;}
        uint8_t *refs=&r->receiver_refs[e.source][e.pitch];
        if(e.on){
            if(*refs==255){hb_receiver_reset(r);return emitted;}
            ++*refs;r->receiver_sounding[e.pitch]=1;
        }else{
            if(!*refs)continue;
            --*refs;int held=0;for(int j=0;j<HB_MAX_INSTANCES;j++)held+=r->receiver_refs[j][e.pitch];
            if(held)continue;
            r->receiver_sounding[e.pitch]=0;
        }
        output[emitted][0]=e.on?0x90:0x80;output[emitted][1]=e.pitch;output[emitted][2]=e.on?e.velocity:0;
        lengths[emitted++]=3;
    }
    if(consumed){
        for(int q=consumed;q<r->receiver_count;q++)r->receiver_queue[q-consumed]=r->receiver_queue[q];
        r->receiver_count-=consumed;
    }
    return emitted;
}

/* Stable source-scale assignment, shared by ordinary Closest and conductor
   reinterpretation. Cache pitch classes; each note chooses its nearest octave. */
static int hb_closest_diverse(Inst *instance,int pitch,int source_root,unsigned source_mask,unsigned target_mask){
    source_mask&=4095u;target_mask&=4095u;
    if(!source_mask||!target_mask||!(source_mask&(1u<<mod12(pitch))))return hb_cs_nearest(pitch,target_mask);
    source_root=mod12(source_root);
    int nominal[12],outputs[12],count=0,source_index=0;unsigned allowed[12];
    for(int interval=0;interval<12;interval++)if(source_mask&(1u<<mod12(source_root+interval))){
        if(mod12(source_root+interval)==mod12(pitch))source_index=count;
        nominal[count]=48+source_root+interval;allowed[count++]=target_mask;
    }
    if(!hb_cached_closest_assignment(&instance->closest_assignments,count,nominal,allowed,outputs))return hb_cs_nearest(pitch,target_mask);
    return hb_cs_nearest_pc(pitch,mod12(outputs[source_index]));
}
static int hb_key_active_pitch(Inst *instance,int pitch,hb_harmony_t harmony){
    hb_key_context context=hb_key_for(instance);
    if(!context.active){
        int root=0;hb_resolve_follower_reference_root(instance,&root);
        context.active=1;context.source_root=context.target_root=mod12(root+g_bus.global_transpose);
        context.source_mask=context.target_mask=hb_transpose_mask(hb_follower_input_scale(instance,root),g_bus.global_transpose);
    }
    unsigned active=context.blues?0:hb_dominant_scale_mask(instance,harmony,context.target_root);
    if(active)context.target_mask=active;
    return hb_key_map(context,pitch);
}
#include "../../../src/key_travel.h"
#include "../../../src/key_voicing.h"
static int hb_key_conductor_pitch(Inst *instance,int pitch){
    hb_harmony_t target=hb_key_harmony(instance,hb_render_harmony(instance));
    int mapped=hb_key_active_pitch(instance,pitch,target);
    /* A conductor defines harmony; it cannot choose its next root by snapping
       to the preceding chord. Closest changes register, never scale degree. */
    if(!g_key_conductor_travel||!hb_key_for(instance).active)return mapped;
    return hb_travel_register(pitch,mapped,g_key_conductor_travel>=4?g_key_conductor_travel:4);
}
static void hb_render_conductor_event(Inst *instance,int note,int velocity,int is_on,int is_off,int recv_channel){
    if(!instance||instance->role!=0||instance->render_channel<0)return;
    if(!(is_on||is_off)||note<0||note>127)return;
    /* Global Transpose is the master musical transpose, so every rendered
       destination must hear the same pitch shift as the local conductor. */
    int rendered_note=is_on?hb_key_conductor_pitch(instance,note+g_bus.global_transpose):instance->mapped[note];
    if(rendered_note<0)rendered_note=hb_key_pitch(instance,note+g_bus.global_transpose);
    if(rendered_note<0)rendered_note=0;
    if(rendered_note>127)rendered_note=127;
    uint8_t packet[4];
    packet[0]=(uint8_t)(0x20 | (is_on?0x09:0x08));
    packet[1]=(uint8_t)((is_on?0x90:0x80) | (instance->render_channel & 0x0F));
    packet[2]=(uint8_t)(rendered_note & 0x7F);
    packet[3]=(uint8_t)(is_on?velocity:0);
    int sent=hb_send_render(instance,packet,instance->render_channel==recv_channel);
    if(sent==4){instance->render_count++;instance->render_last_note=rendered_note;}
    else instance->render_fail_count++;
}

static void hb_render_follower_event(Inst *instance,int mapped_note,int velocity,int is_on,int is_off,int recv_channel){
    if(!instance||instance->role!=1||instance->render_channel<0)return;
    if(!(is_on||is_off)||mapped_note<0||mapped_note>127)return;
    /* The process path has already chosen and stored the pitch for this source
       note. Render must mirror that exact transformed event, not recompute it.
       In particular, note-off must use the same mapped pitch as note-on. */
    uint8_t packet[4];
    packet[0]=(uint8_t)(0x20 | (is_on?0x09:0x08)); /* cable 2 + CIN */
    packet[1]=(uint8_t)((is_on?0x90:0x80) | (instance->render_channel & 0x0F));
    packet[2]=(uint8_t)(mapped_note & 0x7F);
    packet[3]=(uint8_t)(is_on?velocity:0);
    int sent=hb_send_render(instance,packet,instance->render_channel==recv_channel);
    if(sent==4){instance->render_count++;instance->render_last_note=mapped_note;}
    else instance->render_fail_count++;
}

static int hb_inject_follower_note(Inst *instance,int mapped,int velocity,int is_on){
    if(!instance||instance->render_channel<0)return 0;
    uint8_t packet[4];
    packet[0]=(uint8_t)(0x20 | (is_on?0x09:0x08));
    packet[1]=(uint8_t)((is_on?0x90:0x80) | (instance->render_channel & 0x0F));
    packet[2]=(uint8_t)(mapped & 0x7F);
    packet[3]=(uint8_t)(is_on?velocity:0);
    int sent=hb_send_render(instance,packet,0);
    if(sent==4){instance->render_count++;instance->render_last_note=mapped;return 1;}
    instance->render_fail_count++;return 0;
}
static int hb_queue_follower_event(Inst *instance,int note,int velocity,int is_on,int channel){
    /* Chord Grid, Quant Grid, Anticipation and Buffer are follower-render
       timing only. Conductor events must never enter this subsystem. */
    if(!instance||instance->role!=1||instance->follower_queue_count>=64)return 0;
    int slot=instance->follower_queue_count++;
    instance->key_queue[slot]=hb_key_for(instance);
    instance->follower_queue_input[slot]=hb_input_get(instance,note);
    double beat=hb_current_beat();
    instance->follower_queue_note[slot]=(uint8_t)(note&0x7F);
    instance->follower_queue_velocity[slot]=(uint8_t)(velocity&0x7F);
    instance->follower_queue_on[slot]=(uint8_t)(is_on?1:0);
    instance->follower_queue_channel[slot]=(uint8_t)(channel&0x0F);
    instance->follower_queue_origin[slot]=(uint8_t)(instance->movy_playback!=0);
    instance->trail_queue_serial[slot]=instance->trail_input[(note&127)+(instance->movy_playback?128:0)];
    memcpy(instance->motion_follower_events[slot],instance->motion.event_override?instance->motion.event_override:instance->motion.events,sizeof(instance->motion.events));
    instance->follower_queue_target_beat[slot]=-1.0;
    instance->follower_queue_quant_beat[slot]=-1.0;
    instance->follower_queue_harmony_beat[slot]=-1.0;
    instance->follower_queue_arrival_beat[slot]=beat;

    /* Repeat Arp owns its timing through Rate/Phase. Keep input in the
       conductor-first audio queue, but never capture either edge to a
       follower boundary or inherit a delayed note-off. */
    if(hb_cp_playback(&instance->player)==1){
        instance->follower_queue_age_frames[slot]=0;
        return 1;
    }
    if(is_on){
        double capture=hb_follower_capture_beats_for(instance);
        double target;
        double learned_boundary=hb_next_effective_boundary_for(instance,beat);
        if(learned_boundary>=0.0){
            /* Prediction owns the harmonic boundary; Quant Grid remains an
               independent follower-only capture source. */
            target=hb_follower_capture_target(beat,0.0,0.0,hb_quant_grid_beats_for(instance),capture);
            instance->follower_queue_quant_beat[slot]=target;
            double distance=learned_boundary-beat;
            if(distance>=-1e-6&&distance<=capture+1e-6&&
               (hb_next_allows_precapture_for(instance)||distance<=1e-6)){
                instance->follower_queue_harmony_beat[slot]=learned_boundary;
                if(target<0.0||learned_boundary<target)target=learned_boundary;
            }
        }else{
            target=hb_follower_capture_target(
                beat,
                hb_chord_grid_beats(),
                hb_anticipation_beats(),
                hb_quant_grid_beats_for(instance),
                capture);
        }
        instance->follower_queue_target_beat[slot]=target;
    }else{
        /* Preserve articulation. A note-off inherits the exact delay applied
           to its matching note-on. If the ON is still queued, derive that delay
           from its arrival/target pair; otherwise use the delay remembered when
           the ON actually crossed the queue and became sounding. */
        double delay=instance->follower_origin_delay[instance->movy_playback!=0][note];
        for(int index=instance->follower_queue_count-2;index>=0;index--){
            if(instance->follower_queue_note[index]!=note||!instance->follower_queue_on[index]||
               instance->follower_queue_channel[index]!=(channel&15)||instance->follower_queue_origin[index]!=(instance->movy_playback!=0))continue;
            double pending_target=hb_follower_playback_target(instance,index);
            if(pending_target>=0.0){
                double queued_delay=pending_target-instance->follower_queue_arrival_beat[index];
                if(queued_delay>delay)delay=queued_delay;
            }
            break;
        }
        if(delay>0.0)instance->follower_queue_target_beat[slot]=beat+delay;
    }

    /* Start at age zero. Uncaptured notes can render on the first release
       attempt; a conductor change may impose at most ONE scheduler-tick
       ordering barrier in hb_release_follower_queue(). Captured notes are
       governed only by target_beat and therefore land exactly on the boundary. */
    instance->follower_queue_age_frames[slot]=0;
    return 1;
}
static int hb_source_interval_to_default_degree(int source_interval){
    /* Fallback only for a chromatic source pitch that is NOT a member of the
       selected/inferred PARENT scale. This mapping is intentionally independent
       of the detected harmony: conductor chord quality may affect role->output,
       but must never affect input->role classification.

       Ambiguous chromatic alterations use stable functional buckets. Parent
       scale membership overrides these buckets whenever it is informative:
       Lydian +6 is degree 4 (#4), Locrian +6 is degree 5 (b5). */
    switch(mod12(source_interval)){
        case 0:return 0;         /* root / octave */
        case 1:case 2:return 1;  /* 2 / 9 */
        case 3:case 4:return 2;  /* 3 */
        case 5:case 6:return 3;  /* 4 / 11; parent scale may reclassify +6 as b5 */
        case 7:return 4;         /* 5 */
        case 8:case 9:return 5;  /* 6 / 13 */
        case 10:case 11:return 6;/* 7 */
    }
    return 0;
}
static int hb_source_degree_from_parent_scale(int source_interval,int source_root,
                                               uint16_t parent_scale){
    /* INPUT -> ROLE depends only on follower root + parent scale quality.

       If the source pitch is a member of the parent scale, its ordinal scale
       degree is authoritative:
         natural minor: +3 -> degree 3
         Lydian:        +6 -> degree 4 (#4)
         Locrian:       +6 -> degree 5 (b5)

       The detected harmony is deliberately absent from this function. */
    int source_pc=mod12(source_root+source_interval);
    if(parent_scale&(1u<<source_pc)){
        int seen=0;
        for(int semitones=0;semitones<12;semitones++){
            int pc=mod12(source_root+semitones);
            if(!(parent_scale&(1u<<pc)))continue;
            if(pc==source_pc)return seen;
            seen++;
        }
    }
    return hb_source_interval_to_default_degree(source_interval);
}
static int hb_input_degree(Inst *instance,int note,int root,uint16_t scale){
    if(note>=0&&note<128&&instance->movy_input_degree[note])return instance->movy_input_degree[note]-1;
    return hb_source_degree_from_parent_scale(mod12(note-root),root,scale);
}
static const char *hb_role_name_for_degree(int degree){
    static const char *roles[7]={"Root","2nd","3rd","4th","5th","6th","7th"};
    return (degree>=0&&degree<7)?roles[degree]:"--";
}
static int hb_inferred_progression_scale(int source_root);
static int hb_follower_input_scale_index(Inst *instance,int source_root){
    int selected=hb_shared_follower_scale();
    if(selected>0)return selected;
    return hb_inferred_progression_scale(source_root);
}
static uint16_t hb_follower_input_scale(Inst *instance,int source_root){
    return hb_explicit_scale_mask(source_root,hb_follower_input_scale_index(instance,source_root));
}
static uint16_t hb_transpose_mask(uint16_t mask,int semitones){
    uint16_t shifted=0;
    for(int pitch=0;pitch<12;pitch++)if(mask&(1u<<pitch))shifted|=(uint16_t)(1u<<mod12(pitch+semitones));
    return shifted;
}
static const char *hb_follower_degree_role_for_note(Inst *instance,int note){
    if(!instance||note<0||note>127)return "--";
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))return "--";
    uint16_t parent_scale=hb_follower_input_scale(instance,source_root);
    int degree=hb_input_degree(instance,note,source_root,parent_scale);
    return hb_role_name_for_degree(degree);
}
static int hb_popcount12(uint16_t mask){
    int count=0;
    for(int bit=0;bit<12;bit++)if(mask&(1u<<bit))count++;
    return count;
}
/* One baseline for input roles, output mapping and display. The learned
   progression is independent of the current/lookahead rendering position.
   Recompute only when knowledge, reference root or transpose changes. */
static int hb_inferred_progression_scale(int source_root){
    int locked=g_bus.next_model_locked&&g_bus.next_model_count>0&&!g_movy_blocked;
    int count=locked?g_bus.next_model_count:(!g_movy_blocked?g_bus.next_learning_count:0);
    const hb_loop_harmony_event_t *events=locked?g_bus.next_model:g_bus.next_learning;
    unsigned observed=count?0:hb_harmony_chord_mask(g_bus.observed_harmony);
    if(g_infer_cached_root==source_root&&g_infer_cached_transpose==g_bus.global_transpose&&
       g_infer_cached_revision==g_infer_revision&&g_infer_cached_count==count&&g_infer_cached_observed==observed)
        return g_infer_scale;
    int previous=g_infer_cached_root==source_root?g_infer_scale:1;
    int scores[15]={0},evidence=0;
    for(int event=0;event<(count?count:1);event++){
        hb_harmony_t harmony=count?events[event].harmony:g_bus.observed_harmony;
        if(!harmony.valid)continue;
        harmony=hb_transpose_harmony(harmony,-g_bus.global_transpose);
        unsigned chord=hb_harmony_chord_mask(harmony);
        if(!chord)continue;
        evidence++;
        for(int scale=1;scale<=15;scale++){
            unsigned candidate=hb_explicit_scale_mask(source_root,scale);
            /* Every registered harmony contributes equally. A borrowed chord
               is evidence, not permission to replace the entire baseline. */
            scores[scale-1]+=hb_popcount12((uint16_t)(chord&candidate));
        }
    }
    int best=0,ties=0;
    for(int scale=1;scale<15;scale++)if(scores[scale]>scores[best])best=scale;
    for(int scale=0;scale<15;scale++)if(scores[scale]==scores[best])ties++;
    if(!evidence)best=0;
    else if(previous>=1&&previous<=15&&scores[previous-1]==scores[best])best=previous-1;
    g_infer_scale=best+1;g_infer_ambiguous=ties>1;g_infer_has_evidence=evidence>0;
    g_infer_cached_root=source_root;g_infer_cached_transpose=g_bus.global_transpose;
    g_infer_cached_revision=g_infer_revision;g_infer_cached_count=count;g_infer_cached_observed=observed;
    return g_infer_scale;
}
static int hb_parent_scale_at_transpose(Inst *instance,hb_harmony_t harmony,int transpose){
    (void)transpose;
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))source_root=mod12(harmony.root_pc-g_bus.global_transpose);
    return hb_follower_input_scale_index(instance,source_root);
}
static int hb_parent_scale_index(Inst *instance,hb_harmony_t harmony){
    return hb_parent_scale_at_transpose(instance,harmony,g_bus.global_transpose);
}
/* Source-root/scale still define INPUT degrees. These substitutions only
   choose the OUTPUT collection, using the effective (possibly shifted) chord. */
/* Family belongs to a resolution target, not to the root of its V chord.
   kind: 0 = dominant preparation, 1 = V, 2 = leading tone. All share one collection. */
static int hb_target_minor(unsigned parent,int target){
    return (parent&(1u<<mod12(target+3)))&&!(parent&(1u<<mod12(target+4)));
}
static uint16_t hb_accommodate_chord(uint16_t parent,hb_harmony_t harmony,int tonic);
static unsigned hb_function_family(Inst *instance,int target,unsigned parent,int minor,int kind){
    (void)kind;
    int choice=hb_policy_value(instance,minor?HB_P_DOMINANT_MINOR:HB_P_DOMINANT);
    if(!choice)return parent;
    unsigned collection;
    if(choice==7){
        hb_harmony_t dominant={0};dominant.valid=1;dominant.root_pc=mod12(target+7);
        dominant.chord_index=HB_HARMONY_EXPLICIT_TONES;
        dominant.pitch_mask=(1u<<mod12(target+7))|(1u<<mod12(target+11))|(1u<<mod12(target+2))|(1u<<mod12(target+5));
        return hb_accommodate_chord(parent,dominant,mod12(target));
    }
    if(choice==6)collection=hb_simplified_target_scale(instance,target,parent);
    else if(choice==3)collection=hb_explicit_scale_mask(mod12(target+8),9);
    else{
        static const int scales[]={0,8,9,0,1,19};
        collection=hb_explicit_scale_mask(mod12(target),scales[choice]);
    }
    /* Every enabled family retains V's root, major third and minor seventh.
       Altered V may omit its natural fifth; the actual voiced chord wins later. */
    unsigned major_third=1u<<mod12(target+11);
    if(!(collection&major_third))collection&=~(1u<<mod12(target+10));
    collection|=major_third|(1u<<mod12(target+7))|(1u<<mod12(target+5));
    if(choice!=3)collection|=1u<<mod12(target+2);
    return collection;
}
static unsigned hb_effective_parent(Inst *instance,int tonic){
    hb_key_context context=hb_key_for(instance);
    if(context.active)return context.target_mask;
    return hb_explicit_scale_mask(tonic,hb_parent_scale_index(instance,(hb_harmony_t){.valid=1,.root_pc=tonic}));
}
static uint16_t hb_dominant_scale_mask(Inst *instance,hb_harmony_t harmony,int tonic){
    if(!instance||!harmony.valid||harmony.intent_kind==7)return 0;
    unsigned chord=hb_harmony_chord_mask(harmony);
    int preparation=harmony.intent_kind>=8&&harmony.intent_kind<=10;
    int explicit_context=(((harmony.intent_kind>0&&harmony.intent_kind<4)||(harmony.intent_kind>=8&&harmony.intent_kind<=10)))||preparation;
    int target=explicit_context?harmony.intent_target:tonic;
    unsigned parent=explicit_context&&harmony.intent_scale?harmony.intent_scale:hb_effective_parent(instance,tonic);
    int minor=explicit_context?harmony.intent_minor:hb_target_minor(parent,target);
    int root=mod12(harmony.root_pc),relative=mod12(root-target);
    int dominant=harmony.intent_kind==2||(relative==7&&(chord&(1u<<mod12(root+4)))&&!(chord&(1u<<mod12(root+11))));
    int leading=harmony.intent_kind==3||(relative==11&&(chord&(1u<<mod12(root+3)))&&(chord&(1u<<mod12(root+6))));
    int preceding=explicit_context&&harmony.intent_kind==1&&relative==2;
    if(!dominant&&!leading&&!preceding&&!preparation)return 0;
    if(!hb_policy_value(instance,minor?HB_P_DOMINANT_MINOR:HB_P_DOMINANT))return 0;
    return hb_function_family(instance,target,parent,minor,dominant?1:leading?2:0);
}
/* Alter the selected collection only where chord tones require it. Preserve
   all existing chord tones; never substitute a new inferred parent wholesale. */
static uint16_t hb_accommodate_chord(uint16_t parent,hb_harmony_t harmony,int tonic){
    uint16_t chord=hb_harmony_chord_mask(harmony),result=parent;
    if(!parent)return chord;
    int root_degree=hb_source_degree_from_parent_scale(mod12(harmony.root_pc-tonic),tonic,parent);
    static const int interval_degree[12]={0,1,1,2,2,3,4,4,5,5,6,6};
    for(int interval=0;interval<12;interval++){
        int pitch=mod12(harmony.root_pc+interval);
        unsigned bit=1u<<pitch;
        if(!(chord&bit)||(parent&bit))continue;
        int role=interval_degree[interval];
        if(interval==8&&!(chord&(1u<<mod12(harmony.root_pc+7))))role=4; /* augmented fifth */
        if(interval==6&&(chord&(1u<<mod12(harmony.root_pc+7))))role=3; /* sharp eleventh */
        int degree=(root_degree+role)%7,seen=0,replaced=-1;
        /* Prefer the matching diatonic degree (e.g. D7 raises F, Fm lowers A).
           Altered fifths are fifths, not an inferred sharp fourth. */
        for(int offset=0;offset<12;offset++)if(parent&(1u<<mod12(tonic+offset))){
            if(seen++==degree){replaced=mod12(tonic+offset);break;}
        }
        if(replaced>=0&&!(chord&(1u<<replaced)))result&=(uint16_t)~(1u<<replaced);
        result|=bit;
    }
    return (uint16_t)(result|chord);
}
static uint16_t hb_borrowed_scale_mask(Inst *instance,hb_harmony_t harmony,int tonic,uint16_t parent){
    if(!hb_policy_value(instance,HB_P_BORROWED)||!harmony.valid||!(parent&(1u<<mod12(tonic+4))))return 0;
    unsigned chord=hb_harmony_chord_mask(harmony);
    int root=harmony.root_pc,relative=mod12(root-tonic);
    int major=(chord&(1u<<mod12(root+4)))&&(chord&(1u<<mod12(root+7)));
    int minor=(chord&(1u<<mod12(root+3)))&&(chord&(1u<<mod12(root+7)))&&!(chord&(1u<<mod12(root+4)));
    if(!((major&&(relative==3||relative==8||relative==10))||(minor&&relative==5)))return 0;
    static const int selections[]={0,2,3,13};
    return hb_explicit_scale_mask(tonic,selections[hb_policy_value(instance,HB_P_BORROWED)]);
}
#include "../../../src/scale_context.h"
static uint16_t hb_parent_chord_scale(Inst *instance,hb_harmony_t harmony,int source_root,uint16_t parent,int transpose){
    int tonic=mod12(source_root+transpose);
    parent=hb_transpose_mask(parent,transpose);
    hb_key_context context=hb_key_for(instance);
    if(instance->role==1&&context.active){tonic=mod12(context.target_root+transpose-g_bus.global_transpose);parent=hb_transpose_mask(context.target_mask,transpose-g_bus.global_transpose);}
    uint16_t shifted=hb_dominant_scale_mask(instance,harmony,tonic);
    if(shifted)return shifted; /* Final follower target includes actual chord tones without deleting altered tensions. */
    uint16_t borrowed=hb_borrowed_scale_mask(instance,harmony,tonic,parent);
    return hb_accommodate_chord(borrowed?borrowed:parent,harmony,tonic);
}
static uint16_t hb_output_chord_scale_at_transpose(Inst *instance,hb_harmony_t harmony,int source_root,uint16_t parent,int transpose){
    return hb_policy_value(instance,HB_P_GAP)?hb_local_output_scale(instance,harmony):hb_parent_chord_scale(instance,harmony,source_root,parent,transpose);
}
static uint16_t hb_output_chord_scale(Inst *instance,hb_harmony_t harmony,int source_root,uint16_t parent){
    return hb_output_chord_scale_at_transpose(instance,harmony,source_root,parent,g_bus.global_transpose);
}
static int hb_nth_scale_interval_from_root(uint16_t scale_mask,int target_root,int degree){
    /* degree is 0..6 meaning root, 2nd, 3rd, 4th, 5th, 6th, 7th.
       We rotate the parent scale onto the detected harmony root, yielding
       the chord-scale/mode for that diatonic chord root. */
    if(degree<=0)return 0;
    unsigned relative=0;for(int interval=0;interval<12;interval++)if(scale_mask&(1u<<mod12(target_root+interval)))relative|=1u<<interval;
    if(relative==0x55Bu){static const int altered[7]={0,1,4,6,6,8,10};return altered[degree%7];}
    int seen=0;
    for(int semitones=1;semitones<=24;semitones++){
        int pc=mod12(target_root+semitones);
        if(scale_mask&(1u<<pc)){
            seen++;
            if(seen==degree)return semitones;
        }
    }
    return degree*2;
}
static int hb_render_degree_interval(uint16_t scale,hb_harmony_t harmony,int degree){
    int interval=hb_nth_scale_interval_from_root(scale,harmony.root_pc,degree);
    unsigned relative=0;for(int semitone=0;semitone<12;semitone++)if(scale&(1u<<mod12(harmony.root_pc+semitone)))relative|=1u<<semitone;
    if(relative==0x55Bu&&(degree==2||degree==4||degree==6)){
        unsigned chord=hb_harmony_chord_mask(harmony);
        static const int choices[3][3]={{4,3,-1},{7,6,8},{10,11,-1}};
        int role=degree/2-1;
        for(int index=0;index<3;index++){int candidate=choices[role][index];if(candidate>=0&&(chord&(1u<<mod12(harmony.root_pc+candidate))))return candidate;}
    }
    return interval;
}
static int hb_map_scale_degree_relative(Inst *instance,int source_note,hb_harmony_t harmony){
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))return source_note;

    int parent_scale_index=hb_parent_scale_index(instance,harmony);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);

    /* Interpret the source note as a DEGREE of the parent scale first.
       The UI may display the literal chromatic role (m3, b5, etc.), but
       rendering carries the corresponding ordinal degree into the target
       chord-scale. Thus m3/M3 both remain degree 3 when appropriate, while
       a tritone can correctly be #4 or b5 depending on the parent scale. */
    int degree=hb_input_degree(instance,source_note,source_root,hb_follower_input_scale(instance,source_root));

    /* If the detected harmony root is in the parent scale, rotate that parent
       scale onto the harmony root. Example:
         F major + C root => C Mixolydian
         F natural minor + C root => C Phrygian
       Therefore follower G over root F (degree 2/9):
         over C in F major -> D
         over C in F minor -> Db. */
    uint16_t chord_scale=hb_output_chord_scale(instance,harmony,source_root,parent_scale);

    int target_interval=hb_render_degree_interval(chord_scale,harmony,degree);
    int target_pc=mod12(harmony.root_pc+target_interval);

    /* Preserve the degree; choose its nearest octave to the source register. */
    int nominal=source_note+g_bus.global_transpose;
    int adjustment=mod12(target_pc-mod12(nominal));
    if(adjustment>6)adjustment-=12;
    int mapped=nominal+adjustment;
    while(mapped<0)mapped+=12;
    while(mapped>127)mapped-=12;
    return mapped;
}
static hb_harmony_t hb_follower_content_target(Inst *instance,hb_harmony_t harmony,int content_map){
    /* Intermediate content modes are "Chord + guaranteed scale degrees".
       Start with the normal inferred chord-tone set, then fill requested roles
       from the chord-scale implied by the follower parent scale.

       Internal content codes preserve legacy persistence:
         0 Chord
         1 Scale
         2 Free
         3 135
         4 1357
         5 12357
         6 12356
         7 Non-Avoid
         8 123567
    */
    hb_harmony_t target=hb_mapping_target(harmony,0);
    if(content_map<3||content_map>8)return target;

    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))source_root=harmony.root_pc;
    int parent_scale_index=hb_parent_scale_index(instance,harmony);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);
    uint16_t chord_scale=hb_output_chord_scale(instance,harmony,source_root,parent_scale);

    unsigned degree_bits=0;
    if(content_map==3)      degree_bits=(1u<<0)|(1u<<2)|(1u<<4);                 /* 1 3 5 */
    else if(content_map==4) degree_bits=(1u<<0)|(1u<<2)|(1u<<4)|(1u<<6);         /* 1 3 5 7 */
    else if(content_map==5) degree_bits=(1u<<0)|(1u<<1)|(1u<<2)|(1u<<4)|(1u<<6);/* 1 2 3 5 7 */
    else if(content_map==6) degree_bits=(1u<<0)|(1u<<1)|(1u<<2)|(1u<<4)|(1u<<5);/* 1 2 3 5 6 */
    else if(content_map==7) degree_bits=0x7Fu;                                   /* all scale degrees, filter avoids */
    else if(content_map==8) degree_bits=(1u<<0)|(1u<<1)|(1u<<2)|(1u<<4)|(1u<<5)|(1u<<6);/* 1 2 3 5 6 7 */

    uint16_t chord_mask=hb_harmony_chord_mask(harmony);
    for(int degree=0;degree<7;degree++){
        if(!(degree_bits&(1u<<degree)))continue;
        int interval=hb_render_degree_interval(chord_scale,harmony,degree);
        int pc=mod12(harmony.root_pc+interval);

        if(content_map==7){
            /* "Non-Avoid" is evaluated in the DETECTED HARMONY'S chord-scale,
               not in the follower reference scale before rotation.

               General jazz avoid-note rule: chord tones are always available;
               a non-chord scale tone is avoided when it lies a semitone ABOVE
               a chord tone. This yields exactly:
                 Ionian/Mixolydian -> avoid 4 (above 3)
                 Aeolian           -> avoid b6 (above 5)
                 Phrygian          -> avoid b2 and b6 (above root and 5)
               and generalizes to diminished/altered chord-scales without a
               brittle mode-name switch. */
            int is_chord_tone=(chord_mask&(1u<<pc))!=0;
            int semitone_below=mod12(pc-1);
            int is_avoid=!is_chord_tone && ((chord_mask&(1u<<semitone_below))!=0);
            if(is_avoid)continue;
        }
        target.pitch_mask|=(uint16_t)(1u<<pc);
    }
    return target;
}
static int hb_map_note_upward(int source_note,hb_harmony_t harmony){
    if(!harmony.valid||!harmony.pitch_mask)return source_note;
    for(int candidate=source_note;candidate<=127;candidate++){
        if(harmony.pitch_mask&(1u<<mod12(candidate)))return candidate;
    }
    for(int candidate=source_note-1;candidate>=0;candidate--){
        if(harmony.pitch_mask&(1u<<mod12(candidate)))return candidate;
    }
    return source_note;
}
static int hb_map_note_downward(int source_note,hb_harmony_t harmony){
    if(!harmony.valid||!harmony.pitch_mask)return source_note;
    for(int candidate=source_note;candidate>=0;candidate--){
        if(harmony.pitch_mask&(1u<<mod12(candidate)))return candidate;
    }
    for(int candidate=source_note+1;candidate<=127;candidate++){
        if(harmony.pitch_mask&(1u<<mod12(candidate)))return candidate;
    }
    return source_note;
}
static int hb_signed_root_delta(int source_root,int target_root){
    int delta=mod12(target_root-source_root);
    if(delta>6)delta-=12;
    return delta;
}
static int hb_note_near_pc(int nominal,int pitch_class){
    int adjustment=mod12(pitch_class-mod12(nominal));
    if(adjustment>6)adjustment-=12;
    int mapped=nominal+adjustment;
    while(mapped<0)mapped+=12;
    while(mapped>127)mapped-=12;
    return mapped;
}
/* Scope is captured at source onset, including delayed and held owners. */
static int hb_play_applies(Inst *instance){
    return !instance->play.bypass&&(instance->play.scope==0||
        (instance->play.scope==1?instance->movy_playback:!instance->movy_playback));
}
static int hb_play_note(Inst *instance,int note,hb_harmony_t harmony,unsigned mask){
    if(instance->role!=1||!harmony.valid||!hb_play_applies(instance))return note;
    return hb_fp_note(instance->play,note,harmony.root_pc,mask);
}
static int hb_map_follower_note_relative(Inst *instance,int source_note,hb_harmony_t detected,
                                         hb_harmony_t content_target,int content_map){
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))return source_note;

    int nominal=source_note+hb_signed_root_delta(source_root,mod12(detected.root_pc));
    while(nominal<0)nominal+=12;
    while(nominal>127)nominal-=12;

    if(content_map==2)return nominal; /* Free + Relative is chromatic root-relative transport. */

    int parent_scale_index=hb_parent_scale_at_transpose(instance,detected,0);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);
    int source_degree=hb_input_degree(instance,source_note,source_root,hb_follower_input_scale(instance,source_root));
    uint16_t chord_scale=hb_output_chord_scale_at_transpose(instance,detected,source_root,parent_scale,0);

    int target_interval=hb_render_degree_interval(chord_scale,detected,source_degree);
    int target_pc=mod12(detected.root_pc+target_interval);

    int source_interval=hb_nth_scale_interval_from_root(
        hb_follower_input_scale(instance,source_root),source_root,source_degree);
    int source_tonic=hb_note_near_pc(source_note-source_interval,source_root);
    int relative=source_tonic+hb_signed_root_delta(source_root,mod12(detected.root_pc))+target_interval;
    while(relative<0)relative+=12;
    while(relative>127)relative-=12;
    if(content_target.pitch_mask&(1u<<target_pc))return relative;
    return hb_note_near_pc(relative,mod12(hb_map_note(relative,detected.root_pc,content_target,HB_MAP_NEAREST)));
}
static uint16_t hb_scale_degree_mask(uint16_t chord_scale,hb_harmony_t harmony,unsigned degree_bits){
    int harmony_root=harmony.root_pc;
    uint16_t mask=0;
    for(int degree=0;degree<7;degree++){
        if(!(degree_bits&(1u<<degree)))continue;
        int interval=hb_render_degree_interval(chord_scale,harmony,degree);
        mask|=(uint16_t)(1u<<mod12(harmony_root+interval));
    }
    return mask;
}
static int hb_map_follower_note_classified(Inst *instance,int source_note,hb_harmony_t detected,
                                               hb_harmony_t content_target,int split){
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))
        return hb_map_note(source_note,reference_root(instance),content_target,HB_MAP_NEAREST);

    int parent_scale_index=hb_parent_scale_at_transpose(instance,detected,0);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);
    int source_degree=hb_input_degree(instance,source_note,source_root,hb_follower_input_scale(instance,source_root));
    if(source_degree<0)source_degree=0;
    if(source_degree>6)source_degree=6;

    uint16_t chord_scale=hb_output_chord_scale_at_transpose(instance,detected,source_root,parent_scale,0);

    uint16_t legal=(uint16_t)(content_target.pitch_mask&0x0FFFu);
    if(!legal)return source_note;

    uint16_t inferred_chord_mask=hb_harmony_chord_mask(detected);
    uint8_t active_notes[64];
    int active_count=0;
    uint16_t active_mask=0;
    if(split==3){
        active_count=hb_observed_notes(0,active_notes,64);
        for(int index=0;index<active_count;index++)
            active_mask|=(uint16_t)(1u<<mod12(active_notes[index]));
    }

    unsigned int allowed_by_degree[HB_CLOSEST_SPLIT_DEGREES];
    int nominal_by_degree[HB_CLOSEST_SPLIT_DEGREES];
    int output_by_degree[HB_CLOSEST_SPLIT_DEGREES];

    unsigned on_bits_135=(1u<<0)|(1u<<2)|(1u<<4);
    unsigned on_bits_1357=on_bits_135|(1u<<6);
    uint16_t on_mask_135=hb_scale_degree_mask(chord_scale,detected,on_bits_135);
    uint16_t off_mask_135=hb_scale_degree_mask(chord_scale,detected,0x7Fu&~on_bits_135);
    uint16_t on_mask_1357=hb_scale_degree_mask(chord_scale,detected,on_bits_1357);
    uint16_t off_mask_1357=hb_scale_degree_mask(chord_scale,detected,0x7Fu&~on_bits_1357);

    uint16_t input_scale=hb_follower_input_scale(instance,source_root);
    int source_degree_interval=hb_nth_scale_interval_from_root(input_scale,source_root,source_degree);
    int source_root_note=source_note-source_degree_interval;
    source_root_note=hb_note_near_pc(source_root_note,source_root);

    for(int degree=0;degree<HB_CLOSEST_SPLIT_DEGREES;degree++){
        int source_interval=hb_nth_scale_interval_from_root(input_scale,source_root,degree);
        int target_interval=hb_render_degree_interval(chord_scale,detected,degree);
        int degree_pc=mod12(detected.root_pc+target_interval);
        /* Closest Split stays near the source degree's ACTUAL register position.
           Using target_root+target_degree here reconstructs Relative mapping. */
        nominal_by_degree[degree]=source_root_note+source_interval;

        uint16_t preferred=0;
        if(split==1){
            int is_on=(on_bits_135&(1u<<degree))!=0;
            uint16_t pool=is_on?on_mask_135:off_mask_135;
            preferred=(uint16_t)(legal&pool);
            if(!preferred)preferred=pool; /* explicit split must not cross groups */
        }else if(split==2){
            int is_on=(on_bits_1357&(1u<<degree))!=0;
            uint16_t pool=is_on?on_mask_1357:off_mask_1357;
            preferred=(uint16_t)(legal&pool);
            if(!preferred)preferred=pool;
        }else if(split==3){
            int is_on=(active_mask&(1u<<degree_pc))!=0;
            uint16_t out_mask=(uint16_t)(chord_scale&(uint16_t)(~active_mask)&0x0FFFu);
            preferred=(uint16_t)(legal&(is_on?active_mask:out_mask));
        }else{
            /* Classify the SOURCE tonic triad (degrees 1/3/5). The rendered
               chord selects destination pitches, never input membership. */
            int is_on=(on_bits_135&(1u<<degree))!=0;
            uint16_t out_mask=(uint16_t)(chord_scale&(uint16_t)(~inferred_chord_mask)&0x0FFFu);
            uint16_t pool=is_on?inferred_chord_mask:out_mask;
            preferred=(uint16_t)(legal&pool);
            if(!preferred)preferred=pool; /* Content filters must not cross groups. */
        }
        allowed_by_degree[degree]=(unsigned int)(preferred?preferred:legal);
    }

    /* Joint assignment balances proximity, register and pitch-class collisions
       without imposing Relative's degree ordering. */
    if(!hb_cached_closest_assignment(&instance->closest_assignments,HB_CLOSEST_SPLIT_DEGREES,nominal_by_degree,allowed_by_degree,output_by_degree)){
        /* At a MIDI boundary a full octave's bounded assignment can fail.
           Even that fallback must preserve this input's mapping class. */
        int nearest=hb_cs_nearest(source_note,allowed_by_degree[source_degree]);
        return hb_play_note(instance,nearest<0?source_note:nearest,detected,allowed_by_degree[source_degree]);
    }
    return hb_play_note(instance,output_by_degree[source_degree],detected,allowed_by_degree[source_degree]);
}
static int hb_map_follower_note_closest_split(Inst *instance,int note,hb_harmony_t detected,hb_harmony_t target){
    return hb_map_follower_note_classified(instance,note,detected,target,instance->follower_split_map);
}
static int hb_map_follower_base_note(Inst *instance,int source_note,hb_harmony_t detected,hb_harmony_t target){
    int content_map=instance->content_map;
    if(!detected.valid)return hb_cp_clamp(source_note,0,127);


    int travel=instance->travel_map;
    if(travel==7)return hb_play_note(instance,hb_key_travel_pitch(instance,source_note+g_bus.global_transpose,hb_transpose_harmony(detected,g_bus.global_transpose),hb_key_follower_travel(instance),1)-g_bus.global_transpose,detected,target.pitch_mask); /* None bypasses chord-root travel, not active dominant collections. */
    if(travel==5){
        int direct=hb_key_travel_pitch(instance,source_note+g_bus.global_transpose,hb_transpose_harmony(detected,g_bus.global_transpose),hb_key_follower_travel(instance),1)-g_bus.global_transpose;
        if(direct<0)direct=0;
        if(direct>127)direct=127;
        return hb_play_note(instance,direct,detected,target.pitch_mask);
    }
    if(travel==0||travel==8){
        int mapped=hb_map_follower_note_relative(instance,source_note,detected,target,content_map);
        if(travel==8)mapped=hb_travel_register(source_note,mapped,4);
        return hb_play_note(instance,mapped,detected,target.pitch_mask);
    }
    if(travel==9)return hb_map_follower_note_classified(instance,source_note,detected,target,0);
    if(travel==3||travel==6)
        return hb_map_follower_note_closest_split(instance,source_note,detected,target);
    if(travel==1){
        int root=reference_root(instance);hb_resolve_follower_reference_root(instance,&root);
        int closest=hb_closest_diverse(instance,hb_cp_clamp(source_note,0,127),root,hb_follower_input_scale(instance,root),target.pitch_mask);
        return hb_play_note(instance,closest<0?source_note:closest,detected,target.pitch_mask);
    }
    if(travel==2||travel==4){
        int mapped=hb_map_follower_note_relative(instance,source_note,detected,target,content_map);
        return hb_play_note(instance,hb_travel_register(source_note,mapped,travel==2?5:6),detected,target.pitch_mask);
    }
    return source_note;
}
/* Chromatic pads approach the rendering of the next higher diatonic input.
   Resolve that input through the selected travel mapping and register context. */
static int hb_chromatic_approach_target(Inst *instance,int source_note,hb_harmony_t detected){
    if(!detected.valid)return -1;
    if(source_note>=0&&source_note<128&&instance->movy_input_target[source_note])
        return instance->movy_input_target[source_note]-1;
    int source_root=0;
    if(hb_resolve_follower_reference_root(instance,&source_root)){
        uint16_t parent=hb_follower_input_scale(instance,source_root);
        if(parent&&!(parent&(1u<<mod12(source_note))))
            for(int next=source_note+1;next<=127;next++)
                if(parent&(1u<<mod12(next)))return next;
    }
    return -1;
}
static int hb_map_follower_note_split2(Inst *instance,int source_note,hb_harmony_t detected,
                                        hb_harmony_t content_target){
    if(!detected.valid)return hb_cp_clamp(source_note,0,127);
    int next=hb_chromatic_approach_target(instance,source_note,detected);
    if(next>=0){
        int saved=instance->movy_input_degree[next];
        if(instance->movy_input_target[source_note])instance->movy_input_degree[next]=0;
        int target=hb_map_follower_base_note(instance,next,detected,content_target);
        instance->movy_input_degree[next]=(uint8_t)saved;
        return target>0?target-1:0;
    }
    return hb_map_follower_base_note(instance,source_note,detected,content_target);
}
static int hb_chromatic_travel(const Inst *instance){return instance->chromatic_map||instance->travel_map==6;}
static int hb_approach_pad_enabled(const Inst *instance){return instance->role==1&&(instance->approach_layout||hb_chromatic_travel(instance));}
static int hb_map_follower_reference_note(Inst *instance,int source_note,hb_harmony_t detected,hb_harmony_t target){
    return hb_chromatic_travel(instance)?hb_map_follower_note_split2(instance,source_note,detected,target):hb_map_follower_base_note(instance,source_note,detected,target);
}
/* Determine the output role in reference-key space. Master transpose is
   applied exactly once, after travel and role transforms have finished.
   At MIDI limits fold by octaves, preserving the selected pitch class/role. */
static int hb_map_follower_note_unoperated(Inst *instance,int source_note){
    hb_harmony_t detected=hb_key_harmony(instance,hb_render_harmony(instance));
    hb_harmony_t target=detected;
    if(instance->content_map==2)target.pitch_mask=0x0FFFu;
    else if(instance->content_map==1)target=hb_follower_scale_target(instance,detected);
    else target=hb_follower_content_target(instance,detected,instance->content_map);
    int transpose=g_bus.global_transpose;
    detected=hb_transpose_harmony(detected,-transpose);
    target=hb_transpose_harmony(target,-transpose);
    /* A piano gap owns a separate input identity, but resolves the exact
       lower pad before applying its chromatic approach. */
    int shift=source_note>=0&&source_note<128?instance->movy_pad_shift[source_note]:0;
    int resolved=source_note+shift;
    int rendered;
    if(shift&&resolved>=0&&resolved<128){
        uint8_t degree=instance->movy_input_degree[resolved], next=instance->movy_input_target[resolved];
        instance->movy_input_degree[resolved]=instance->movy_input_degree[source_note];
        int source_next=instance->movy_input_target[source_note];
        instance->movy_input_target[resolved]=source_next&&source_next+shift>0&&source_next+shift<=128?source_next+shift:0;
        rendered=hb_map_follower_reference_note(instance,resolved,detected,target)+transpose-(instance->approach_rows.tokens[source_note]?0:1);
        instance->movy_input_degree[resolved]=degree;instance->movy_input_target[resolved]=next;
    }else rendered=hb_map_follower_reference_note(instance,source_note,detected,target)+transpose;
    while(rendered<0)rendered+=12;
    while(rendered>127)rendered-=12;
    return rendered;
}
static unsigned hb_approach_scale(Inst *instance,hb_harmony_t harmony){
    if(harmony.valid)return hb_follower_scale_target(instance,harmony).pitch_mask;
    int root=0;hb_resolve_follower_reference_root(instance,&root);
    return hb_transpose_mask(hb_follower_input_scale(instance,root),g_bus.global_transpose);
}
static int hb_target_diminished(unsigned scale,int target){
    return (scale&(1u<<mod12(target+3)))&&(scale&(1u<<mod12(target+6)))&&!(scale&(1u<<mod12(target+7)));
}
static int hb_current_target_diminished(Inst *instance,unsigned scale,int target){
    if(hb_key_for(instance).approach_scale)return hb_target_diminished(scale,target);
    hb_key_context context=hb_key_for(instance);
    if(instance->role==0&&context.active&&!context.blues){
        target=hb_key_map(context,target);scale=context.target_mask;
    }
    return hb_target_diminished(scale,target);
}
static int hb_relative_approach_offset(int role,int target,unsigned scale){
    if(role==2&&hb_target_diminished(scale,target))return -1;
    if(role==15)role=1;else if(role==16)role=9;else if(role==17)role=4;
    return role==14?hb_nth_scale_interval_from_root((uint16_t)scale,mod12(target),4)-12:role==12?-1:role==13?1:role==11?2:role==1?hb_nth_scale_interval_from_root((uint16_t)scale,mod12(target),1):role==8?hb_nth_scale_interval_from_root((uint16_t)scale,mod12(target),2):role==9?hb_nth_scale_interval_from_root((uint16_t)scale,mod12(target),3):role==10?hb_nth_scale_interval_from_root((uint16_t)scale,mod12(target),6)-12:role==2?-5:role==4?
        hb_nth_scale_interval_from_root((uint16_t)scale,mod12(target),5)-12:
        role==5?5:role==6?-2:role==7?-4:0;
}
static int hb_context_approach_offset(Inst *instance,int role,int target,unsigned scale){
    if(role==2)return hb_current_target_diminished(instance,scale,target)?-1:-5;
    return hb_relative_approach_offset(role,target,scale);
}
/* A chromatic pad is itself a destination. Borrow the scale of that local
   destination for its relative cadence, without changing its landing quality.
   In-scale destinations retain the complete effective parent collection. */
static const char *HB_TARGET_KEYS[]={"target_scale_source","target_scale_major","target_scale_minor","target_scale_diminished"};
static const char *HB_TARGET_SOURCE[]={"Auto","Parent","Simplified"};
static const char *HB_TARGET_MAJOR[]={"Major","Lydian","Harmonic Major"};
static const char *HB_TARGET_MINOR[]={"Natural Minor","Dorian","Harmonic Minor","Melodic Minor"};
static const char *HB_TARGET_DIMINISHED[]={"Locrian","Locrian #2"};
static const char *const *HB_TARGET_OPTIONS[]={HB_TARGET_SOURCE,HB_TARGET_MAJOR,HB_TARGET_MINOR,HB_TARGET_DIMINISHED};
static const int HB_TARGET_COUNTS[]={3,3,4,2};
/* A target's third and fifth determine its quality before any dominant-family
   treatment. The selected family is rooted at the destination, not the approach. */
static unsigned hb_simplified_target_scale(Inst *instance,int target,unsigned parent){
    static const int major[]={1,5,19},minor[]={2,3,8,9},diminished[]={7,14};
    int is_minor=hb_target_minor(parent,target);
    int is_diminished=is_minor&&(parent&(1u<<mod12(target+6)))&&!(parent&(1u<<mod12(target+7)));
    int scale=is_diminished?diminished[instance->target_scale_policy[3]]:
        is_minor?minor[instance->target_scale_policy[2]]:major[instance->target_scale_policy[1]];
    return hb_explicit_scale_mask(mod12(target),scale);
}
static unsigned hb_relative_target_scale(Inst *instance,int target,unsigned parent,int dominant){
    int source=instance->target_scale_policy[0];
    int family=hb_policy_value(instance,hb_target_minor(parent,target)?HB_P_DOMINANT_MINOR:HB_P_DOMINANT);
    if(source==2||(source==0&&((dominant&&family&&family!=7)||(instance->motion.render_flags&HB_MO_SIMPLE))))
        return hb_simplified_target_scale(instance,target,parent);
    if(parent&(1u<<mod12(target)))return parent;
    int tonic=0;hb_resolve_follower_reference_root(instance,&tonic);tonic=mod12(tonic+g_bus.global_transpose);
    int relative=mod12(target-tonic);
    if((parent&(1u<<mod12(tonic+4)))&&(relative==3||relative==8||relative==10)){
        /* Missing chromatic destinations keep the established borrowed family. */
        static const int borrowed[]={2,2,3,13};
        return hb_explicit_scale_mask(tonic,borrowed[hb_policy_value(instance,HB_P_BORROWED)]);
    }
    return hb_explicit_scale_mask(mod12(target),1);
}
/* Legacy Simple Chord edits only the chord core; explicit new source choices
   operate on the whole target collection and take precedence over that mode. */
static int hb_target_simple_chord(Inst *instance){
    return instance->target_scale_policy[0]==0&&(instance->motion.render_flags&HB_MO_SIMPLE)&&
        !(instance->motion.render_flags&HB_MO_SIMPLE_SCALE);
}
static unsigned hb_relative_dominant_scale(Inst *instance,int root,int destination,int leading,unsigned parent){
    (void)root;
    return hb_function_family(instance,destination,parent,hb_target_minor(parent,destination),leading?2:1);
}
static unsigned hb_secondary_collection(Inst *instance,int role,int target,unsigned parent){
    unsigned collection=hb_relative_target_scale(instance,target,hb_key_approach_scale(instance,target,parent),role==2||role==12||role>=15);
    return role>=15?hb_function_family(instance,target,collection,hb_target_minor(collection,target),0):collection;
}
/* Resolve a recorded cadence step against its current destination. */
typedef struct {int root,quality,destination;unsigned scale;int minor;} hb_cadence_result;
static hb_cadence_result hb_resolve_cadence(Inst *instance,const hb_cadence_step *step,int target,unsigned parent){
    parent=hb_key_approach_scale(instance,target,parent);
    unsigned collection=hb_relative_target_scale(instance,target,parent,0);
    for(int depth=0;depth<3&&step->path[depth];depth++){
        int term=step->path[depth];
        static const int major[]={0,0,2,4,5,7,9,11};
        target+=term<0?major[-term]:hb_nth_scale_interval_from_root(collection,mod12(target),term-1);
        collection=hb_relative_target_scale(instance,target,parent,0);
    }
    int diminished=hb_current_target_diminished(instance,collection,target);
    int dominant=step->kind==HB_CAD_DOMINANT||step->kind==HB_CAD_MINOR_DOMINANT||step->kind==HB_CAD_LEADING;
    collection=hb_relative_target_scale(instance,target,collection,dominant);
    hb_cadence_result result={target,0,target,collection,hb_target_minor(collection,target)};
    if(step->kind==HB_CAD_MINOR_DEGREE||step->kind==HB_CAD_MINOR_DOMINANT||step->kind==HB_CAD_MINOR_TARGET)result.minor=1;
    if(step->kind==HB_CAD_BORROWED||step->kind==HB_CAD_MINOR_DEGREE||step->kind==HB_CAD_MINOR_DOMINANT||step->kind==HB_CAD_MINOR_TARGET)
        result.scale=hb_explicit_scale_mask(mod12(target),2);
    if(step->kind==HB_CAD_DOMINANT||step->kind==HB_CAD_MINOR_DOMINANT){
        result.root=target-(diminished?1:5);result.quality=diminished?9:6;
        result.scale=hb_relative_dominant_scale(instance,result.root,target,diminished,result.scale);
    }else if(step->kind==HB_CAD_LEADING){
        result.root=target-1;result.quality=9;
        result.scale=hb_relative_dominant_scale(instance,result.root,target,1,result.scale);
    }else if(step->kind==HB_CAD_DEGREE||step->kind==HB_CAD_BORROWED||step->kind==HB_CAD_MINOR_DEGREE){
        if(step->degree==2&&(step->kind==HB_CAD_DEGREE||step->kind==HB_CAD_MINOR_DEGREE))
            result.scale=hb_function_family(instance,target,result.scale,hb_target_minor(result.scale,target),0);
        result.root+=hb_nth_scale_interval_from_root(result.scale,mod12(target),step->degree-1)-(step->degree>=6?12:0);
        if(step->kind==HB_CAD_MINOR_DEGREE)result.quality=8;
        else if(step->kind==HB_CAD_DEGREE&&hb_target_simple_chord(instance)){
            int third=hb_nth_scale_interval_from_root(result.scale,mod12(result.root),2);
            int fifth=hb_nth_scale_interval_from_root(result.scale,mod12(result.root),4);
            int seventh=hb_nth_scale_interval_from_root(result.scale,mod12(result.root),6);
            result.quality=third==3?(fifth==6?8:7):(seventh==11?5:6);
            int old_root=target+hb_nth_scale_interval_from_root(parent,mod12(target),step->degree-1);
            result.scale=(parent&~(1u<<mod12(old_root)))|(1u<<mod12(result.root));
        }
    }else if(step->kind==HB_CAD_MINOR_TARGET)result.quality=7;
    result.scale|=1u<<mod12(result.root);
    return result;
}
static int hb_map_follower_note_now(Inst *instance,int source_note){
    int rendered=hb_map_follower_note_unoperated(instance,source_note);
    if(!hb_cp_mode(&instance->player)){
        int secondary=hb_secondary_at(instance,source_note);
        hb_harmony_t harmony=hb_key_harmony(instance,hb_render_harmony(instance));
        int parent_root=0;hb_resolve_follower_reference_root(instance,&parent_root);
        unsigned parent=hb_parent_chord_scale(instance,harmony,parent_root,hb_follower_input_scale(instance,parent_root),g_bus.global_transpose);
        const hb_cadence_step *cadence=hb_mo_current_cadence(&instance->motion);
        if(cadence)rendered=hb_resolve_cadence(instance,cadence,rendered,parent).root;
        else rendered+=hb_context_approach_offset(instance,secondary,rendered,hb_secondary_collection(instance,secondary,rendered,parent));
        while(rendered<0)rendered+=12;while(rendered>127)rendered-=12;
    }
    return rendered;
}
static int hb_active_approach(const Inst *instance){
    if(!instance)return HB_APPROACH_OFF;
    return hb_approach_effective(instance->approach_control,instance->approach_pad_armed);
}
static int hb_apply_approach(Inst *instance,int mapped,int approach){
    if(approach==HB_APPROACH_CHROM_ABOVE)return mapped<127?mapped+1:127;
    if(approach==HB_APPROACH_CHROM_BELOW){
        return mapped>0?mapped-1:0;
    }
    if(approach==HB_APPROACH_SCALE_ABOVE){
        hb_harmony_t detected=hb_key_harmony(instance,hb_render_harmony(instance));
        if(!detected.valid)return mapped;
        int source_root=0;
        if(!hb_resolve_follower_reference_root(instance,&source_root))
            source_root=detected.root_pc;
        uint16_t scale_mask=hb_follower_scale_target(instance,detected).pitch_mask;
        return hb_next_scale_pitch_above(mapped,(unsigned int)scale_mask);
    }
    return mapped;
}
static void hb_consume_next_approach(Inst *instance){
    if(!instance)return;
    instance->approach_pad_armed=hb_approach_consume_next(instance->approach_pad_armed);
}
static int hb_conductor_pending_for_follow(void){
    /* Follower and conductor tick callbacks are independently scheduled.
       A one-tick follower barrier therefore does not guarantee that the
       conductor tick which commits the new harmony has already run.
       Hold queued follower events until every live conductor has consumed
       its current note-set change (dirty==0) and any confirmation candidate
       has either committed or been rejected (candidate_frames==0). */
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *conductor=&g_pool[index];
        if(!conductor->used||conductor->role!=0)continue;
        if(conductor->dirty||conductor->candidate_frames>0)return 1;
    }
    return 0;
}
static void hb_prepare_conductors(int frames,int sample_rate);
static double hb_motion_position(Inst *instance);
static double hb_motion_condition_position(void);
static hb_cp_config hb_chord_config_at(Inst *instance,int source,double beat,double condition){
    hb_role_sync(instance);
    hb_cp_config config=hb_cp_effective_config(&instance->player);
    for(int index=0;index<HB_MOTION_LANES;index++){
        int operation=hb_mo_operation(&instance->motion,index);double value;
        if(operation==HB_MO_CHORD_FORM&&hb_mo_value_at(&instance->motion,index,beat,condition,source,&value))
            config.size=hb_mo_clamp(hb_mo_round(value),0,HB_CP_FORMS-1);
    }
    return config;
}
static int hb_secondary_at(Inst *instance,int source_note){
    hb_motion_config *motion=&instance->motion;
    const unsigned long long *events=motion->event_override?motion->event_override:motion->events;
    motion->render_flags=events[HB_MOTION_LANES]&(HB_MO_SIMPLE|HB_MO_SIMPLE_SCALE|HB_MO_CONNECTOR_ABOVE|HB_MO_LEGACY_II);
    unsigned held=hb_mo_held_trigger(motion);
    if(held)motion->render_flags=hb_mo_source_flags(motion,held);
    if(hb_mo_held_modifier(motion))return 0;
    int selected=hb_mo_held_secondary(motion);
    if(!selected)selected=hb_mo_source_secondary(motion);
    if(!selected)for(int lane=0;lane<HB_MOTION_LANES;lane++){
        int operation=hb_mo_operation(motion,lane);double value=0;
        int role=hb_mo_role(operation);
        if(role&&hb_mo_lane_active(motion,lane)&&hb_mo_value_at(motion,lane,hb_motion_position(instance),hb_motion_condition_position(),source_note,&value)&&value>0){
            selected=role;
            motion->render_flags=(motion->render_flags&~(HB_MO_SIMPLE|HB_MO_SIMPLE_SCALE))|((hb_mo_has_scale_mode(operation)&&hb_mo_round(value)>=2)?(HB_MO_SIMPLE|(hb_mo_round(value)==3?HB_MO_SIMPLE_SCALE:0)):0);
        }
    }
    return selected==1&&(motion->render_flags&HB_MO_LEGACY_II)?11:selected;
}
/* Freeze each gesture's pitches at its musical onset. On a conductor, the
   first Conductor Chord gesture bootstraps from Scale Root if no harmony has
   been established yet; subsequent gestures can voice the recognized chord. */
static int hb_operation_modifier(Inst *instance,int source_note){
    int modifier=hb_mo_held_modifier(&instance->motion);
    if(!modifier)modifier=hb_mo_source_modifier(&instance->motion);
    for(int lane=0;lane<HB_MOTION_LANES&&!modifier;lane++){
        int operation=hb_mo_operation(&instance->motion,lane);double value=0;
        if((operation==HB_MO_BELOW||operation==HB_MO_ABOVE||operation==HB_MO_CHROM_ABOVE||operation==HB_MO_TRITONE_V)&&
            hb_mo_lane_active(&instance->motion,lane)&&
            hb_mo_value_at(&instance->motion,lane,hb_motion_position(instance),hb_motion_condition_position(),source_note,&value)&&value>0)
            {modifier=operation==HB_MO_BELOW?-1:(operation==HB_MO_CHROM_ABOVE||operation==HB_MO_TRITONE_V)?2:1;
            if(operation==HB_MO_CHROM_ABOVE)instance->motion.render_flags|=HB_MO_CONNECTOR_ABOVE;
            else instance->motion.render_flags&=~HB_MO_CONNECTOR_ABOVE;}
    }
    return modifier;
}
typedef struct {int root,intent_kind,intent_target,intent_minor;unsigned scale;hb_cp_config config;} hb_approach_result;
/* Shared by live chord construction and anchor-relative motif replay. */
static hb_approach_result hb_resolve_chord_approach(Inst *instance,int target_note,unsigned target_scale,
    hb_cp_config config,hb_harmony_t harmony,int secondary,const hb_cadence_step *cadence,int chord_modifier,int pad_approach){
    target_scale=hb_key_approach_scale(instance,target_note,target_scale);
    int intent_kind=0,intent_target=mod12(target_note),intent_minor=0;
    if(secondary==2&&hb_current_target_diminished(instance,target_scale,target_note))secondary=12;
        int root_note=target_note;
        config.mode=1;
        if(cadence){
            hb_cadence_result result=hb_resolve_cadence(instance,cadence,target_note,target_scale);
            root_note=result.root;target_scale=result.scale;config.quality=result.quality;
            intent_target=mod12(result.destination);intent_minor=result.minor;intent_kind=(cadence->kind==HB_CAD_DOMINANT||cadence->kind==HB_CAD_MINOR_DOMINANT)?(result.quality==9?3:2):cadence->kind==HB_CAD_LEADING?3:1;
            if(cadence->kind==HB_CAD_TARGET&&pad_approach)config.quality=config.chromatic_quality==6?hb_cp_auto_leading_quality(target_note+1,target_scale):hb_cp_chromatic_quality(config.chromatic_quality);
        }else if(secondary==12||secondary==13){
            root_note=target_note+(secondary==12?-1:1);config.quality=9;
            intent_kind=secondary==12?3:5;
            if(secondary==12){
                intent_minor=hb_target_minor(target_scale,target_note);
                target_scale=hb_relative_target_scale(instance,target_note,target_scale,1);
                target_scale=hb_relative_dominant_scale(instance,root_note,target_note,1,target_scale);
            }
        }else if(secondary){
            unsigned parent_collection=target_scale;
            int parent_root=target_note+hb_context_approach_offset(instance,secondary,target_note,parent_collection);
            intent_minor=hb_target_minor(target_scale,target_note);
            target_scale=hb_secondary_collection(instance,secondary,target_note,target_scale);
            root_note+=hb_context_approach_offset(instance,secondary,target_note,target_scale);
            if(secondary==5||secondary==6){
                static const int borrowed[]={2,2,3,13};
                target_scale=hb_explicit_scale_mask(mod12(target_note),borrowed[hb_policy_value(instance,HB_P_BORROWED)]);
            }
            if(secondary==2||secondary==6)target_scale=hb_relative_dominant_scale(instance,root_note,
                target_note+(secondary==6?3:0),0,target_scale);
            /* II/VI/target stack the actual parent collection. V retains its
               dominant function while taking extensions from that collection. */
            if(secondary==7)target_scale=hb_explicit_scale_mask(mod12(root_note),3); /* substitute ii: Dorian */
            config.quality=secondary==2||secondary==6?6:secondary==5||secondary==7?7:0;
            if(hb_target_simple_chord(instance)&&secondary!=2&&secondary!=3){
                int third=hb_nth_scale_interval_from_root((uint16_t)target_scale,mod12(root_note),2);
                int fifth=hb_nth_scale_interval_from_root((uint16_t)target_scale,mod12(root_note),4);
                int seventh=hb_nth_scale_interval_from_root((uint16_t)target_scale,mod12(root_note),6);
                config.quality=third==3?(fifth==6?8:7):(seventh==11?5:6);
                /* Preserve the parent's remaining ordinal degrees if the
                   simplified root changes (e.g. B to Bb as VI of D minor). */
                target_scale=(parent_collection&~(1u<<mod12(parent_root)))|(1u<<mod12(root_note));
            }
            if(secondary==3&&pad_approach){
                target_scale=hb_relative_dominant_scale(instance,target_note,target_note+1,1,hb_approach_scale(instance,harmony));
                config.quality=config.chromatic_quality==6?hb_cp_auto_leading_quality(target_note+1,target_scale):
                    hb_cp_chromatic_quality(config.chromatic_quality);
            }
        }else if(chord_modifier==2){
            root_note=target_note+1;
            /* subV takes Lydian-dominant extensions, preserving its dominant core. */
            if(instance->motion.render_flags&HB_MO_CONNECTOR_ABOVE){
                config.quality=config.chromatic_quality==6?hb_cp_auto_leading_quality(target_note,target_scale):hb_cp_chromatic_quality(config.chromatic_quality);
            }else{target_scale=hb_explicit_scale_mask(mod12(root_note),12);config.quality=6;}
        }else if(chord_modifier>0){
            root_note=hb_apply_approach(instance,target_note,HB_APPROACH_SCALE_ABOVE);config.quality=0;
        }else{
            if(chord_modifier<0)root_note=root_note>0?root_note-1:0;
            int resolution=chord_modifier<0?target_note:root_note+1;
            target_scale=hb_relative_dominant_scale(instance,root_note,resolution,1,target_scale);
            config.quality=config.chromatic_quality==6?hb_cp_auto_leading_quality(resolution,target_scale):
                hb_cp_chromatic_quality(config.chromatic_quality);
        }
        if(!intent_kind)intent_kind=secondary>=15&&secondary<=17?secondary-7:secondary==2||secondary==6?2:chord_modifier==2&&!(instance->motion.render_flags&HB_MO_CONNECTOR_ABOVE)?4:secondary?7:5;
        if(secondary==6)intent_target=mod12(target_note+3);
        if(!cadence&&!secondary)intent_minor=hb_target_minor(hb_approach_scale(instance,harmony),intent_target);
        while(root_note<0)root_note+=12;while(root_note>127)root_note-=12;
    return (hb_approach_result){root_note,intent_kind,intent_target,intent_minor,target_scale,config};
}
static void hb_player_note_on_config(Inst *instance,int source_note,int channel,int velocity,const hb_cp_config *onset){
    hb_chord_player *player=&instance->player;
    int pair_input=instance->chord_pair_render,pair_top=instance->chord_pair_top;
    if(instance->chord_pair_input&&instance->chord_pair_owner[instance->movy_playback!=0][channel]==source_note+1){
        for(int pitch=source_note+1;pitch<128;pitch++)if(instance->chord_pair_held[instance->movy_playback!=0][channel][pitch]){
            pair_input=1;pair_top=pitch;
        }
    }
    if(hb_cp_toggle_off_origin(player,source_note,channel,instance->movy_playback!=0))return;
    hb_harmony_t harmony=hb_render_harmony(instance);
    if(instance->role==1)harmony=hb_key_harmony(instance,harmony);
    instance->follower_path_harmony[source_note]=harmony;
    hb_harmony_t untransposed=hb_transpose_harmony(harmony,-g_bus.global_transpose);
    int scale_root=untransposed.root_pc;
    hb_resolve_follower_reference_root(instance,&scale_root);
    unsigned scale=hb_cp_mode(player)==1?hb_follower_input_scale(instance,scale_root):
        hb_explicit_scale_mask(scale_root,hb_parent_scale_index(instance,harmony));
    if(hb_cp_mode(player)!=1&&harmony.valid)
        scale=hb_output_chord_scale_at_transpose(instance,untransposed,scale_root,(uint16_t)scale,0);
    uint16_t dominant=(hb_cp_mode(player)==1||hb_policy_value(instance,HB_P_GAP))?0:hb_dominant_scale_mask(instance,harmony,mod12(scale_root+g_bus.global_transpose));
    if(dominant){
        scale=0;
        for(int pitch=0;pitch<12;pitch++)if(dominant&(1u<<pitch))scale|=1u<<mod12(pitch-g_bus.global_transpose);
    }
    hb_cp_config config=onset?*onset:hb_chord_config_at(instance,source_note,hb_motion_position(instance),hb_motion_condition_position());
    if(instance->role==0&&config.mode==2&&!harmony.valid)config.mode=1;
    hb_cp_config requested_config=config;
    int secondary=hb_secondary_at(instance,source_note);
    int source_modifier=hb_mo_source_modifier(&instance->motion);
    int operation_modifier=hb_operation_modifier(instance,source_note);
    int direct_approach=hb_approach_effective(instance->approach_control,instance->approach_pad_armed);
    int chord_active=config.mode!=0;
    int pad_approach=instance->role==1&&chord_active&&harmony.valid&&!instance->approach_rows.tokens[source_note]&&
        ((instance->movy_pad_shift[source_note]&&source_note+instance->movy_pad_shift[source_note]>=0&&
          source_note+instance->movy_pad_shift[source_note]<128)||
         (hb_chromatic_travel(instance)&&hb_chromatic_approach_target(instance,source_note,harmony)>=0));
    int target_note=instance->role==1?hb_map_follower_note_unoperated(instance,source_note):source_note+g_bus.global_transpose;
    unsigned target_scale=harmony.valid?hb_follower_scale_target(instance,harmony).pitch_mask:
        hb_transpose_mask((uint16_t)scale,g_bus.global_transpose);
    int top_note=(config.inversion==8||pair_input)&&chord_active;
    int melody=pair_input?pair_top+g_bus.global_transpose:target_note,modified_note=source_note;
    int chord_modifier=chord_active?(operation_modifier?operation_modifier:
        direct_approach==HB_APPROACH_CHROM_BELOW?-1:direct_approach==HB_APPROACH_SCALE_ABOVE?1:0):0;
    const hb_cadence_step *cadence=hb_mo_current_cadence(&instance->motion);
    if(secondary||cadence){
        unsigned harmonic_parent=hb_parent_chord_scale(instance,harmony,scale_root,hb_follower_input_scale(instance,scale_root),g_bus.global_transpose);
        if(harmonic_parent)target_scale=harmonic_parent;
    }
    target_scale=hb_key_approach_scale(instance,target_note,target_scale);
    int intent_kind=0,intent_target=mod12(target_note),intent_minor=0;
    if(secondary==2&&hb_current_target_diminished(instance,target_scale,target_note))secondary=12;
    int approach_chord=pad_approach||chord_modifier||((secondary||cadence)&&chord_active);
    unsigned anticipated_parent=approach_chord&&hb_key_for(instance).approach_scale?
        hb_key_approach_scale(instance,target_note,target_scale):0;
    if(approach_chord){
        hb_approach_result approach=hb_resolve_chord_approach(instance,target_note,target_scale,config,harmony,secondary,cadence,chord_modifier,pad_approach);
        int root_note=approach.root;target_scale=approach.scale;config=approach.config;
        intent_kind=approach.intent_kind;intent_target=approach.intent_target;intent_minor=approach.intent_minor;
        melody=root_note;modified_note=root_note-g_bus.global_transpose;
        while(modified_note<0)modified_note+=12;while(modified_note>127)modified_note-=12;
        instance->approach_pad_armed=HB_APPROACH_OFF;
    }else if(top_note&&direct_approach!=HB_APPROACH_OFF)melody=hb_apply_approach(instance,melody,direct_approach);
    if(approach_chord&&(harmony.valid||secondary||cadence)){
        unsigned parent=target_scale;scale=0;
        for(int pitch=0;pitch<12;pitch++)if(parent&(1u<<pitch))scale|=1u<<mod12(pitch-g_bus.global_transpose);
    }
    /* A functional II can introduce a chromatic root (F# into E minor in
       C major). Keep its approach identity; do not classify it as a raw
       chromatic key and replace its scale-derived voices with that family. */
    if(approach_chord&&(secondary||cadence))scale|=1u<<mod12(modified_note);
    int scale_mode=config.mode==1;
    if(instance->role==1&&scale_mode&&!approach_chord){
        /* Expand the already travelled input. This includes Follow Play and
           master transpose once; the physical source still owns releases. */
        modified_note=target_note-g_bus.global_transpose;
        scale=hb_transpose_mask((uint16_t)target_scale,-g_bus.global_transpose);
    }
    if(!scale_mode&&g_bus.global_transpose){
        unsigned shifted=0;
        for(int pitch=0;pitch<12;pitch++)if(scale&(1u<<pitch))shifted|=1u<<mod12(pitch+g_bus.global_transpose);
        scale=shifted;
    }
    int pitches[HB_CP_VOICES];
    unsigned semantic_mask=0;
    hb_cp_config voice_config=config;
    if(top_note)voice_config.inversion=0;
    if(instance->role==0&&scale_mode&&!approach_chord&&hb_policy_value(instance,HB_P_GAP)){
        hb_cp_config core_config=voice_config;core_config.size=3;
        unsigned core_mask=0;int core_notes[HB_CP_VOICES];
        hb_cp_voice_semantic(core_config,modified_note,harmony.root_pc,0,scale,core_notes,&core_mask);
        hb_harmony_t local={0};local.valid=1;local.root_pc=mod12(modified_note);local.bass_pc=local.root_pc;
        local.chord_index=HB_HARMONY_EXPLICIT_TONES;local.pitch_mask=(uint16_t)core_mask;
        local=hb_transpose_harmony(local,g_bus.global_transpose);
        scale=hb_transpose_mask(hb_local_output_scale(instance,local),-g_bus.global_transpose);
    }
    if(hb_key_for(instance).blues&&config.mode){
        voice_config.quality=6;
        scale=hb_explicit_scale_mask(scale_mode?mod12(modified_note):harmony.root_pc,6);
    }
    int voice_count=hb_cp_voice_semantic(voice_config,modified_note+(scale_mode?0:g_bus.global_transpose),
        harmony.root_pc,harmony.valid?(voice_config.size==HB_CP_FOLLOW_DETECTED?hb_harmony_detected_mask(harmony):hb_harmony_chord_mask(harmony)):0,
        (instance->role==0||harmony.valid||secondary||cadence)?scale:0,pitches,&semantic_mask);
    if(instance->role==1&&config.mode==0){
        /* Raw-note arps use the ordinary follower mapper before scheduling.
           Its result already includes master transpose. Keep the physical
           source key as owner so release still finds the remapped voice. */
        pitches[0]=hb_map_follower_note_now(instance,source_note);voice_count=1;
        int approach=hb_approach_effective(instance->approach_control,instance->approach_pad_armed);
        if(approach!=HB_APPROACH_OFF){
            pitches[0]=hb_apply_approach(instance,pitches[0],approach);
            instance->approach_pad_armed=HB_APPROACH_OFF;
        }
    }
    if(scale_mode&&voice_count){
        for(int voice=0;voice<voice_count;voice++)pitches[voice]+=g_bus.global_transpose;
        while(pitches[0]<0)for(int voice=0;voice<voice_count;voice++)pitches[voice]+=12;
        while(pitches[voice_count-1]>127)for(int voice=0;voice<voice_count;voice++)pitches[voice]-=12;
    }
    if(instance->role==1&&config.mode==2&&harmony.valid&&!approach_chord){
        unsigned collection=0;
        for(int voice=0;voice<voice_count;voice++)collection|=1u<<mod12(pitches[voice]);
        for(int voice=0;voice<voice_count;voice++)pitches[voice]=hb_play_note(instance,pitches[voice],harmony,collection);
        hb_cp_sort(pitches,voice_count);
    }
    /* Bind pending approaches to source voices, not generated arp/strum onsets. */
    if(source_modifier&&!approach_chord)for(int voice=0;voice<voice_count;voice++)
        pitches[voice]=hb_apply_approach(instance,pitches[voice],source_modifier<0?HB_APPROACH_CHROM_BELOW:source_modifier==2?HB_APPROACH_CHROM_ABOVE:HB_APPROACH_SCALE_ABOVE);
    if(top_note&&voice_count){
        if(source_modifier&&!approach_chord)melody=hb_apply_approach(instance,melody,
            source_modifier<0?HB_APPROACH_CHROM_BELOW:source_modifier==2?HB_APPROACH_CHROM_ABOVE:HB_APPROACH_SCALE_ABOVE);
        while(melody<0)melody+=12;while(melody>127)melody-=12;
        int root=config.mode==2?harmony.root_pc:mod12(modified_note+g_bus.global_transpose);
        unsigned collection=0;
        for(int voice=0;voice<voice_count;voice++)collection|=1u<<mod12(pitches[voice]);
        int fifth=mod12(root+7);
        if(!(collection&(1u<<fifth))){
            if(collection&(1u<<mod12(root+6)))fifth=mod12(root+6);
            else if(collection&(1u<<mod12(root+8)))fifth=mod12(root+8);
        }
        voice_count=hb_cp_top_note(pitches,voice_count,melody,root,fifth,config.voicing);
        instance->approach_pad_armed=HB_APPROACH_OFF;
    }
    if(pair_input&&voice_count){
        /* Explicit bass and top bound the generated inner voices. */
        int bass=hb_cp_clamp(source_note+g_bus.global_transpose,0,127);
        int top=hb_cp_clamp(pair_top+g_bus.global_transpose,0,127),count=0;
        int bounded[HB_CP_VOICES];bounded[count++]=bass;
        for(int voice=0;voice<voice_count&&count<HB_CP_VOICES-1;voice++)
            if(pitches[voice]>bass&&pitches[voice]<top)bounded[count++]=pitches[voice];
        if(top>bass)bounded[count++]=top;
        memcpy(pitches,bounded,(size_t)count*sizeof(int));voice_count=count;melody=top;
    }
    int reference_pitches[HB_CP_VOICES],reference_count=voice_count;memcpy(reference_pitches,pitches,sizeof(reference_pitches));
    hb_key_context voice_context=hb_key_for(instance);
    if(anticipated_parent)voice_context=hb_key_collection_context(voice_context,target_note,anticipated_parent);
    if(instance->role==0&&!voice_context.active&&!approach_chord){
        unsigned active=hb_dominant_scale_mask(instance,harmony,mod12(scale_root+g_bus.global_transpose));
        if(active){
            /* Render after capturing source semantics, just like a key change.
               Dominant colors must not become new classifier evidence. */
            voice_context.active=1;
            voice_context.source_root=voice_context.target_root=mod12(scale_root+g_bus.global_transpose);
            voice_context.source_mask=hb_transpose_mask(hb_follower_input_scale(instance,scale_root),g_bus.global_transpose);
            voice_context.target_mask=active;
        }
    }
    if(instance->role==0&&voice_context.active&&
       (voice_context.blues||g_key_conductor_travel||!hb_key_pitch_identity(voice_context))){
        /* Reinterpret the input, then voice it in the destination collection.
           Snapping each generated voice independently collapses chord forms. */
        hb_key_context context=voice_context;
        /* Resolve the source root in the new parent key before voicing. The
           previous bus chord is not a destination for this new conductor. */
        int mapped_input=hb_key_map(context,modified_note+g_bus.global_transpose);
        hb_harmony_t destination=hb_key_harmony(instance,harmony);
        unsigned mapped_scale=scale_mode&&!approach_chord?context.target_mask:
            hb_key_mask(context,hb_transpose_mask(scale,scale_mode?g_bus.global_transpose:0));
        unsigned active_family=context.blues?0:hb_dominant_scale_mask(instance,destination,context.target_root);
        /* A raw conductor chord chooses its own function below. A preceding
           V must not lend harmonic minor to the next tonic's seventh. */
        if(active_family&&!approach_chord&&(!scale_mode||!hb_key_for(instance).active))mapped_scale=active_family;
        if(!context.blues&&voice_config.mode){
            int original_root=scale_mode?mod12(modified_note+g_bus.global_transpose):harmony.root_pc;
            int source_target=approach_chord?intent_target:context.source_root;
            unsigned original_mask=0;for(int voice=0;voice<voice_count;voice++)original_mask|=1u<<mod12(reference_pitches[voice]);
            int functional=intent_kind==2||(intent_kind!=7&&mod12(original_root-source_target)==7&&(original_mask&(1u<<mod12(original_root+4)))&&!(original_mask&(1u<<mod12(original_root+11))));
            int leading=approach_chord&&intent_kind==3;
            int preceding=approach_chord&&intent_kind==1&&mod12(original_root-source_target)==2;
            int target=approach_chord?mod12(hb_key_map(context,60+source_target)):context.target_root;
            int family=hb_policy_value(instance,hb_target_minor(context.target_mask,target)?HB_P_DOMINANT_MINOR:HB_P_DOMINANT);
            int preparation=intent_kind>=8&&intent_kind<=10;
            if(preparation||(family&&(functional||leading||preceding))){
                unsigned collection=hb_function_family(instance,target,context.target_mask,hb_target_minor(context.target_mask,target),0);
                int new_root=mod12(target+(preparation?hb_nth_scale_interval_from_root(collection,target,1+2*(intent_kind-8)):functional?7:leading?11:2));
                int delta=new_root-mod12(mapped_input);while(delta>6)delta-=12;while(delta< -6)delta+=12;mapped_input+=delta;
                mapped_scale=hb_function_family(instance,target,context.target_mask,hb_target_minor(context.target_mask,target),functional?1:leading?2:0);
                if(functional)voice_config.quality=6;
                else if(leading)voice_config.quality=9;
                else voice_config.quality=0;
            }
        }
        if(context.blues&&voice_config.mode){
            voice_config.quality=6;
            mapped_scale=hb_explicit_scale_mask(voice_config.mode==1?mod12(mapped_input):destination.root_pc,6);
        }
        unsigned ignored_semantic=0;
        voice_count=hb_cp_voice_semantic(voice_config,mapped_input,destination.root_pc,
            destination.valid?(voice_config.size==HB_CP_FOLLOW_DETECTED?hb_harmony_detected_mask(destination):hb_harmony_chord_mask(destination)):0,
            mapped_scale,pitches,&ignored_semantic);
        /* Explicit inversions and played bass/top anchors remain intentional.
           The legacy non-Relative policies all preserve conductor identity;
           their separate chord/scale/split pools still apply to followers. */
        if(g_key_conductor_travel&&voice_config.inversion==0&&!top_note&&!pair_input)
            hb_key_travel_voicing(&instance->closest_assignments,pitches,voice_count,reference_pitches,reference_count,g_key_conductor_travel);
        if(top_note&&voice_count){
            int mapped_melody=hb_key_conductor_pitch(instance,melody),root=config.mode==2?destination.root_pc:mod12(mapped_input);
            unsigned collection=0;for(int voice=0;voice<voice_count;voice++)collection|=1u<<mod12(pitches[voice]);
            int fifth=mod12(root+7);
            if(!(collection&(1u<<fifth)))fifth=collection&(1u<<mod12(root+6))?mod12(root+6):collection&(1u<<mod12(root+8))?mod12(root+8):fifth;
            voice_count=hb_cp_top_note(pitches,voice_count,mapped_melody,root,fifth,config.voicing);
        }
        if(pair_input&&voice_count){
            int bass=hb_key_conductor_pitch(instance,source_note+g_bus.global_transpose);
            int top=hb_key_conductor_pitch(instance,pair_top+g_bus.global_transpose),count=0,bounded[HB_CP_VOICES];
            while(top<bass)top+=12;
            bounded[count++]=bass;
            for(int voice=0;voice<voice_count&&count<HB_CP_VOICES-1;voice++)if(pitches[voice]>bass&&pitches[voice]<top)bounded[count++]=pitches[voice];
            if(top>bass&&top<=127)bounded[count++]=top;
            memcpy(pitches,bounded,(size_t)count*sizeof(int));voice_count=count;
        }
    }
    if(!hb_cp_on_origin(player,source_note,channel,velocity,pitches,voice_count,instance->movy_playback!=0))return;
    for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *key=&player->keys[index];
        if(key->used&&key->source==source_note&&key->channel==channel&&key->playback_origin==(instance->movy_playback!=0)){
            memcpy(instance->key_reference[index],reference_pitches,sizeof(reference_pitches));
            key->root_pc=config.mode==0?mod12(reference_pitches[0]):(config.mode==2?harmony.root_pc:mod12(modified_note+g_bus.global_transpose));
            key->played_pitch=config.mode?(instance->role==0?hb_key_pitch(instance,melody):melody):pitches[0];
            key->intent_scale=((intent_kind>0&&intent_kind<4)||(intent_kind>=8&&intent_kind<=10))?anticipated_parent:0;
            if(!intent_kind&&instance->role==0&&config.mode==1&&config.quality==0){
                const unsigned long long *events=instance->motion.event_override?instance->motion.event_override:instance->motion.events;
                unsigned long long provenance=events[HB_MOTION_LANES];
                int recorded=(provenance&(1ULL<<20))!=0;
                int original_mode=recorded?(int)((provenance>>21)&3):config.mode;
                int original_quality=recorded?(int)((provenance>>23)&15):config.quality;
                int original_gap=recorded?(int)((provenance>>41)&3):hb_policy_value(instance,HB_P_GAP);
                if(original_mode==1&&!original_quality&&!original_gap){
                    int original_root=recorded?(int)((provenance>>32)&15):scale_root;
                    int original_scale=recorded?(int)((provenance>>36)&31):hb_follower_input_scale_index(instance,scale_root);
                    intent_kind=6;intent_target=mod12(original_root+g_bus.global_transpose);
                    key->intent_scale=hb_explicit_scale_mask(intent_target,original_scale);
                    intent_minor=(key->intent_scale&(1u<<mod12(intent_target+3)))&&!(key->intent_scale&(1u<<mod12(intent_target+4)));
                }
            }
            if(intent_kind==7)key->intent_scale=target_scale;
            key->intent_kind=intent_kind;key->intent_target=intent_target;key->intent_minor=intent_minor;
            key->gap_mask=approach_chord?target_scale:hb_transpose_mask(scale,scale_mode?g_bus.global_transpose:0);
            key->onset_config=approach_chord?requested_config:config;
            key->semantic_mask=semantic_mask;
            if(scale_mode&&g_bus.global_transpose){
                key->semantic_mask=0;
                for(int pitch=0;pitch<12;pitch++)if(semantic_mask&(1u<<pitch))
                    key->semantic_mask|=1u<<mod12(pitch+g_bus.global_transpose);
            }
            if(intent_kind==2)key->semantic_mask|=(1u<<key->root_pc)|(1u<<mod12(key->root_pc+4))|(1u<<mod12(key->root_pc+10));
            if(intent_kind==3)key->semantic_mask|=(1u<<key->root_pc)|(1u<<mod12(key->root_pc+3))|(1u<<mod12(key->root_pc+6))|(1u<<mod12(key->root_pc+9));
            if(intent_kind>=8&&intent_kind<=10){
                hb_cp_config identity=config;identity.mode=1;identity.size=3;
                int identity_notes[HB_CP_VOICES];unsigned identity_mask=0;
                hb_cp_voice_semantic(identity,60+key->root_pc,key->root_pc,0,target_scale,identity_notes,&identity_mask);
                key->semantic_mask|=identity_mask;
            }
            /* Retain explicitly requested quality, not internal approach-family
               substitutions, when the conductor form omits its seventh. */
            if(instance->role==0&&!approach_chord&&config.mode&&requested_config.quality>=5&&requested_config.quality<=12){
                static const int seventh[]={11,10,10,10,9,11,11,10};
                key->semantic_mask|=1u<<mod12(key->root_pc+seventh[requested_config.quality-5]);
            }
            if(instance->role==0){instance->dirty=1;instance->frames_since_change=0;instance->conductor_note_on_pending=1;}
            key->recordable=instance->role==0&&!instance->movy_playback&&!instance->synthetic_advance&&!instance->adopt_held;
            memcpy(instance->motion_player_events[index],instance->motion.event_override?instance->motion.event_override:instance->motion.events,sizeof(instance->motion.events));
            if(approach_chord)instance->motion_player_events[index][HB_MOTION_LANES]|=HB_MO_CHORD_APPROACH;
            key->playback_origin=instance->movy_playback;
            hb_input_intent input=hb_input_get(instance,source_note);
            key->input_degree=input.degree;key->input_target=input.target;key->input_shift=input.shift;key->input_token=input.token;
            key->trail_serial=instance->trail_event_serial?instance->trail_event_serial:instance->trail_input[(source_note&127)+(instance->movy_playback?128:0)];
            key->transform_revision=instance->play_revision;
            key->range=hb_play_applies(instance)?instance->play.range+1:1;
            key->harmony_sequence=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);
            key->harmony_root=harmony.valid?harmony.root_pc:-1;
            key->harmony_mask=harmony.valid?hb_harmony_chord_mask(harmony):0;
            hb_override_capture(instance,source_note,1,key->root_pc,pitches,voice_count,key->semantic_mask);
            /* A resolving target starts its own first-note-free attack. An
               overlapping or latched approach pool must not defer it until
               the old arp division, where a short target tap can vanish. */
            if(!instance->movy_playback&&instance->target_attack_owner==channel*128+source_note+1){
                instance->target_attack_owner=0;
                if(hb_cp_playback(player)==1&&hb_cp_settings(player)->phase!=1){player->running=0;player->step=0;}
            }
            break;
        }
    }
}
static void hb_player_note_on(Inst *instance,int source_note,int channel,int velocity){
    hb_player_note_on_config(instance,source_note,channel,velocity,0);
}
/* One post-render pipeline for local sound and the original MIDI broadcast.
   Harmony Choice is deliberately evaluated earlier, in hb_render_harmony. */
static double hb_motion_position(Inst *instance){return hb_clock_status()==MOVE_CLOCK_STATUS_RUNNING?hb_current_beat():instance->motion_beat;}
static double hb_motion_condition_position(void){
    if(hb_clock_status()!=MOVE_CLOCK_STATUS_RUNNING)return -1.0;
    /* hb_movy_clip publishes the completed tick, already master_tick - 1. */
    if(g_movy_present)return (double)g_movy_tick/g_movy_ppqn;
    return hb_current_beat();
}
static void hb_motion_values(Inst *instance,const uint8_t message[3],int *pitch,int *velocity,int *pan,double *off_beat,int *skip){
    *pitch=message[1];*velocity=message[2];*pan=-1;*off_beat=-1;*skip=0;
    double beat=hb_motion_position(instance),condition=hb_motion_condition_position();
    if(instance->role==0&&!hb_cp_mode(&instance->player)){
        int secondary=hb_secondary_at(instance,message[1]);
        const hb_cadence_step *cadence=hb_mo_current_cadence(&instance->motion);
        if(secondary||cadence){
            hb_harmony_t harmony=hb_render_harmony(instance);
            int parent_root=0;hb_resolve_follower_reference_root(instance,&parent_root);
            unsigned parent=hb_parent_chord_scale(instance,harmony,parent_root,hb_follower_input_scale(instance,parent_root),g_bus.global_transpose);
            if(cadence)*pitch=hb_resolve_cadence(instance,cadence,*pitch,parent).root;
            else *pitch+=hb_context_approach_offset(instance,secondary,*pitch,hb_secondary_collection(instance,secondary,*pitch,parent));
            while(*pitch<0)*pitch+=12;while(*pitch>127)*pitch-=12;
        }
    }
    for(int index=0;index<HB_MOTION_LANES;index++){
        double value;
        if(!hb_mo_value_at(&instance->motion,index,beat,condition,message[1],&value))continue;
        hb_motion_lane resolved=hb_mo_settings(&instance->motion,index);const hb_motion_lane *lane=&resolved;
        if(lane->operation==HB_MO_VELOCITY)*velocity=hb_mo_clamp(hb_mo_round(*velocity*(1.0+value/100.0)),1,127);
        else if(lane->operation==HB_MO_PAN)*pan=hb_mo_clamp(hb_mo_round(64.0+value*0.63),0,127);
        else if(lane->operation==HB_MO_OCTAVE){
            *pitch+=12*hb_mo_clamp(hb_mo_round(value),-4,4);
            while(*pitch<0)*pitch+=12;while(*pitch>127)*pitch-=12;
        }else if(lane->operation==HB_MO_ROTATE){
            hb_harmony_t harmony=hb_render_harmony(instance);if(!harmony.valid)continue;
            hb_harmony_t collection=instance->content_map==1?hb_follower_scale_target(instance,harmony):hb_follower_content_target(instance,harmony,instance->content_map);
            if(instance->content_map==2)collection.pitch_mask=0xfff;
            hb_fp_config transform={0};transform.rotate=hb_mo_clamp(hb_mo_round(value),-24,24);
            *pitch=hb_fp_note(transform,*pitch,harmony.root_pc,collection.pitch_mask);
        }else if(lane->operation==HB_MO_GATE){
            double deadline=beat+hb_mo_grid(lane->grid)*hb_mo_clamp(hb_mo_round(value),1,400)/100.0;
            if(*off_beat<0||deadline<*off_beat)*off_beat=deadline;
        }else if(lane->operation==HB_MO_SKIP&&value>0)*skip=1;
        else if(lane->operation==HB_MO_TRANSPOSE)*pitch=hb_mo_clamp(*pitch+hb_mo_round(value),0,127);
        else if(!hb_mo_chord_approach_done(&instance->motion)&&!(instance->motion.held&(1ULL<<index))&&(lane->operation==HB_MO_BELOW||lane->operation==HB_MO_ABOVE||lane->operation==HB_MO_CHROM_ABOVE||lane->operation==HB_MO_TRITONE_V)&&value>0)
            *pitch=hb_apply_approach(instance,*pitch,lane->operation==HB_MO_BELOW?HB_APPROACH_CHROM_BELOW:(lane->operation==HB_MO_CHROM_ABOVE||lane->operation==HB_MO_TRITONE_V)?HB_APPROACH_CHROM_ABOVE:HB_APPROACH_SCALE_ABOVE);
    }
}
static void hb_motion_resolve_output(Inst *instance,const uint8_t message[3],int *result_pitch,int *result_velocity,int *result_pan,double *result_off,int *result_skip){
    int pitch=message[1],velocity=message[2],pan=-1,skip=0;double off_beat=-1;
    if((message[0]&0xf0)==0x90&&message[2])hb_motion_values(instance,message,&pitch,&velocity,&pan,&off_beat,&skip);
    if((message[0]&0xf0)==0x90&&message[2]){
        int modifier=hb_mo_held_modifier(&instance->motion);
        if(!modifier&&(!hb_cp_enabled(&instance->player)||instance->movy_passthrough))modifier=hb_mo_source_modifier(&instance->motion);
        if(modifier&&!hb_mo_chord_approach_done(&instance->motion))pitch=hb_apply_approach(instance,pitch,modifier<0?HB_APPROACH_CHROM_BELOW:modifier==2?HB_APPROACH_CHROM_ABOVE:HB_APPROACH_SCALE_ABOVE);
    }
    *result_pitch=pitch;*result_velocity=velocity;*result_pan=pan;*result_off=off_beat;*result_skip=skip;
}
static unsigned hb_trail_target(Inst *instance,int source,int target,int velocity){
    int owner=(source&127)+(instance->movy_playback?128:0);
    unsigned serial=instance->trail_event_serial?instance->trail_event_serial:instance->trail_input[owner];
    if(!serial||instance->trail_queued[owner]>=serial)return 0;
    uint8_t message[3]={0x90,(uint8_t)target,(uint8_t)velocity};
    int pitch,gain,pan,skip;double off;
    hb_motion_resolve_output(instance,message,&pitch,&gain,&pan,&off,&skip);
    if(skip||!gain||pitch<0||pitch>127)return 0;
    instance->trail_queued[owner]=serial;return (unsigned)pitch+1;
}
#include "../../../src/harmony_override_input.h"
static void hb_motion_output(Inst *instance,hb_motion_route *route,const uint8_t message[3]){
    hb_rr_prepare(instance,route);
    if(instance->role>=2){hb_mo_event_owned(route,message,message[1],message[2],-1,-1,0,instance->motion_event_owner);return;}
    int pitch,velocity,pan,skip;double off_beat;
    hb_motion_resolve_output(instance,message,&pitch,&velocity,&pan,&off_beat,&skip);
    if(hb_mo_event_owned(route,message,pitch,velocity,pan,off_beat,skip,instance->motion_event_owner)&&!skip&&(message[0]&0xf0)==0x90&&message[2])
        hb_mo_repeat_schedule(route,&instance->motion,message,pitch,velocity,hb_motion_position(instance),hb_motion_condition_position(),instance->motion_beat);
}
static void hb_motion_harmony_refresh(Inst *instance){
    hb_harmony_t harmony=hb_render_harmony(instance);
    unsigned signature=harmony.valid?((unsigned)hb_harmony_chord_mask(harmony)<<4)|(unsigned)(harmony.root_pc+1):0;
    int active=0;for(int lane=0;lane<HB_MOTION_LANES;lane++)if(hb_mo_lane_active(&instance->motion,lane)&&instance->motion.lanes[lane].operation==HB_MO_HARMONY)active=1;
    if((active||instance->next_lookahead||instance->motion_harmony_signature)&&signature!=instance->motion_harmony_signature)instance->play_revision++;
    instance->motion_harmony_signature=(active||instance->next_lookahead)?signature:0;
}
static void hb_conductor_player_sense(Inst *instance){
    uint8_t next[128];
    memcpy(next,instance->passthrough_held,sizeof(next));
    for(int index=0;index<HB_CP_KEYS;index++){
        const hb_cp_key *key=&instance->player.keys[index];
        if(!key->used)continue;
        for(int voice=0;voice<key->count;voice++){
            /* The classifier consumes reference-key notes and transposes once. */
            int reference=instance->key_reference[index][voice]-g_bus.global_transpose;
            while(reference<0)reference+=12;while(reference>127)reference-=12;
            next[reference]=1;
        }
    }
    int addition=0;
    for(int pitch=0;pitch<128;pitch++){
        int was=instance->held_count[pitch]>0,now=next[pitch]!=0;
        if(was==now)continue;
        instance->held_count[pitch]=(uint8_t)now;
        instance->held_now[pitch]=(uint8_t)now;
        instance->pending_off_frames[pitch]=!now&&g_bus.analysis_release_ms>0?1:0;
        if(now)addition=1;
    }
    if(addition){
        instance->conductor_note_on_pending=1;
        instance->candidate_frames=0;
        instance->dirty=1;
        instance->frames_since_change=0;
    }
    hb_publish_instance_notes(instance);
}
static int hb_release_follower_queue(Inst *instance,int frames,int sample_rate,
                                     uint8_t output[][3],int lengths[],int max_output){
    if(!instance||instance->role!=1||max_output<=0)return 0;
    /* Resolve already-delivered conductor MIDI before mapping any due note.
       Movy supplies a block-wide conductor phase; older hosts use a zero-time
       flush here, without aging classifier/release timers twice. */
    if(!g_conductor_block_ready)hb_prepare_conductors(0,sample_rate);
    int target_frames=0; /* Follow Lookahead removed; Follower Buffer owns intentional pre-boundary delay. */
    int emitted=0;
    int next_override=hb_approach_normalize(instance->approach_pad_armed);
    int active_approach=hb_approach_effective(instance->approach_control,next_override);
    int next_approach_used=0;
    double next_arrival=-1.0;
    for(int index=0;index<instance->follower_queue_count;index++){
        int age=instance->follower_queue_age_frames[index];
        age+=frames;
        instance->follower_queue_age_frames[index]=age;
        if(age<target_frames||emitted>=max_output){
            /* Capacity pressure must never consume/drop a queued MIDI event.
               Leave it intact for the next tick. */
            continue;
        }
        double target_beat=hb_follower_playback_target(instance,index);
        if(target_beat>=0.0&&hb_current_beat()+1e-6<target_beat){
            continue;
        }
        /* Protect only the first release attempt of an UNCAPTURED note when
           a conductor update is concurrently pending. Never wait for harmony
           confirmation beyond that one scheduler tick, and never hold a note
           beyond an explicit pre-boundary target. */
        if(hb_follower_needs_same_tick_barrier(
               target_beat, age, frames, hb_conductor_pending_for_follow())){
            continue;
        }
        instance->render_harmony_active=0;
        double harmony_beat=instance->follower_queue_harmony_beat[index];
        if(harmony_beat>=0.0 && hb_render_shift_ready_for(instance)&&!hb_next_elapsed_for(instance)){
            double now=hb_current_beat();
            if(harmony_beat<now)harmony_beat=now;
            double phase=hb_next_phase(hb_clip_playhead()+harmony_beat-now+1e-7);
            int event=hb_next_model_event_for_phase_for(instance,phase,1);
            if(event>=0){
                instance->render_harmony=g_bus.next_model[event].harmony;
                instance->render_harmony_active=1;
            }
        }
        instance->motion.event_override=instance->motion_follower_events[index];
        int saved_origin=instance->movy_playback;
        instance->key_onset=instance->key_queue[index];instance->key_scope=1;
        instance->movy_playback=instance->follower_queue_origin[index];instance->trail_event_serial=instance->trail_queue_serial[index];
        int source_note=instance->follower_queue_note[index];
        hb_input_intent saved_input=hb_input_get(instance,source_note);
        hb_input_set(instance,source_note,instance->follower_queue_input[index]);
        int velocity=instance->follower_queue_velocity[index];
        int is_on=instance->follower_queue_on[index]!=0;
        if(hb_cp_enabled(&instance->player)){
            int channel=instance->follower_queue_channel[index];
            if(is_on){
                hb_player_note_on(instance,source_note,channel,velocity);
                double delay=target_beat>=0.0?target_beat-instance->follower_queue_arrival_beat[index]:0.0;
                instance->follower_origin_delay[instance->movy_playback!=0][source_note]=instance->follower_note_delay_beats[source_note]=delay>0.0?delay:0.0;
            }else{
                hb_cp_off_origin(&instance->player,source_note,channel,instance->movy_playback!=0);
                hb_override_capture(instance,source_note,0,0,0,0,0);
                instance->follower_origin_delay[instance->movy_playback!=0][source_note]=instance->follower_note_delay_beats[source_note]=0.0;
            }
            hb_input_set(instance,source_note,saved_input);
            instance->render_harmony_active=0;
            instance->follower_queue_age_frames[index]=-2147483647;
            instance->movy_playback=saved_origin;instance->trail_event_serial=0;instance->motion.event_override=0;instance->key_scope=0;
            continue;
        }
        hb_follower_voice *voice=hb_fv_find(instance->follower_voices,source_note,
            instance->follower_queue_channel[index],instance->movy_playback!=0,is_on);
        if(!voice)goto follower_event_done;
        int mapped;
        if(is_on){
            mapped=hb_map_follower_note_now(instance,source_note);
            int approach=active_approach;
            if(approach!=HB_APPROACH_OFF){
                int apply=1;
                if(next_override!=HB_APPROACH_OFF){
                    double arrival=instance->follower_queue_arrival_beat[index];
                    if(next_arrival<0.0)next_arrival=arrival;
                    double difference=arrival-next_arrival;
                    if(difference<0.0)difference=-difference;
                    /* A one-shot decorates one physical chord/gesture, even if
                       its note-ons serialize. Persistent toggles apply to every
                       subsequent gesture until explicitly disabled. */
                    if(difference>0.002)apply=0;
                }
                if(apply){
                    mapped=hb_apply_approach(instance,mapped,approach);
                    if(next_override!=HB_APPROACH_OFF)next_approach_used=1;
                }
            }
            *voice=(hb_follower_voice){.used=1,.source=source_note,.channel=instance->follower_queue_channel[index],
                .origin=instance->movy_playback!=0,.pitch=mapped,.velocity=velocity,.harmony=hb_render_harmony(instance),.input=hb_input_get(instance,source_note)};
            memcpy(voice->events,instance->motion_follower_events[index],sizeof(voice->events));
            double delay=target_beat>=0.0
                ?target_beat-instance->follower_queue_arrival_beat[index]:0.0;
            instance->follower_origin_delay[instance->movy_playback!=0][source_note]=instance->follower_note_delay_beats[source_note]=delay>0.0?delay:0.0;
            instance->source_seen[source_note%12]=1;
        }else{
            mapped=voice->pitch;
            voice->used=0;
            instance->follower_origin_delay[instance->movy_playback!=0][source_note]=instance->follower_note_delay_beats[source_note]=0.0;
        }
        hb_follower_summary(instance,source_note);
        instance->motion.event_override=voice->events;
        instance->motion_event_owner=hb_fv_identity(source_note,voice->channel,voice->origin);
        hb_override_capture(instance,source_note,is_on,mod12(mapped),&mapped,1,0);
        if(is_on)hb_trail_output(instance,output[emitted],hb_trail_target(instance,source_note,mapped,velocity));
        if(instance->motion_output_base){
            int offset=(int)(output+emitted-instance->motion_output_base);
            if(offset>=0&&offset<128){
                memcpy(instance->motion_output_events[offset],voice->events,sizeof(instance->motion.events));
                instance->motion_output_owner[offset]=instance->motion_event_owner;
                instance->motion_output_valid[offset]=1;instance->rhythm_output_prewarped[offset]=instance->movy_playback;
            }
        }
        output[emitted][0]=(uint8_t)((is_on?0x90:0x80)|instance->follower_queue_channel[index]);
        output[emitted][1]=(uint8_t)(mapped&0x7F);
        output[emitted][2]=(uint8_t)(is_on?velocity:0);
        lengths[emitted]=3;
        emitted++;
        if(instance->render_channel>=0)
            hb_render_follower_event(instance,mapped,velocity,is_on,!is_on,instance->follower_queue_channel[index]);
follower_event_done:
        hb_input_set(instance,source_note,saved_input);
        instance->motion_event_owner=0;
        instance->render_harmony_active=0;
        instance->movy_playback=saved_origin;instance->trail_event_serial=0;instance->motion.event_override=0;instance->key_scope=0;
        /* Mark released; compaction below drops it. */
        instance->follower_queue_age_frames[index]=-2147483647;
    }
    int write=0;
    for(int index=0;index<instance->follower_queue_count;index++){
        if(instance->follower_queue_age_frames[index]==-2147483647)continue;
        if(write!=index){
            instance->key_queue[write]=instance->key_queue[index];
            instance->follower_queue_input[write]=instance->follower_queue_input[index];
            instance->follower_queue_note[write]=instance->follower_queue_note[index];
            instance->follower_queue_velocity[write]=instance->follower_queue_velocity[index];
            instance->follower_queue_on[write]=instance->follower_queue_on[index];
            instance->follower_queue_channel[write]=instance->follower_queue_channel[index];
            instance->follower_queue_origin[write]=instance->follower_queue_origin[index];instance->trail_queue_serial[write]=instance->trail_queue_serial[index];
            memcpy(instance->motion_follower_events[write],instance->motion_follower_events[index],sizeof(instance->motion.events));
            instance->follower_queue_age_frames[write]=instance->follower_queue_age_frames[index];
            instance->follower_queue_target_beat[write]=instance->follower_queue_target_beat[index];
            instance->follower_queue_quant_beat[write]=instance->follower_queue_quant_beat[index];
            instance->follower_queue_harmony_beat[write]=instance->follower_queue_harmony_beat[index];
            instance->follower_queue_arrival_beat[write]=instance->follower_queue_arrival_beat[index];
        }
        write++;
    }
    instance->follower_queue_count=write;
    if(next_approach_used)hb_consume_next_approach(instance);
    return emitted;
}
/* Revoice chord-player owners through the same path as their original press.
   The player's owned-note diff drains OFFs before ONs, even at small capacity. */
static void hb_reharmonize_held_chords(Inst *instance){
    int transform_changed=instance->play_applied!=instance->play_revision;
    if(!transform_changed&&hb_cp_mode(&instance->player)!=2&&!(hb_cp_mode(&instance->player)==0&&hb_cp_playback(&instance->player)))return;
    /* Pending input owns its future onset, not the harmony of existing owners.
       Due input is released before this call; remaining queued notes must not
       block revoicing the held chord before this tick's arp step is emitted. */
    unsigned sequence=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);
    if(sequence==instance->follower_bus_seq&&!transform_changed)return;
    instance->follower_bus_seq=sequence;
    instance->play_applied=instance->play_revision;
    if(!instance->retrigger_held&&!(transform_changed&&hb_cp_playback(&instance->player)==1))return;
    hb_harmony_t harmony=hb_render_harmony(instance);if(!harmony.valid)return;
    hb_chord_player *player=&instance->player;
    unsigned mask=hb_harmony_chord_mask(harmony);
    hb_cp_config *runtime_config=player->state_override?&player->state_config:&player->config;
    int changed=0,latch=runtime_config->latch,armed=instance->approach_pad_armed;
    /* A harmony update is not a fresh physical press: it must neither replace
       the latched gesture nor consume a next-note modifier. */
    runtime_config->latch=0;instance->approach_pad_armed=HB_APPROACH_OFF;
    for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *key=&player->keys[index];
        if(!key->used||(key->release_end>0&&!key->release_follow))continue; /* Default: frozen release harmony. */
        int transform_due=key->transform_revision!=instance->play_revision;
        if(!transform_due&&(key->harmony_sequence==sequence||
            (key->harmony_root==harmony.root_pc&&key->harmony_mask==mask)))continue;
        int held=key->held;unsigned order=key->sequence;
        int release_synced=key->release_synced,release_follow=key->release_follow;double release_start=key->release_start,release_end=key->release_end;
        key->release_end=0; /* Following harmony does not re-trigger the release envelope. */
        instance->motion.event_override=instance->motion_player_events[index];
        int saved_origin=instance->movy_playback;
        instance->movy_playback=key->playback_origin;
        hb_cp_config current_config=*runtime_config;
        *runtime_config=key->onset_config;runtime_config->latch=0;
        hb_cp_config onset_config=key->onset_config;
        hb_input_intent saved_input=hb_input_get(instance,key->source);
        hb_input_set(instance,key->source,(hb_input_intent){key->input_degree,key->input_target,key->input_shift,key->input_token});
        hb_player_note_on_config(instance,key->source,key->channel,key->velocity,&onset_config);
        hb_input_set(instance,key->source,saved_input);
        *runtime_config=current_config;
        instance->movy_playback=saved_origin;
        instance->motion.event_override=0;
        key->held=held;key->sequence=order;changed=1;
        key->release_synced=release_synced;key->release_follow=release_follow;key->release_start=release_start;key->release_end=release_end;
    }
    runtime_config->latch=latch;instance->approach_pad_armed=armed;
    /* Revoicing is not a new gesture. In particular, resetting Auto at a
       due division would arm the following division and drop this hit. */
    if(changed&&hb_cp_playback(player)!=1){player->running=0;player->step=0;}
}
static int hb_reharmonize_held_follower(Inst *instance,uint8_t output[][3],int lengths[],int max_output){
    if(!instance||instance->role!=1||!output||!lengths||max_output<=0)return 0;
    unsigned bus_seq=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);
    if(bus_seq==instance->follower_bus_seq&&instance->play_applied==instance->play_revision)return 0;
    instance->follower_bus_seq=bus_seq;
    instance->play_applied=instance->play_revision;
    /* Off means event-preserving follower behavior: harmony changes affect the
       next source note-on, but never synthesize extra note-offs/note-ons for a
       note that is already held. This keeps follower rhythm exactly aligned
       with its source clip/performance. */
    if(!instance->retrigger_held)return 0;
    hb_harmony_t harmony=hb_render_harmony(instance);
    if(!harmony.valid)return 0;

    int owners[HB_FOLLOWER_VOICES],new_outputs[HB_FOLLOWER_VOICES],count=0,pending=0;
    int saved_origin=instance->movy_playback;
    const unsigned long long *saved_events=instance->motion.event_override;
    for(int slot=0;slot<HB_FOLLOWER_VOICES;slot++){
        hb_follower_voice *voice=&instance->follower_voices[slot];
        if(!voice->used)continue;
        instance->movy_playback=voice->origin;instance->motion.event_override=voice->events;
        hb_input_intent saved_input=hb_input_get(instance,voice->source);
        hb_input_set(instance,voice->source,voice->input);
        int pitch=hb_map_follower_note_now(instance,voice->source);
        hb_input_set(instance,voice->source,saved_input);
        if(voice->pitch==pitch){voice->harmony=hb_render_harmony(instance);hb_follower_summary(instance,voice->source);continue;}
        /* Reserve both edges before changing ownership. Retry excess voices on
           the next tick instead of losing their OFF under output pressure. */
        if(2*(count+1)>max_output){pending=1;continue;}
        owners[count]=slot;new_outputs[count++]=pitch;
    }
    if(pending){instance->follower_bus_seq=bus_seq-1;instance->play_applied=instance->play_revision-1;}
    int emitted=0;
    for(int pass=0;pass<2;pass++)for(int index=0;index<count;index++){
        hb_follower_voice *voice=&instance->follower_voices[owners[index]];
        instance->movy_playback=voice->origin;instance->motion.event_override=voice->events;
        instance->motion_event_owner=hb_fv_identity(voice->source,voice->channel,voice->origin);
        int pitch=pass?new_outputs[index]:voice->pitch;
        if(instance->motion_output_base){
            int offset=(int)(output+emitted-instance->motion_output_base);
            if(offset>=0&&offset<128){
                memcpy(instance->motion_output_events[offset],voice->events,sizeof(voice->events));
                instance->motion_output_valid[offset]=1;instance->rhythm_output_prewarped[offset]=(uint8_t)voice->origin;
                instance->motion_output_owner[offset]=instance->motion_event_owner;
            }
        }
        output[emitted][0]=(uint8_t)((pass?0x90:0x80)|voice->channel);
        output[emitted][1]=(uint8_t)pitch;output[emitted][2]=(uint8_t)(pass?voice->velocity:0);
        lengths[emitted++]=3;
        if(instance->render_channel>=0)hb_inject_follower_note(instance,pitch,pass?voice->velocity:0,pass);
        if(pass){voice->pitch=pitch;voice->harmony=hb_render_harmony(instance);hb_follower_summary(instance,voice->source);}
    }
    instance->movy_playback=saved_origin;instance->motion.event_override=saved_events;instance->motion_event_owner=0;
    return emitted;
}

static void hb_scan_raw_midi_out(Inst *instance) {
    if (!instance || !g_host || !g_host->mapped_memory) return;
    const uint8_t *raw = g_host->mapped_memory;
    int recv_channel = instance->source_channel>=0
        ? instance->source_channel : instance->resolved_source_channel;

    for (int offset = 0; offset + 3 < HB_MIDI_OUT_BYTES; offset += 4) {
        const uint8_t header = raw[offset];
        const uint8_t status = raw[offset + 1];
        const uint8_t data1 = raw[offset + 2];
        const uint8_t data2 = raw[offset + 3];

        if (header == 0 && status == 0 && data1 == 0 && data2 == 0) {
            memset(instance->raw_prev + offset, 0, 4);
            continue;
        }

        if (instance->raw_prev[offset] == header &&
            instance->raw_prev[offset + 1] == status &&
            instance->raw_prev[offset + 2] == data1 &&
            instance->raw_prev[offset + 3] == data2) {
            continue;
        }

        instance->raw_prev[offset] = header;
        instance->raw_prev[offset + 1] = status;
        instance->raw_prev[offset + 2] = data1;
        instance->raw_prev[offset + 3] = data2;

        const int cin = header & 0x0F;
        const int cable = (header >> 4) & 0x0F;
        const int type = status & 0xF0;
        const int channel = status & 0x0F;

        if (cin < 0x08 || cin > 0x0E) continue;
        if (type < 0x80 || type > 0xE0) continue;

        instance->raw_event_count++;
        instance->raw_last_status = status;
        instance->raw_last_channel = channel;
        instance->raw_last_cable = cable;
        instance->raw_last_velocity = data2;

        if (type != 0x80 && type != 0x90) continue;
        if (recv_channel >= 0 && channel != recv_channel) continue;

        const int is_on = (type == 0x90 && data2 > 0);
        const int is_off = (type == 0x80 || (type == 0x90 && data2 == 0));
        if (!(is_on || is_off) || data1 > 127) continue;

        instance->raw_note_count++;
        instance->raw_last_note = data1;
        if (is_on) instance->raw_note_on_count++;
        if (is_off) instance->raw_note_off_count++;

        /* Diagnostics only.
         *
         * Do NOT promote MIDI_OUT mailbox snapshots into musical held-note
         * state.  Hardware testing showed that enabling a track MIDI-Out route
         * can create echoed/repeated traffic here, while the snapshot can miss
         * matching note-offs.  Treating that as authoritative produced stuck
         * notes and event storms.  Realtime conductor sensing must come from an
         * event-preserving upstream track-input/instrument-bound tap instead. */
    }
}


static int hb_same_harmony(hb_harmony_t left,hb_harmony_t right){
    /* Bass is part of harmonic identity for the bus.  C, C/E and C/G share
       root/quality, but followers and display should still see the inversion
       change immediately rather than treating all three as identical. */
    return left.valid&&right.valid&&
           left.root_pc==right.root_pc&&
           left.chord_index==right.chord_index&&
           left.bass_pc==right.bass_pc;
}
static int hb_character_change(hb_harmony_t candidate,hb_harmony_t committed){
    if(!candidate.valid||!committed.valid)return 0;
    /* Root motion and inversion motion are both musically structural for the
       harmony bus, even when the pitch-class set itself is unchanged. */
    if(candidate.root_pc!=committed.root_pc)return 1;
    if(candidate.bass_pc!=committed.bass_pc&&candidate.chord_index==committed.chord_index)return 1;

    /* Ignore pure subset/superset changes. They usually represent voicing
       thinning/thickening (F -> F5 -> F, adding/removing 7/9, etc.). */
    uint16_t candidate_mask=candidate.pitch_mask;
    uint16_t committed_mask=committed.pitch_mask;
    if((candidate_mask&~committed_mask)==0||(committed_mask&~candidate_mask)==0)return 0;

    /* Same-root, non-nested pitch sets disagree about chord-defining content:
       major/minor/diminished/augmented/sus character, altered fifths, etc. */
    return 1;
}
static uint16_t hb_active_mask(const Inst *instance){
    uint16_t mask=0;for(int note=0;note<128;note++)if(instance->active[note])mask|=(uint16_t)(1u<<(note%12));return mask;
}
static int hb_timescale_frames(int sample_rate){
    if(g_bus.chord_timescale==0)return 0;
    float bpm=(g_host&&g_host->get_bpm)?g_host->get_bpm():120.0f;if(bpm<20.0f)bpm=120.0f;
    float quarter_seconds=60.0f/bpm;
    static const float quarter_units[6]={0.0f,0.25f,0.5f,1.0f,2.0f,4.0f};
    return (int)(quarter_seconds*quarter_units[g_bus.chord_timescale]*(float)sample_rate);
}
static int hb_confirm_frames(int sample_rate){
    int base_ms=g_bus.stability==0?25:(g_bus.stability==1?70:140);
    int dwell=hb_timescale_frames(sample_rate);
    int scaled=g_bus.stability==0?dwell/12:(g_bus.stability==1?dwell/6:dwell/4);
    int cap_ms=g_bus.stability==0?70:(g_bus.stability==1?140:280);
    int frames=base_ms*sample_rate/1000;if(scaled>frames)frames=scaled;
    int cap=cap_ms*sample_rate/1000;if(frames>cap)frames=cap;return frames;
}
static uint16_t hb_scale_mask(hb_harmony_t harmony){
    static const int major[7]={0,2,4,5,7,9,11};
    static const int minor[7]={0,2,3,5,7,8,10};
    uint16_t relative=harmony.pitch_mask;
    int has_minor=(relative&(1u<<((harmony.root_pc+3)%12)))!=0;
    int has_major=(relative&(1u<<((harmony.root_pc+4)%12)))!=0;
    const int *degrees=(has_minor&&!has_major)?minor:major;
    uint16_t mask=0;for(int i=0;i<7;i++)mask|=(uint16_t)(1u<<((harmony.root_pc+degrees[i])%12));return mask;
}
static uint16_t hb_explicit_scale_mask(int root_pc,int scale_index){
    return scale_index>0&&scale_index<HB_SCALE_COUNT?hb_transpose_mask(HB_SCALE_MASKS[scale_index],root_pc):0;
}
static hb_harmony_t hb_follower_scale_target(Inst *instance,hb_harmony_t harmony){
    if(!harmony.valid)return harmony;

    /* Preserve the explicit follower baseline, accommodating the actual chord. */
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))
        source_root=harmony.root_pc;

    int parent_scale_index=hb_parent_scale_index(instance,harmony);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);

    harmony.pitch_mask=(uint16_t)(hb_output_chord_scale(instance,harmony,source_root,parent_scale)|hb_harmony_chord_mask(harmony));
    return harmony;
}
static hb_harmony_t hb_mapping_target(hb_harmony_t harmony,int map_target){
    if(!harmony.valid)return harmony;
    /* Followers follow the inferred HARMONY, not the conductor's literal
       voicing. A shell such as C-E-Bb should still expose the implied G to
       Chord/Nearest mapping, and inversions should not change the available
       chord-tone set merely because the bass moved. */
    harmony.pitch_mask=hb_harmony_chord_mask(harmony);
    if(map_target==0)return harmony;
    harmony.pitch_mask=hb_scale_mask(harmony);
    return harmony;
}
static int hb_harmony_is_minor(hb_harmony_t harmony){
    if(!harmony.valid)return 0;
    int minor_pc=(harmony.root_pc+3)%12,major_pc=(harmony.root_pc+4)%12;
    return (harmony.pitch_mask&(1u<<minor_pc))&&!(harmony.pitch_mask&(1u<<major_pc));
}
static int hb_auto_prefers_flats(hb_harmony_t harmony){
    static const unsigned flat_major=(1u<<5)|(1u<<10)|(1u<<3)|(1u<<8)|(1u<<1)|(1u<<6);
    static const unsigned flat_minor=(1u<<2)|(1u<<7)|(1u<<0)|(1u<<5)|(1u<<10)|(1u<<3)|(1u<<8);
    unsigned bit=1u<<harmony.root_pc;return hb_harmony_is_minor(harmony)?((flat_minor&bit)!=0):((flat_major&bit)!=0);
}
static void hb_update_auto_spelling(hb_harmony_t harmony){
    if(g_bus.accidentals!=0||g_bus.auto_spell_locked||!harmony.valid)return;
    g_bus.auto_spell_sharps=hb_auto_prefers_flats(harmony)?0:1;
    g_bus.auto_spell_locked=1;
}
static unsigned hb_accidental_sharp_mask(hb_harmony_t context){
    /* bits are pitch classes; only black keys matter */
    static const unsigned all_sharp=(1u<<1)|(1u<<3)|(1u<<6)|(1u<<8)|(1u<<10);
    static const unsigned mode2=(1u<<1)|(1u<<3)|(1u<<6)|(1u<<8);       /* Bb */
    static const unsigned mode3=(1u<<1)|(1u<<6)|(1u<<8);               /* Eb,Bb */
    static const unsigned mode4=(1u<<1)|(1u<<6);                       /* Eb,Ab,Bb */
    static const unsigned mode5=(1u<<6);                               /* Db,Eb,Ab,Bb */
    if(g_bus.accidentals==1)return all_sharp;
    if(g_bus.accidentals==2)return mode2;
    if(g_bus.accidentals==3)return mode3;
    if(g_bus.accidentals==4)return mode4;
    if(g_bus.accidentals==5)return mode5;
    if(g_bus.accidentals==6)return 0;
    /* Auto is locked from the first committed harmonic context so later
       chords inherit the same key-signature family. */
    (void)context;return g_bus.auto_spell_sharps?all_sharp:0;
}
static const char *hb_pc_display(int pc,hb_harmony_t context){
    static const char *sharp[12]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    static const char *flat[12]={"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
    pc%=12;if(pc<0)pc+=12;
    unsigned mask=hb_accidental_sharp_mask(context);
    return (mask&(1u<<pc))?sharp[pc]:flat[pc];
}
static const char *hb_quality_suffix(int chord_index){
    static const char *suffix[]={"","m","5","s2","s4","dim","aug","6","m6","M7","7","m7","mM7","m7b5","dim7","a9","ma9","M9","9","m9","11","m11","13","6/9","m6/9","m(b6,9)","7#5","M7#5","9#5","M9#5","7b5","9b5","mMaj9","7sus4","9sus4","7sus2","M7b5","m9b5","7b9","7#9","7#5b9","7#5#9"};
    chord_index&=~HB_HARMONY_EXPLICIT_TONES;
    return (chord_index>=0&&chord_index<(int)(sizeof(suffix)/sizeof(suffix[0])))?suffix[chord_index]:"";
}
static int hb_format_harmony(char *buffer,int length,hb_harmony_t harmony){
    if(!harmony.valid)return snprintf(buffer,(size_t)length,"--");
    if(harmony.chord_index<0)return snprintf(buffer,(size_t)length,"%s?",hb_pc_display(harmony.root_pc,harmony));
    if(harmony.chord_index&HB_HARMONY_EXPLICIT_TONES)return snprintf(buffer,(size_t)length,"%s%s+",hb_pc_display(harmony.root_pc,harmony),hb_quality_suffix(harmony.chord_index));
    if(harmony.bass_pc!=harmony.root_pc)return snprintf(buffer,(size_t)length,"%s%s/%s",hb_pc_display(harmony.root_pc,harmony),hb_quality_suffix(harmony.chord_index),hb_pc_display(harmony.bass_pc,harmony));
    return snprintf(buffer,(size_t)length,"%s%s",hb_pc_display(harmony.root_pc,harmony),hb_quality_suffix(harmony.chord_index));
}

static const char *hb_note_name_with_octave(int midi_note,hb_harmony_t context,char *buffer,int length){
    if(!buffer||length<2)return "";
    if(midi_note<0||midi_note>127){snprintf(buffer,(size_t)length,"--");return buffer;}
    const char *pc=hb_pc_display(midi_note%12,context);
    int octave=(midi_note/12)-1;
    snprintf(buffer,(size_t)length,"%s%d",pc,octave);
    return buffer;
}
static int hb_nth_held_note(const Inst *instance,int ordinal){
    if(!instance||ordinal<0)return -1;
    int seen=0;
    for(int note=0;note<128;note++){
        if(instance->held_now[note]){
            if(seen==ordinal)return note;
            seen++;
        }
    }
    return -1;
}
static int hb_held_count(const Inst *instance){
    if(!instance)return 0;
    int count=0;
    for(int note=0;note<128;note++)if(instance->held_now[note])count++;
    return count;
}
static int hb_nth_follower_note(const Inst *instance,int ordinal){
    if(!instance||instance->role!=1||ordinal<0)return -1;
    int seen=0;
    for(int note=0;note<128;note++){
        if(instance->follower_held[note]){
            if(seen==ordinal)return note;
            seen++;
        }
    }
    return -1;
}
static int hb_follower_held_count(const Inst *instance){
    if(!instance||instance->role!=1)return 0;
    int count=0;
    for(int note=0;note<128;note++)if(instance->follower_held[note])count++;
    return count;
}
static const char *hb_role_name_for_interval(int interval){
    static const char *roles[12]={"Root","b2","2/9","b3","3rd","4/11","b5","5th","#5/b6","6/13","b7","7th"};
    return roles[mod12(interval)];
}
static const char *hb_follower_role_name(const Inst *instance,int ordinal){
    int note=hb_nth_follower_note(instance,ordinal);
    return hb_follower_degree_role_for_note((Inst*)instance,note);
}
static int hb_nth_active_note(const Inst *instance,int ordinal){
    return hb_nth_observed_note(instance,ordinal);
}
static int hb_format_active_notes(const Inst *instance,char *buffer,int length){
    if(!buffer||length<2)return -1;
    uint8_t notes[64];
    int count=hb_observed_notes(instance,notes,64);
    int used=0;
    for(int index=0;index<count;index++){
        int written=snprintf(buffer+used,(size_t)(length-used),used?",%d":"%d",(int)notes[index]);
        if(written<0||used+written>=length)return used;
        used+=written;
    }
    if(used==0)return snprintf(buffer,(size_t)length,"--");
    return used;
}
static unsigned hb_active_pc_mask(const Inst *instance){
    unsigned mask=0;
    uint8_t notes[64];
    int count=hb_observed_notes(instance,notes,64);
    for(int index=0;index<count;index++)mask|=(1u<<(notes[index]%12));
    return mask;
}
static int active_notes(const Inst *instance,uint8_t *output){return hb_observed_notes(instance,output,128);}
static int local_conductor_notes(const Inst *instance,uint8_t *output,int max_notes){
    if(!instance||instance->role!=0||!output||max_notes<=0)return 0;
    int count=0;
    for(int note=0;note<128&&count<max_notes;note++)
        if(instance->held_count[note]>0||instance->pending_off_frames[note]>0)
            output[count++]=(uint8_t)note;
    return count;
}
/* Semantic evidence is separate from the sounding-note display. Shell and
   rootless gestures retain their defining root/fifth for harmonic inference. */
static int local_conductor_harmony_notes(const Inst *instance,uint8_t *output){
    uint8_t present[128]={0};
    int count=local_conductor_notes(instance,output,128);
    unsigned pitch_classes=0;
    for(int index=0;index<count;index++){present[output[index]]=1;pitch_classes|=1u<<mod12(output[index]);}
    for(int index=0;index<HB_CP_KEYS;index++){
        const hb_cp_key *key=&instance->player.keys[index];
        if(!key->used)continue;
        for(int pitch=0;pitch<12;pitch++)if(key->semantic_mask&(1u<<pitch))
            {
                int reference=mod12(pitch-g_bus.global_transpose);
                if(!(pitch_classes&(1u<<reference)))present[116+mod12(reference-116)]=1;
            }
    }
    for(int index=0;index<HB_MT_SCHEDULE;index++){
        const hb_mt_scheduled *event=&instance->motif.events[index];
        if(!event->used||!event->started)continue;
        unsigned mask=event->semantic_mask?event->semantic_mask:1u<<mod12(event->reference_pitch);
        for(int pitch=0;pitch<12;pitch++)if(mask&(1u<<pitch)){
            int reference=mod12(pitch-g_bus.global_transpose);
            if(!(pitch_classes&(1u<<reference)))present[116+mod12(reference-116)]=1;
        }
    }
    count=0;for(int pitch=0;pitch<128;pitch++)if(present[pitch])output[count++]=(uint8_t)pitch;
    return count;
}
static int infer_reference_root(Inst *instance){static const int major[7]={0,2,4,5,7,9,11};static const int minor[7]={0,2,3,5,7,8,10};int pitch_classes=0,best=-999,best_root=instance->resolved_root;for(int index=0;index<12;index++)pitch_classes+=instance->source_seen[index]?1:0;if(!pitch_classes)return instance->resolved_root;for(int root=0;root<12;root++)for(int scale=0;scale<2;scale++){int score=0;for(int pitch_class=0;pitch_class<12;pitch_class++)if(instance->source_seen[pitch_class]){int relative=mod12(pitch_class-root),inside=0;for(int degree=0;degree<7;degree++)if(relative==(scale?minor[degree]:major[degree])){inside=1;break;}score+=inside?5:-4;}if(instance->source_seen[root])score+=3;if(score>best){best=score;best_root=root;}}instance->resolved_confidence=pitch_classes>=4?80:(pitch_classes>=3?65:45);return best_root;}
static int reference_root(Inst *instance){if(g_bus.global_root_policy==0)return mod12(g_bus.global_explicit_root);if(g_bus.global_root_policy==1)return mod12(g_bus.global_input_root);instance->resolved_root=infer_reference_root(instance);return mod12(instance->resolved_root);}
static int hb_player_tick(Inst *instance,uint8_t output[][3],int lengths[],int max_output);
static int hb_mt_tick(Inst *instance,uint8_t output[][3],int lengths[],int capacity);
static void hb_init_instance(Inst *instance){memset(instance,0,sizeof(*instance));instance->key_owner=hb_override_index(instance);hb_mt_init(&instance->motif);instance->motif_load[0]=1;instance->motif_load[1]=instance->motif_load_last=60;instance->motif_load[3]=12;hb_ar_init(&instance->approach_rows);instance->dominant_color_family=3;instance->target_scale_policy[2]=2;instance->chord_edit_lane=-1;instance->used=1;instance->render_velocity_gain=10000;instance->next_predict=1;instance->next_anti_buffer_ms=25;instance->boundary_buffer_ms=-3;hb_cp_defaults(&instance->player.config);instance->player.config.order=5;instance->player.config.chromatic_quality=3;hb_mo_defaults(&instance->motion);if(g_motion_settings_ready)hb_motion_copy_settings(&instance->motion,&g_motion_settings);hb_mo_route_init(&instance->motion_local);hb_mo_route_init(&instance->motion_render);instance->motion_local.strict_ownership=instance->motion_render.strict_ownership=1;instance->player.render_channel=-1;instance->movy_track=-1;instance->role=2;instance->mode=0;instance->content_map=1;instance->travel_map=7;instance->chromatic_map=1;memcpy(instance->touch_lanes,g_touch_lanes,sizeof(g_touch_lanes));instance->follower_split_map=0;instance->quant_timing=0;instance->approach_control=HB_APPROACH_OFF;instance->approach_mode=0;instance->map_target=0;instance->window_ms=25;instance->last_note=-1;instance->last_status=-1;instance->last_velocity=-1;instance->raw_last_note=-1;instance->raw_last_status=-1;instance->raw_last_velocity=-1;instance->raw_last_channel=-1;instance->raw_last_cable=-1;instance->render_channel=-1;instance->source_channel=-1;instance->resolved_source_channel=-1;instance->render_last_note=-1;instance->retrigger_held=1;instance->follow_lookahead_ms=0;instance->approach_pad_armed=HB_APPROACH_OFF;instance->approach_below_held=0;instance->approach_above_held=0;instance->follower_queue_count=0;for(int note=0;note<128;note++){instance->mapped[note]=-1;instance->follower_role_interval[note]=255;}hb_role_sync(instance);}
static void *create_inst(const char *module_dir,const char *config_json){(void)module_dir;(void)config_json;ensure_init();for(int index=0;index<HB_MAX_INSTANCES;index++)if(!g_pool[index].used){Inst *instance=&g_pool[index];g_conductor_block_ready=0;memset(&g_movy_clips[index],0,sizeof(g_movy_clips[index]));hb_init_instance(instance);return instance;}return 0;}
static void destroy_inst(void *value){Inst *instance=(Inst*)value;
if(instance){
    hb_override_clear(instance);
    memset(&g_sc_key_sequence[instance-g_pool],0,sizeof(g_sc_key_sequence[0]));g_sc_key_audition[instance-g_pool].on=g_sc_key_request[instance-g_pool].on=0;
    memset(g_sc_live[instance-g_pool],0,sizeof(g_sc_live[0]));g_sc_manual[instance-g_pool]=g_sc_latch[instance-g_pool]=g_sc_lane[instance-g_pool]=0;
    if(instance->movy_track>=0)memset(g_sc_replay[instance->movy_track],0,sizeof(g_sc_replay[0]));
    g_sc_dirty=1;hb_sc_resolve(instance);
    instance->motif.cancel=1;uint8_t motif_output[16][3];int motif_lengths[16];
    while(instance->motif.cancel)hb_mt_tick(instance,motif_output,motif_lengths,16);
    hb_cp_clear(&instance->player);
    uint8_t discarded[16][3];int lengths[16];
    while(instance->player.sounding_count)hb_player_tick(instance,discarded,lengths,16);
    hb_mo_panic(&instance->motion_render);hb_motion_flush_render(instance);
    hb_receiver_remove_source(instance);
    instance->used=0;
}for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used)return;memset(g_clip_cache,0,sizeof(g_clip_cache));memset(g_timelines,0,sizeof(g_timelines));memset(g_timeline_owners,0,sizeof(g_timeline_owners));g_clip_cache_key_valid=0;memset(&g_clip_cache_key,0,sizeof(g_clip_cache_key));g_motion_settings_ready=g_motion_settings_restored=g_quant_restored=0;g_bus.quant_timing=0;g_buffer_restored=0;g_bus.boundary_buffer_ms=-3;g_lookahead_restored=0;hb_touch_defaults();g_bus.next_lookahead=0;g_bus.next_anti_buffer_ms=25;hb_set_shared_follower_scale(1);g_scale_restored=0;hb_store_scale_exceptions(0,0);g_scale_exceptions_restored=0;memset(g_motifs,0,sizeof(g_motifs));g_motifs_restored=0;g_motif_rhythm=g_motif_span=g_motif_timing_restored=0;g_render_window=g_render_restored=0;g_role_ready=g_role_restored=0;if(g_global_shared)g_global_shared->role_ready=0;hb_pad_defaults();}
static int hb_source_channel_matches(Inst *instance,int midi_channel){
    if(!instance)return 0;
    /* Movy delivers each private chain on channel 1; Receive selects bus
       subscriptions and must never filter that already-isolated input. */
    if(instance->movy_track>=0)return midi_channel==0;
    if(instance->source_channel>=0)return midi_channel==instance->source_channel;
    /* Wait for authoritative ownership; do not read disk or accept unrelated
       broadcast notes while a conductor's source channel is unresolved. */
    int owning_channel=hb_monitor_channel(instance);
    if(owning_channel>=0&&owning_channel<16)return midi_channel==owning_channel;
    return instance->role==0?0:1;
}
static void hb_confirm_live_note(Inst *instance,int note){
    /* Legacy focus/pad pairing is diagnostic only. It is not event-preserving
       and therefore must never mutate held musical state. */
    (void)instance;(void)note;
}
static int hb_take_recent_unpaired_note(Inst *instance){
    if(!instance)return -1;
    int best=-1,best_age=0x7fffffff;
    for(int i=0;i<16;i++){
        if(instance->recent_live_valid[i]&&instance->recent_live_age[i]<best_age){
            best=i;best_age=instance->recent_live_age[i];
        }
    }
    if(best<0)return -1;
    int note=instance->recent_live_note[best];
    instance->recent_live_valid[best]=0;
    return note;
}
static void hb_offer_note_for_live_pairing(Inst *instance,int note){
    if(!instance||instance->role!=0)return;
    if(instance->live_vouch_pending>0){
        instance->live_vouch_pending--;
        instance->live_vouch_age=0;
        hb_confirm_live_note(instance,note);
        return;
    }
    int slot=-1,worst=-1,worst_age=-1;
    for(int i=0;i<16;i++){
        if(!instance->recent_live_valid[i]){slot=i;break;}
        if(instance->recent_live_age[i]>worst_age){worst_age=instance->recent_live_age[i];worst=i;}
    }
    if(slot<0)slot=worst;
    if(slot>=0){
        instance->recent_live_note[slot]=note;
        instance->recent_live_age[slot]=0;
        instance->recent_live_valid[slot]=1;
    }
}
static void hb_receive_live_vouch(Inst *instance){
    if(!instance||instance->role!=0)return;
    instance->live_press_count++;
    int note=hb_take_recent_unpaired_note(instance);
    if(note>=0)hb_confirm_live_note(instance,note);
    else if(instance->live_vouch_pending<16)instance->live_vouch_pending++;
}
static void hb_inject_note_off_to_render(Inst *instance,int pitch){
    if(!instance||pitch<0||pitch>127||instance->render_channel<0)return;
    uint8_t packet[4];
    packet[0]=0x28; /* cable 2 + note-off CIN */
    packet[1]=(uint8_t)(0x80 | (instance->render_channel & 0x0F));
    packet[2]=(uint8_t)(pitch & 0x7F);
    packet[3]=0;
    int sent=hb_send_render(instance,packet,0);
    if(sent==4){instance->render_count++;instance->render_last_note=pitch;}
    else instance->render_fail_count++;
}
static void hb_prepare_role_change_flush(Inst *instance){
    if(!instance)return;
    /* Preserve OFFs queued by an earlier routing change until the audio tick. */
    instance->role_flush_cursor=0;
    for(int source_note=0;source_note<128;source_note++){
        int local_pitch=-1;
        int render_pitch=-1;
        if(instance->role==0 && instance->passthrough_held[source_note]){
            local_pitch=instance->mapped[source_note];
            render_pitch=local_pitch;
        }else if(instance->role==0 && !hb_cp_enabled(&instance->player) &&
           (instance->held_count[source_note]>0 || instance->pending_off_frames[source_note]>0)){
            local_pitch=instance->mapped[source_note];
            if(local_pitch<0){
                local_pitch=source_note+g_bus.global_transpose;
                if(local_pitch<0)local_pitch=0;
                if(local_pitch>127)local_pitch=127;
            }
            /* Match the globally transposed conductor render note exactly. */
            render_pitch=local_pitch;
            if(render_pitch<0)render_pitch=0;
            if(render_pitch>127)render_pitch=127;
        }else if(instance->role==1 && instance->follower_sounding[source_note]){
            local_pitch=instance->mapped[source_note];
            if(local_pitch<0)local_pitch=source_note;
            render_pitch=local_pitch;
        }
        if(local_pitch>=0&&local_pitch<128)instance->role_flush_pending[local_pitch]=1;
        if(render_pitch>=0&&render_pitch<128)hb_inject_note_off_to_render(instance,render_pitch);
    }
}
static int hb_emit_role_change_flush(Inst *instance,uint8_t output[][3],int lengths[],int max_output){
    if(!instance||!output||!lengths||max_output<=0)return 0;
    int emitted=0;
    for(int pitch=instance->role_flush_cursor;pitch<128&&emitted<max_output;pitch++){
        instance->role_flush_cursor=pitch+1;
        if(!instance->role_flush_pending[pitch])continue;
        instance->role_flush_pending[pitch]=0;
        output[emitted][0]=0x80;
        output[emitted][1]=(uint8_t)pitch;
        output[emitted][2]=0;
        lengths[emitted]=3;
        emitted++;
    }
    if(instance->role_flush_cursor>=128)instance->role_flush_cursor=0;
    return emitted;
}
static void hb_clear_instance_note_state(Inst *instance){
    if(!instance)return;
    instance->approach_rows.down=instance->approach_rows.knob_down=instance->approach_rows.step_down=0;instance->approach_rows.performance=instance->approach_rows.latch=instance->approach_rows.motif_latch=0;instance->approach_rows.bank_armed=-1;memset(instance->approach_rows.swallow,0,sizeof(instance->approach_rows.swallow));
    instance->motif.cancel=1;instance->motif.editor.armed=-1;
    memset(instance->chord_pair_held,0,sizeof(instance->chord_pair_held));memset(instance->chord_pair_owner,0,sizeof(instance->chord_pair_owner));instance->chord_pair_render=0;
    instance->next_touch_mask=0;instance->target_attack_owner=0;
    instance->movy_pad_pending=0;memset(instance->movy_pad_shift,0,sizeof(instance->movy_pad_shift));
    instance->motion.held=0;instance->motion.pitch_held=0;instance->motion.enclosure=0;hb_mo_gesture_reset(&instance->motion);
    hb_mo_input_reset(&instance->motion);
    hb_mo_panic(&instance->motion_local);hb_mo_panic(&instance->motion_render);hb_motion_flush_render(instance);
    if(instance->role==3)hb_receiver_reset(instance);
    else hb_receiver_remove_source(instance);
    hb_cp_clear(&instance->player);
    memset(instance->passthrough_held,0,sizeof(instance->passthrough_held));
    memset(instance->held_now,0,sizeof(instance->held_now));memset(instance->held_count,0,sizeof(instance->held_count));
    memset(instance->active,0,sizeof(instance->active));
    memset(instance->pending_off_frames,0,sizeof(instance->pending_off_frames));
    memset(instance->follower_held,0,sizeof(instance->follower_held));
    memset(instance->follower_sounding,0,sizeof(instance->follower_sounding));
    memset(instance->follower_voices,0,sizeof(instance->follower_voices));
    memset(instance->follower_input_held,0,sizeof(instance->follower_input_held));
    memset(instance->follower_origin_delay,0,sizeof(instance->follower_origin_delay));
    memset(instance->follower_note_delay_beats,0,sizeof(instance->follower_note_delay_beats));
    memset(instance->follower_velocity,0,sizeof(instance->follower_velocity));instance->follower_queue_count=0;memset(instance->published_conductor,0,sizeof(instance->published_conductor));memset(instance->published_follower,0,sizeof(instance->published_follower));instance->settle_frames_remaining=0;instance->clip_event_idle_frames=0;
    for(int note=0;note<128;note++){
        instance->mapped[note]=-1;
        instance->follower_role_interval[note]=255;
    }
    instance->active_count=0;
    instance->last_inferred_count=0;
    instance->approach_pad_armed=HB_APPROACH_OFF;
    instance->approach_below_held=0;
    instance->approach_above_held=0;
    instance->candidate_frames=0;
    instance->dirty=1;
    instance->frames_since_change=0;
}
static void hb_stop_instance_note_state(Inst *instance){
    instance->player.release_held=instance->player.release_armed=instance->player.release_latched=0;
    instance->dominant_color_held=instance->dominant_color_latched=0;
    memset(instance->physical_velocity,0,sizeof(instance->physical_velocity));instance->physical_target=instance->target_attack_owner=0;instance->advance_pending=instance->advance_phase=0;
    hb_override_clear(instance);
    instance->key_pending=instance->key_schedule_arm=0;
    /* Clearing the maps first loses the actual sounding pitch for raw and
       baked notes. Preserve local OFFs and release the original render route
       before discarding those owners; CP/motion voices retain their own drain. */
    hb_prepare_role_change_flush(instance);
    hb_clear_instance_note_state(instance);
}
static void hb_publish_instance_notes(Inst *instance){
    if(!instance)return;
    if(instance->role==0){
        for(int note=0;note<128;note++)instance->published_conductor[note]=instance->held_count[note]>0?1:0;
        for(int event=0;event<HB_MT_SCHEDULE;event++)if(instance->motif.events[event].used&&instance->motif.events[event].started){int note=instance->motif.events[event].pitch-g_bus.global_transpose;if(note>=0&&note<128)instance->published_conductor[note]=1;}
        memset(instance->published_follower,0,sizeof(instance->published_follower));
    }else if(instance->role==1){
        memcpy(instance->published_follower,instance->follower_held,sizeof(instance->published_follower));
        memset(instance->published_conductor,0,sizeof(instance->published_conductor));
    }else{
        memset(instance->published_conductor,0,sizeof(instance->published_conductor));
        memset(instance->published_follower,0,sizeof(instance->published_follower));
    }
}
static int hb_instance_index(const Inst *instance){
    if(!instance)return -1;
    for(int index=0;index<HB_MAX_INSTANCES;index++)if(instance==&g_pool[index])return index;
    return -1;
}
static void hb_trace_note_event(Inst *instance,int note,int is_on,int channel){
    if(!instance||note<0||note>127||channel<0||channel>15)return;
    unsigned slot=instance->trace_count%8u;
    instance->trace_note[slot]=(uint8_t)note;
    instance->trace_on[slot]=(uint8_t)(is_on?1:0);
    instance->trace_channel[slot]=(uint8_t)channel;
    instance->trace_count++;
}
static int hb_trace_event_index(const Inst *instance,int ordinal){
    if(!instance||ordinal<0||ordinal>=8)return -1;
    unsigned available=instance->trace_count<8u?instance->trace_count:8u;
    if((unsigned)ordinal>=available)return -1;
    unsigned newest=(instance->trace_count-1u)%8u;
    return (int)((newest+8u-(unsigned)ordinal)%8u);
}
static int hb_format_trace_event(const Inst *instance,int ordinal,char *buffer,int length){
    if(!buffer||length<=0)return -1;
    int index=hb_trace_event_index(instance,ordinal);
    if(index<0)return snprintf(buffer,(size_t)length,"%s","--");
    char note_buf[8];
    hb_harmony_t harmony=bus_read();
    const char *note_name=hb_note_name_with_octave(instance->trace_note[index],harmony,note_buf,sizeof(note_buf));
    return snprintf(buffer,(size_t)length,"%s %s Ch%d",instance->trace_on[index]?"ON":"OFF",note_name,(int)instance->trace_channel[index]+1);
}
static int pass(const uint8_t *input,int length,uint8_t output[][3],int lengths[],int max_output){if(!input||length<1||length>3||max_output<1)return 0;memcpy(output[0],input,(size_t)length);lengths[0]=length;return 1;}
/* A pair is one chord owner per MIDI channel. No timer guesses whether a
   second note was intended: two overlapping notes explicitly complete it. */
static void hb_chord_pair_event(Inst *instance,int note,int channel,int velocity,int on){
    instance->chord_pair_held[instance->movy_playback!=0][channel][note]=on?(uint8_t)velocity:0;
    int lower=-1,upper=-1;
    for(int pitch=0;pitch<128;pitch++)if(instance->chord_pair_held[instance->movy_playback!=0][channel][pitch]){
        if(lower<0)lower=pitch;upper=pitch;
    }
    int previous=instance->chord_pair_owner[instance->movy_playback!=0][channel]-1;
    if(previous>=0){
        /* Pair ownership is physical, independent of the arp latch preference. */
        for(int index=0;index<HB_CP_KEYS;index++)if(instance->player.keys[index].used&&
            instance->player.keys[index].source==previous&&instance->player.keys[index].channel==channel&&
            instance->player.keys[index].playback_origin==(instance->movy_playback!=0))
            instance->player.keys[index].used=0;
        instance->chord_pair_owner[instance->movy_playback!=0][channel]=0;
    }
    if(lower>=0&&upper>lower){
        instance->chord_pair_render=1;instance->chord_pair_top=upper;
        hb_player_note_on(instance,lower,channel,instance->chord_pair_held[instance->movy_playback!=0][channel][upper]);
        instance->chord_pair_render=0;instance->chord_pair_owner[instance->movy_playback!=0][channel]=lower+1;
    }
    if(instance->role==0)hb_conductor_player_sense(instance);
}
static void hb_adopt_physical_targets(Inst *instance){
    if(!instance->adopt_held||!hb_cp_enabled(&instance->player))return;
    int saved_origin=instance->movy_playback;instance->movy_playback=0;
    unsigned short owner=instance->physical_target;
    if(!owner||!instance->physical_velocity[(owner-1)/128][(owner-1)%128])owner=0;
    for(int channel=0;channel<16;channel++)for(int note=0;note<128;note++){
        int velocity=instance->physical_velocity[channel][note];if(!velocity)continue;
        if(!owner)owner=(unsigned short)(channel*128+note+1);
        if(instance->chord_pair_input)hb_chord_pair_event(instance,note,channel,velocity,1);
        else hb_player_note_on(instance,note,channel,velocity);
        instance->approach_rows.swallow[channel][note]=0;instance->motif.editor.swallow[channel][note]=0;
        instance->active_count++;
    }
    instance->movy_playback=saved_origin;
    if(owner)for(int lane=0;lane<HB_MOTION_LANES;lane++){
        unsigned long long bit=1ULL<<lane;
        if(instance->motion.lanes[lane].operation==HB_MO_AUTO_CHORD_REPEAT&&
           ((instance->motion.gesture_once|instance->motion.gesture_down)&bit)&&!(instance->motion.gesture_persistent&bit))
            instance->motion.gesture_target_owner[lane]=owner;
    }
    if(instance->role==0)hb_conductor_player_sense(instance);
}
static void hb_chord_state_sync(Inst *instance){
    int selected=-1;unsigned serial=0;
    if(instance->role<2)for(int lane=0;lane<HB_MOTION_USER_LANES;lane++){
        const hb_motion_lane *settings=&instance->motion.lanes[lane];double amount;
        if(settings->operation!=HB_MO_CHORD_STATE||!settings->chord_state_valid||
            !hb_mo_value_at(&instance->motion,lane,hb_motion_position(instance),hb_motion_condition_position(),0,&amount))continue;
        unsigned order=instance->motion.gesture_serial[lane]>instance->motion.held_serial[lane]?instance->motion.gesture_serial[lane]:instance->motion.held_serial[lane];
        if(selected<0||order>=serial){selected=lane;serial=order;}
    }
    int active=selected>=0;
    int pair=active&&instance->motion.lanes[selected].chord_input;
    hb_cp_config config=active?instance->motion.lanes[selected].chord_state:instance->player.config;
    if(pair==instance->chord_pair_input&&active==instance->player.state_override&&(!active||!memcmp(&config,&instance->player.state_config,sizeof(config))))return;
    hb_motion_config motion=instance->motion;hb_ar_state approach=instance->approach_rows;
    hb_prepare_role_change_flush(instance);hb_clear_instance_note_state(instance);
    instance->motion=motion;instance->approach_rows=approach;instance->player.state_override=active;instance->chord_pair_input=pair;
    if(active)instance->player.state_config=config;
    hb_adopt_physical_targets(instance);
}
/* Repeat is a runtime overlay. Panel settings remain authoritative and serializable. */
static void hb_auto_chord_repeat_sync(Inst *instance){
    hb_chord_state_sync(instance);
    int active=0;
    if(instance->role<2)for(int lane=0;lane<HB_MOTION_LANES;lane++){
        double value;
        if(instance->motion.lanes[lane].operation==HB_MO_AUTO_CHORD_REPEAT&&
           hb_mo_value_at(&instance->motion,lane,hb_motion_position(instance),hb_motion_condition_position(),0,&value)){
            int amount=hb_mo_chord_arp_choice(instance->motion.lanes[lane].amount)%3;
            active|=amount==0?1:amount==2?2:3;
        }
    }
    active=active==3?1:active==1?2:active==2?3:0;
    if(active==instance->player.repeat_override)return;
    /* Mode boundaries release all old ownership before accepting new presses.
       Preserve the operation gesture that caused the boundary. */
    hb_motion_config motion=instance->motion;hb_ar_state approach=instance->approach_rows;
    hb_prepare_role_change_flush(instance);hb_clear_instance_note_state(instance);
    instance->motion=motion;instance->approach_rows=approach;instance->player.repeat_override=active;
    if(active)hb_adopt_physical_targets(instance);
}
static void hb_capture_input_intent(Inst *instance,int note,unsigned long long *actions){
    hb_mo_capture(&instance->motion,hb_motion_position(instance),hb_motion_condition_position(),note,actions);
    actions[HB_MOTION_LANES]|=hb_ar_alias_word(instance->movy_pad_shift[note]);
    if(instance->approach_rows.tokens[note]){
        unsigned token=instance->approach_rows.tokens[note];
        actions[HB_MOTION_LANES]=(actions[HB_MOTION_LANES]&~(HB_AR_MASK|0x573ULL))|((unsigned long long)token<<HB_AR_SHIFT)|hb_ar_intent(token);
    }
    if(instance->role==0){
        hb_cp_config captured=hb_chord_config_at(instance,note,hb_motion_position(instance),hb_motion_condition_position());
        int root=0;hb_resolve_follower_reference_root(instance,&root);
        actions[HB_MOTION_LANES]|=(1ULL<<20)|((unsigned long long)captured.mode<<21)|((unsigned long long)captured.quality<<23)|((unsigned long long)captured.size<<27)|((unsigned long long)root<<32)|((unsigned long long)hb_follower_input_scale_index(instance,root)<<36)|((unsigned long long)hb_policy_value(instance,HB_P_GAP)<<41);
    }
    /* Construction mode 3 is reserved for a prospective destination scale.
       This metadata affects only this recorded approach, never the live arm. */
    int destination_scale=hb_key_for(instance).approach_scale;
    if(destination_scale&&(actions[HB_MOTION_LANES]&(0x173ULL|HB_MO_CADENCE_MASK|HB_AR_MASK))){
        unsigned long long *intent=&actions[HB_MOTION_LANES];
        *intent=(*intent&~HB_MO_INTENT_MASK)|(1ULL<<20)|(3ULL<<21)|
            ((unsigned long long)(destination_scale<0?31:destination_scale)<<36);
    }
}
static int process_core(void *value,const uint8_t *input,int length,uint8_t output[][3],int lengths[],int max_output){Inst *instance=(Inst*)value;if(!instance||!input||length<1)return 0;hb_auto_chord_repeat_sync(instance);hb_movy_refresh();hb_next_touch_clear_expired(instance);g_bus.global_process_count++;g_bus.global_last_status=input[0];g_bus.global_last_instance=hb_instance_index(instance);
if(input[0]==0xFC){
    hb_stop_instance_note_state(instance);
    instance->resolved_source_channel=-1;
}
else if(input[0]==0xFA){
    g_bus.clip_clock_ticks=0;
}
else if(instance->role==0&&input[0]==0xF8){
    g_bus.clip_clock_ticks++;
}
instance->rx_count++;int status=input[0]&0xF0,is_on=(status==0x90&&length>=3&&input[2]>0),is_off=(status==0x80&&length>=3)||(status==0x90&&length>=3&&input[2]==0);if(is_on||is_off){g_bus.global_note_event_count++;g_bus.global_last_note=input[1]&0x7F;g_bus.global_last_channel=input[0]&0x0F;}
if(status==0xB0&&length>=3&&(input[1]==120||input[1]==123)){
    int control_channel=input[0]&0x0F;
    if(hb_source_channel_matches(instance,control_channel))hb_stop_instance_note_state(instance);
    return pass(input,length,output,lengths,max_output);
}
/* Pressure belongs to the original held input, before chord expansion.
   Update future arp attacks without restarting the clock or current gate. */
if(status==0xA0&&length>=3&&instance->role<2&&hb_cp_playback(&instance->player)==1&&
   hb_source_channel_matches(instance,input[0]&15)){
    int source=input[1]&127,channel=input[0]&15,velocity=input[2]&127;
    if(instance->chord_pair_input&&instance->chord_pair_held[instance->movy_playback!=0][channel][source]&&instance->chord_pair_owner[instance->movy_playback!=0][channel])
        source=instance->chord_pair_owner[instance->movy_playback!=0][channel]-1;
    if(instance->pressure_full_velocity)velocity=127;
    else if(velocity==0)velocity=1; /* Never turn a generated note-on into note-off. */
    int pending=-1;
    for(int index=0;index<instance->follower_queue_count;index++)
        if(instance->follower_queue_note[index]==source&&instance->follower_queue_channel[index]==channel&&instance->follower_queue_origin[index]==(instance->movy_playback!=0))
            pending=index;
    if(pending>=0){
        if(instance->follower_queue_on[pending])instance->follower_queue_velocity[pending]=velocity;
    }else for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *key=&instance->player.keys[index];
        if(key->used&&key->held&&key->source==source&&key->channel==channel&&key->playback_origin==(instance->movy_playback!=0))key->velocity=velocity;
    }
    return 0;
}
if(is_on&&!instance->movy_playback&&instance->movy_pad_pending==(input[1]&127)+1&&!hb_approach_pad_enabled(instance)){
    instance->movy_pad_pending=0;return 0;
}
if(instance->role==3)return (is_on||is_off)?0:pass(input,length,output,lengths,max_output);
if(!(is_on||is_off))return pass(input,length,output,lengths,max_output);int note=input[1]&0x7F,mapped;if(is_on&&!instance->movy_playback){instance->movy_input_degree[note]=0;instance->movy_input_target[note]=0;}if(is_on){
    unsigned long long marker=instance->movy_playback&&instance->recorded_action_valid[note]?instance->recorded_actions[note][HB_MOTION_LANES]:0;
    instance->approach_rows.tokens[note]=instance->movy_playback&&instance->recorded_action_valid[note]?(instance->recorded_actions[note][HB_MOTION_LANES]>>HB_AR_SHIFT)&2047:0;
    instance->movy_pad_shift[note]=hb_ar_alias_shift(marker);
    if(!instance->movy_playback&&instance->movy_pad_pending==note+1&&hb_approach_pad_enabled(instance))
        instance->movy_pad_shift[note]=instance->movy_pad_pending_shift;
    if(!instance->movy_playback&&(instance->approach_rows.enabled||instance->movy_pad_shift[note])){
        int row=instance->movy_pad_shift[note]?(instance->approach_rows.pending_row<0?3:instance->approach_rows.pending_row):-1;
        instance->approach_rows.tokens[note]=hb_ar_live_peek(&instance->approach_rows,row);if(instance->approach_rows.tokens[note])hb_ar_live_advance(&instance->approach_rows,row);
    }
    instance->movy_pad_pending=0;instance->approach_rows.pending_row=-1;
}int input_channel=input[0]&0x0F;if(instance->role==2)return pass(input,length,output,lengths,max_output);if(!hb_source_channel_matches(instance,input_channel))return pass(input,length,output,lengths,max_output);if(is_on){
    if(hb_cp_settings(&instance->player)->start==1||hb_cp_settings(&instance->player)->start==2||hb_cp_settings(&instance->player)->start==5)instance->player.anchor_pending=1;
    if(instance->movy_playback&&instance->recorded_action_valid[note])instance->motion.event_override=instance->recorded_actions[note];
    else if(instance->motif.editor.recording<0)hb_mo_input(&instance->motion,note,instance->motion_beat,hb_ms_to_beats(25));
    if(instance->approach_rows.tokens[note]&&!instance->movy_playback){
        unsigned token=instance->approach_rows.tokens[note];
        instance->approach_rows.saved_word=instance->motion.events[HB_MOTION_LANES];instance->approach_rows.restore_word=1;
        instance->motion.events[HB_MOTION_LANES]=(instance->motion.events[HB_MOTION_LANES]&~0x573ULL)|hb_ar_intent(token);
    }
    instance->motion.gesture_once_used|=instance->motion.gesture_once&~instance->next_touch_mask;
    if(!instance->synthetic_advance&&!instance->movy_playback&&instance->motif.editor.recording<0&&instance->role<2&&instance->action_count<64){
        int slot=(instance->action_head+instance->action_count)%64;
        hb_capture_input_intent(instance,note,instance->action_queue[slot]);
        instance->action_pitch[slot]=(uint8_t)note;instance->action_count++;
    }
}g_bus.global_accepted_note_count++;instance->last_status=input[0];instance->last_note=note;instance->last_velocity=length>=3?input[2]:0;hb_trace_note_event(instance,note,is_on,input_channel);if(is_on){instance->note_on_count++;instance->active_count++;}else if(is_off){instance->note_off_count++;if(instance->active_count>0)instance->active_count--;}if(instance->chord_pair_input&&hb_cp_enabled(&instance->player)&&!instance->movy_passthrough){
    hb_chord_pair_event(instance,note,input_channel,length>=3?input[2]:100,is_on);return 0;
}if(instance->role==0){
    hb_follower_voice *input_voice=0;
    if(instance->movy_passthrough||!hb_cp_enabled(&instance->player)){
        input_voice=hb_fv_find(instance->follower_voices,note,input_channel,instance->movy_playback!=0,is_on);
        if(!input_voice)return 0;
        if(is_on){
            *input_voice=(hb_follower_voice){.used=1,.source=note,.channel=input_channel,.origin=instance->movy_playback!=0,.velocity=input[2]};
            memcpy(input_voice->events,instance->motion.event_override?instance->motion.event_override:instance->motion.events,sizeof(input_voice->events));
        }else instance->mapped[note]=input_voice->pitch;
        instance->motion_event_owner=hb_fv_identity(note,input_channel,instance->movy_playback!=0);
        if(instance->motion_output_base){
            int offset=(int)(output-instance->motion_output_base);
            if(offset>=0&&offset<128){
                instance->motion_output_owner[offset]=instance->motion_event_owner;
                memcpy(instance->motion_output_events[offset],input_voice->events,sizeof(input_voice->events));
                instance->motion_output_valid[offset]=1;
            }
        }
    }
    if(instance->movy_passthrough){
        /* Saved voices bypass chord generation, not master transpose. */
        if(is_on){
            if(instance->passthrough_held[note]<255)instance->passthrough_held[note]++;
            mapped=hb_key_conductor_pitch(instance,note+g_bus.global_transpose);
            if(mapped<0)mapped=0;if(mapped>127)mapped=127;
            instance->mapped[note]=mapped;
        }else{
            if(!instance->passthrough_held[note])return 0;
            instance->passthrough_held[note]--;
            mapped=instance->mapped[note];
        }
        hb_conductor_player_sense(instance);
        if(instance->render_channel>=0)
            hb_render_conductor_event(instance,note,length>=3?input[2]:0,is_on,is_off,input_channel);
        if(is_off&&!instance->passthrough_held[note])instance->mapped[note]=-1;
        input_voice->pitch=mapped;input_voice->used=is_on;instance->motion_event_owner=0;
        if(max_output<1)return 0;
        if(is_on)hb_trail_output(instance,output[0],hb_trail_target(instance,note,mapped,input[2]));
        output[0][0]=input[0];output[0][1]=(uint8_t)mapped;output[0][2]=input[2];lengths[0]=3;
        return 1;
    }
    if(hb_cp_enabled(&instance->player)){
        if(is_on)hb_player_note_on(instance,note,input_channel,length>=3?input[2]:100);
        else hb_cp_off_origin(&instance->player,note,input_channel,instance->movy_playback!=0);
        hb_conductor_player_sense(instance);
        return 0;
    }
    if(instance->render_channel>=0)
        hb_render_conductor_event(instance,note,length>=3?input[2]:0,is_on,is_off,input_channel);
    if(is_on){
        instance->conductor_note_on_pending=1;
        if(instance->held_count[note]<255)instance->held_count[note]++;
        instance->held_now[note]=instance->held_count[note]>0;
        instance->pending_off_frames[note]=0;
        mapped=hb_key_conductor_pitch(instance,note+g_bus.global_transpose);if(mapped<0)mapped=0;if(mapped>127)mapped=127;
        instance->mapped[note]=mapped;
        /* Only note-on evidence may request a new harmony classification.
           Note-offs still update the live conductor-note panels, but releasing
           an Abm voicing can no longer reclassify its shrinking Cb/Eb subset. */
        instance->candidate_frames=0;
        instance->dirty=1;
        instance->frames_since_change=0;
    }else{
        if(instance->held_count[note]>0)instance->held_count[note]--;
        instance->held_now[note]=instance->held_count[note]>0;
        instance->pending_off_frames[note]=(g_bus.analysis_release_ms>0)?1:0;
        mapped=instance->mapped[note];
        if(mapped<0)mapped=hb_key_conductor_pitch(instance,note+g_bus.global_transpose);
        if(mapped<0)mapped=0;if(mapped>127)mapped=127;
        if(instance->held_count[note]==0)instance->mapped[note]=-1;
    }
    input_voice->pitch=mapped;input_voice->used=is_on;instance->motion_event_owner=0;
    hb_publish_instance_notes(instance);
    if(max_output<1)return 0;
    if(is_on)hb_trail_output(instance,output[0],hb_trail_target(instance,note,mapped,input[2]));
    output[0][0]=input[0];output[0][1]=(uint8_t)mapped;output[0][2]=length>=3?input[2]:0;lengths[0]=3;
    return 1;
}
/* The auxiliary monitor is only a pre-input observation fallback. Once the
   instance receives its own MIDI, that event stream owns the held-note set.
   Mixing a monitor snapshot into it can invent silent notes or resurrect OFFs. */
if(!instance->follower_input_seen){
    instance->follower_input_seen=1;
    memset(instance->follower_held,0,sizeof(instance->follower_held));
    memset(instance->follower_velocity,0,sizeof(instance->follower_velocity));
}
if(is_on){
    instance->follower_input_held[instance->movy_playback!=0][note]=1;
    instance->follower_held[note]=1;
    instance->follower_velocity[note]=(uint8_t)(length>=3?input[2]:100);
    int root=0;
    instance->follower_role_interval[note]=hb_resolve_follower_reference_root(instance,&root)
        ?(uint8_t)mod12(note-root):255;
}else{
    instance->follower_input_held[instance->movy_playback!=0][note]=0;
    instance->follower_held[note]=instance->follower_input_held[0][note]||instance->follower_input_held[1][note];
    if(!instance->follower_held[note])instance->follower_velocity[note]=0;
    instance->follower_role_interval[note]=255;
}
hb_publish_instance_notes(instance);
/* Defer every follower note event to tick(). Even with Follow Lookahead=0 this
   creates a one-tick micro-batch barrier, so conductor events delivered in the
   same scheduler quantum can commit harmony before follower pitch mapping. */
if(!hb_queue_follower_event(instance,note,length>=3?input[2]:0,is_on,input_channel)){
    /* Queue overflow is safer as transparent pass-through than a stuck note. */
    return pass(input,length,output,lengths,max_output);
}
return 0;}
static int hb_tick_conductor(Inst *instance,int frames,int sample_rate){
    int release_expired=0;
    int physical_held=0;
    for(int note=0;note<128;note++){
        if(instance->held_count[note]>0)physical_held++;
        if(instance->pending_off_frames[note]>0){
            int release_frames=(g_bus.analysis_release_ms*sample_rate)/1000;
            instance->pending_off_frames[note]+=frames;
            if(release_frames<=0||instance->pending_off_frames[note]>=release_frames){
                instance->pending_off_frames[note]=0;
                release_expired=1;
            }
        }
    }
    /* Released notes are not evidence for a newly played voicing. The prior
       committed harmony remains latched while an incomplete new gesture is
       grouped. Release-only changes retain their normal analysis grace. */
    if(instance->conductor_note_on_pending){
        for(int note=0;note<128;note++)if(!instance->held_count[note])instance->pending_off_frames[note]=0;
        instance->conductor_note_on_pending=0;
    }
    /* A release tail expiring can intentionally change a still-held voicing
       (e.g. C7 -> C after Bb is released), but when all physical keys are up
       the committed harmony remains latched rather than collapsing through
       transient subsets. */
    if(release_expired&&physical_held>0){
        instance->candidate_frames=0;
        instance->dirty=1;
        instance->frames_since_change=0;
    }

    int live_pair_window=(sample_rate>0)?(sample_rate*120/1000):5292;
    if(instance->live_vouch_pending>0){
        instance->live_vouch_age+=frames;
        if(instance->live_vouch_age>live_pair_window){
            instance->live_vouch_pending=0;
            instance->live_vouch_age=0;
        }
    }
    for(int i=0;i<16;i++){
        if(!instance->recent_live_valid[i])continue;
        instance->recent_live_age[i]+=frames;
        if(instance->recent_live_age[i]>live_pair_window)instance->recent_live_valid[i]=0;
    }

    if(g_movy_present||(g_host&&(g_host->get_clock_status||g_host->get_beat_position))){
        int clock_status=hb_clock_status();
        if(clock_status==MOVE_CLOCK_STATUS_RUNNING){
            double playhead=hb_clip_playhead();
            g_bus.last_clip_playhead=playhead;
            g_bus.have_last_clip_playhead=1;
            if(instance->role==0)hb_next_update_playhead(frames,sample_rate);
        }else{
            g_bus.next_have_playhead=0;g_bus.next_shift_active=0;
        }
        g_bus.last_clock_status=clock_status;
    }

    if(instance->role==0){
        uint8_t observed_notes[64];
        int observed_count=local_conductor_notes(instance,observed_notes,64);
        int sense_changed=(observed_count!=instance->local_sense_count);
        if(!sense_changed){
            for(int i=0;i<observed_count;i++){
                if(instance->local_sense_notes[i]!=observed_notes[i]){sense_changed=1;break;}
            }
        }
        /* active[] is classifier scratch only. Rebuild it from the authoritative
           per-instance held_count[] aggregate every tick so it can never retain ghosts. */
        memset(instance->active,0,sizeof(instance->active));
        for(int i=0;i<observed_count;i++)instance->active[observed_notes[i]]=1;
        if(sense_changed){
            int has_addition=0;
            for(int i=0;i<observed_count&&!has_addition;i++){
                int found=0;
                for(int j=0;j<instance->local_sense_count;j++){
                    if(observed_notes[i]==instance->local_sense_notes[j]){found=1;break;}
                }
                if(!found)has_addition=1;
            }
            instance->local_sense_count=observed_count;
            memset(instance->local_sense_notes,0,sizeof(instance->local_sense_notes));
            for(int i=0;i<observed_count;i++)instance->local_sense_notes[i]=observed_notes[i];
            g_bus.sense_rev++;
            if(has_addition){
                instance->candidate_frames=0;
                instance->dirty=1;
                instance->frames_since_change=0;
            }
        }
    }

    /* Dwell time must advance continuously, not only on MIDI changes. */
    instance->committed_frames+=frames;

    /* A stable pending candidate keeps accumulating confirmation time even
     * after the note-set has stopped changing. */
    if(!instance->dirty && instance->candidate_frames>0 && instance->candidate_harmony.valid){
        instance->candidate_frames+=frames;
        int min_dwell=hb_timescale_frames(sample_rate);
        int confirm=hb_confirm_frames(sample_rate);
        if(instance->candidate_frames>=confirm && instance->committed_frames>=min_dwell){
            hb_timeline_observe(instance,instance->candidate_harmony);
            hb_commit_observed_harmony(instance->candidate_harmony);
            instance->committed_frames=0;
            instance->candidate_frames=0;
        }
        return 0;
    }

    if(!instance->dirty)return 0;

    instance->frames_since_change+=frames;
    int needed=(g_bus.inference_window_ms*sample_rate)/1000;
    uint8_t notes[128];
    int count=local_conductor_harmony_notes(instance,notes);
    /* A complete quantized/live chord should not pay the generic grouping
       window. With Free timing + Live context, three or more simultaneous
       conductor notes are authoritative in this block; the follower queue's
       one-tick barrier then guarantees they win over same-time follower notes. */
    /* The explicit Movy phase follows the entire sequencer MIDI batch.
       Recognized dyads are complete evidence there, not an unfinished live
       gesture. Do not let grouping/confirmation retain the previous rendering
       at a follower's already-due boundary. Lone pitches still cannot commit. */
    int complete_batch=(g_conductor_block_ready&&count>=2);
    int complete_live_free=complete_batch||(g_bus.context==0&&count>=3);
    if(!complete_live_free&&instance->frames_since_change<needed)return 0;

    instance->last_inferred_count=count;
    instance->dirty=0;

    if(count<=0){
        memset(&instance->candidate_harmony,0,sizeof(instance->candidate_harmony));
        instance->candidate_frames=0;
        return 0;
    }

    hb_harmony_t committed=hb_observed_read();
    hb_harmony_t committed_sensor=hb_transpose_harmony(committed,-g_bus.global_transpose);
    /* Generated chord gestures are complete evidence. Reusing the previous
       root turns F -> Dm into F -> F6, and G -> Em into G -> G6, because
       the previous root and third are shared. Raw performance input retains
       contextual interpretation for partial/rolling voicings. */
    hb_harmony_t candidate=hb_cp_mode(&instance->player)!=0
        ?hb_infer_harmony(notes,count)
        :hb_infer_harmony_contextual(notes,count,committed_sensor);
    /* A generated gesture supplies its root explicitly, even when the
       sounding voicing omits it or resembles an inversion of another chord. */
    int generated_root=-1,multiple_roots=0;
    for(int index=0;index<HB_CP_KEYS;index++){
        const hb_cp_key *key=&instance->player.keys[index];
        if(!key->used||!key->semantic_mask)continue;
        int root=mod12(key->root_pc-g_bus.global_transpose);
        if(generated_root>=0&&generated_root!=root)multiple_roots=1;
        generated_root=root;
    }
    for(int index=0;index<HB_MT_SCHEDULE;index++){
        const hb_mt_scheduled *event=&instance->motif.events[index];
        if(!event->used||!event->started||!event->semantic_mask)continue;
        int root=mod12(event->root_pc-g_bus.global_transpose);
        if(generated_root>=0&&generated_root!=root)multiple_roots=1;
        generated_root=root;
    }
    if(generated_root>=0&&!multiple_roots){
        hb_harmony_t rooted=candidate;rooted.valid=1;rooted.root_pc=generated_root;
        candidate=hb_refine_harmony_with_root(notes,count,rooted);
        unsigned semantic=0;
        for(int note=0;note<count;note++)semantic|=1u<<mod12(notes[note]);
        if(hb_harmony_chord_mask(candidate)!=semantic){
            /* Keep exact generated alterations/extensions, even when no short
               template name exists. The suffix '+' denotes these added colors. */
            unsigned relative=0;
            for(int pitch=0;pitch<12;pitch++)if(semantic&(1u<<mod12(generated_root+pitch)))relative|=1u<<pitch;
            int quality=(relative&8)?((relative&64)?5:1):0;
            if(relative&1024)quality=(relative&8)?((relative&64)?13:11):10;
            else if(relative&2048)quality=(relative&8)?12:9;
            candidate.chord_index=quality|HB_HARMONY_EXPLICIT_TONES;
            candidate.pitch_mask=(uint16_t)semantic;
        }
        uint8_t sounding_notes[128];
        if(local_conductor_notes(instance,sounding_notes,128))candidate.bass_pc=mod12(sounding_notes[0]);
        hb_format_harmony(candidate.name,sizeof(candidate.name),candidate);
    }
    if(generated_root>=0&&!multiple_roots){
        int have=0,conflict=0;
        for(int index=0;index<HB_CP_KEYS;index++){
            const hb_cp_key *key=&instance->player.keys[index];if(!key->used)continue;
            if(!key->intent_kind){conflict=1;continue;}
            if(have&&(candidate.intent_kind!=key->intent_kind||candidate.intent_target!=mod12(key->intent_target-g_bus.global_transpose)))conflict=1;
            candidate.intent_kind=key->intent_kind;candidate.intent_target=mod12(key->intent_target-g_bus.global_transpose);candidate.intent_minor=key->intent_minor;candidate.intent_scale=hb_transpose_mask(key->intent_scale,-g_bus.global_transpose);have=1;
        }
        for(int index=0;index<HB_MT_SCHEDULE;index++){
            const hb_mt_scheduled *event=&instance->motif.events[index];
            if(!event->used||!event->started||!event->semantic_mask)continue;
            if(!event->intent_kind){conflict=1;continue;}
            int target=mod12(event->intent_target-g_bus.global_transpose);
            if(have&&(candidate.intent_kind!=event->intent_kind||candidate.intent_target!=target))conflict=1;
            candidate.intent_kind=event->intent_kind;candidate.intent_target=target;candidate.intent_minor=event->intent_minor;
            candidate.intent_scale=hb_transpose_mask(event->intent_scale,-g_bus.global_transpose);have=1;
        }
        if(conflict)candidate.intent_kind=0;
    }
    /* Generated semantic evidence retains implied identity tones, while the
       detected form follows the complete voiced chord (also during arps). */
    if(generated_root>=0){
        unsigned voiced=0;
        uint8_t sounding[128];int sounded=local_conductor_notes(instance,sounding,128);
        for(int n=0;n<sounded;n++)voiced|=1u<<mod12(sounding[n]);
        for(int k=0;k<HB_CP_KEYS;k++){
            const hb_cp_key *key=&instance->player.keys[k];if(!key->used)continue;
            for(int n=0;n<key->count;n++)voiced|=1u<<mod12(instance->key_reference[k][n]-g_bus.global_transpose);
        }
        for(int index=0;index<HB_MT_SCHEDULE;index++){
            const hb_mt_scheduled *event=&instance->motif.events[index];
            if(event->used&&event->started)voiced|=1u<<mod12(event->reference_pitch-g_bus.global_transpose);
        }
        candidate.detected_mask=(uint16_t)voiced;
    }
    candidate=hb_transpose_harmony(candidate,g_bus.global_transpose);

    /* Candidate display distinguishes a lone pitch from a major triad: F?
       means "only F is currently evidenced", while committed Harmony remains
       stable until enough structural evidence arrives. */
    if(count==1){
        hb_harmony_t unresolved;memset(&unresolved,0,sizeof(unresolved));
        unresolved.valid=1;
        unresolved.root_pc=mod12(notes[0]+g_bus.global_transpose);
        unresolved.bass_pc=unresolved.root_pc;
        unresolved.pitch_mask=(uint16_t)(1u<<unresolved.root_pc);
        unresolved.chord_index=-1;
        unresolved.confidence=25;
        instance->candidate_harmony=unresolved;
        return 0;
    }

    instance->candidate_harmony=candidate;

    if(!candidate.valid)return 0;

    if(!committed.valid){
        hb_timeline_observe(instance,candidate);
        hb_commit_observed_harmony(candidate);
        instance->committed_frames=0;
        instance->candidate_frames=0;
        return 0;
    }

    if(hb_same_harmony(candidate,committed)){
        if(hb_harmony_detected_mask(candidate)!=hb_harmony_detected_mask(committed))
            hb_commit_observed_harmony(candidate);
        hb_timeline_observe(instance,candidate);
        instance->candidate_frames=0;
        return 0;
    }

    /* Be deliberately insensitive to nested subset/superset voicing changes,
       but promote a complete structurally incompatible harmony immediately.
       Example: Fmaj -> F5 holds Fmaj; Fmaj -> Fmin/Fdim changes now. */
    if(count>=3&&hb_character_change(candidate,committed)){
        hb_timeline_observe(instance,candidate);
        hb_commit_observed_harmony(candidate);
        instance->committed_frames=0;
        instance->candidate_frames=0;
        return 0;
    }

    /* Free timing means a complete current voicing is authoritative now.
       Quantized/anticipated timing retains confirmation/dwell behavior. */
    if(complete_batch||(g_bus.context==0 && count>=3)){
        hb_timeline_observe(instance,candidate);
        hb_commit_observed_harmony(candidate);
        instance->committed_frames=0;
        instance->candidate_frames=0;
        return 0;
    }

    if(candidate.valid){
        int min_dwell=hb_timescale_frames(sample_rate);
        int overwhelming=(candidate.confidence>=90&&count>=3);
        if(overwhelming && instance->committed_frames>=min_dwell/2){
            hb_timeline_observe(instance,candidate);
        hb_commit_observed_harmony(candidate);
            instance->committed_frames=0;
            instance->candidate_frames=0;
        }else{
            instance->candidate_frames=frames>0?frames:1;
        }
    }
    return 0;
}

static void hb_prepare_conductors(int frames,int sample_rate){
    hb_override_commit();
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *conductor=&g_pool[index];
        if(!conductor->used||conductor->role!=0)continue;
        hb_sync_conductor_from_monitor(conductor);
        if(conductor->last_transport_playing&&hb_clock_status()!=MOVE_CLOCK_STATUS_RUNNING)
            hb_stop_instance_note_state(conductor);
        conductor->last_transport_playing=hb_clock_status()==MOVE_CLOCK_STATUS_RUNNING;
        hb_tick_conductor(conductor,frames,sample_rate);
    }
    /* The clip snapshot for this block arrived before its MIDI. Promote a
       complete learned traversal only now, after the wrap chord was visible. */
    hb_timeline_finalize();
}

static int hb_player_tick(Inst *instance,uint8_t output[][3],int lengths[],int max_output){
    hb_chord_player *player=&instance->player;
    hb_harmony_t anchor_harmony=hb_render_harmony(instance);
    unsigned anchor_mask=hb_harmony_chord_mask(anchor_harmony);
    if(anchor_harmony.valid){
        if((hb_cp_settings(player)->start==3||hb_cp_settings(player)->start==4||hb_cp_settings(player)->start==6)&&player->anchor_harmony_valid&&
           (anchor_mask!=player->anchor_harmony_mask||anchor_harmony.root_pc!=player->anchor_harmony_root))player->anchor_pending=1;
        player->anchor_harmony_valid=1;player->anchor_harmony_mask=anchor_mask;player->anchor_harmony_root=anchor_harmony.root_pc;
    }
    for(int owner=0;owner<HB_CP_KEYS;owner++){
        hb_cp_key *key=&player->keys[owner];
        int applies=!instance->play.bypass&&(instance->play.scope==0||
            (instance->play.scope==1?key->playback_origin:!key->playback_origin));
        key->range=applies?instance->play.range+1:1;
    }
    int emitted=hb_cp_tick(player,output,lengths,max_output);
    for(int index=0;index<emitted;index++){
        int on=(output[index][0]&0xF0)==0x90;
        int rhythm_origin=instance->movy_playback;
        const unsigned long long *saved_events=instance->motion.event_override;
        if(on)for(int owner=0;owner<HB_CP_KEYS;owner++){
            hb_cp_key *key=&player->keys[owner];if(!key->used||key->channel!=(output[index][0]&15))continue;
            int matches=0;
            for(int voice=0;voice<key->count;voice++){
                int offset=output[index][1]-key->notes[voice];
                if(offset>=0&&offset%12==0&&offset<12*(hb_cp_playback(player)==1?key->range:1))matches=1;
            }
            if(!matches)continue;
            instance->motion.event_override=instance->motion_player_events[owner];instance->movy_playback=key->playback_origin;
            {unsigned saved_serial=instance->trail_event_serial;instance->trail_event_serial=key->trail_serial;
            hb_trail_output(instance,output[index],hb_trail_target(instance,key->source,key->played_pitch,key->velocity));instance->trail_event_serial=saved_serial;}
            if(instance->motion_output_base){
                long offset=output[index]-instance->motion_output_base[0];offset/=3;
                if(offset>=0&&offset<128){
                    memcpy(instance->motion_output_events[offset],instance->motion_player_events[owner],sizeof(instance->motion.events));
                    instance->motion_output_valid[offset]=1;instance->rhythm_output_prewarped[offset]=key->playback_origin;
                }
            }
            break;
        }

        if(instance->role==0&&instance->movy_track>=0&&instance->movy_track<16){
            int channel=output[index][0]&15,pitch=output[index][1];
            int recordable=0;
            if(on)for(int owner=0;owner<HB_CP_KEYS;owner++){
                const hb_cp_key *key=&player->keys[owner];
                if(!key->used||!key->recordable||key->channel!=channel)continue;
                for(int voice=0;voice<key->count;voice++){
                    int offset=pitch-key->notes[voice];
                    int range=hb_cp_playback(player)==1?key->range:1;
                    if(offset>=0&&offset%12==0&&offset<12*range)recordable=1;
                }
            }
            else recordable=instance->recorded_sounding[channel][pitch];
            if(recordable&&g_host&&g_host->midi_inject_to_move){
                /* Private cable 3 is consumed by Movy's recording bridge;
                   it never enters the destination MIDI routing. */
                /* Store reference-key voices; playback applies the current master.
                   OFF keeps the ON's reference pitch even if transpose changed. */
                int reference=on?pitch-g_bus.global_transpose:instance->recorded_source_pitch[channel][pitch];
                while(reference<0)reference+=12;while(reference>127)reference-=12;
                if(on)instance->recorded_source_pitch[channel][pitch]=(uint8_t)reference;
                uint8_t packet[4]={(uint8_t)(on?0x39:0x38),
                    (uint8_t)((on?0x90:0x80)|instance->movy_track),(uint8_t)reference,output[index][2]};
                if(g_host->midi_inject_to_move(packet,4)==4)
                    instance->recorded_sounding[channel][pitch]=(uint8_t)on;
            }
        }
        if(player->render_channel<0){instance->motion.event_override=saved_events;instance->movy_playback=rhythm_origin;continue;}
        uint8_t packet[4]={(uint8_t)(on?0x29:0x28),(uint8_t)((on?0x90:0x80)|player->render_channel),output[index][1],output[index][2]};
        if(hb_send_render(instance,packet,player->render_channel==(output[index][0]&15))==4){instance->render_count++;instance->render_last_note=output[index][1];}
        else instance->render_fail_count++;
        instance->motion.event_override=saved_events;instance->movy_playback=rhythm_origin;
    }
    return emitted;
}
static int tick_core(void *value,int frames,int sample_rate,uint8_t output[][3],int lengths[],int max_output){
    Inst *instance=(Inst*)value;
    if(!instance)return 0;
    hb_movy_refresh();
    hb_next_touch_clear_expired(instance);
    g_bus.global_tick_count++;
    /* set_param cannot return local MIDI, so role-change OFFs are drained on
       the next audio tick before the new role can synthesize anything. */
    int role_flush_emitted=hb_emit_role_change_flush(instance,output,lengths,max_output);
    if(role_flush_emitted>0)return role_flush_emitted;
    if(instance->role==1)hb_sync_from_monitor(instance);
    else if(instance->role==0)hb_sync_conductor_from_monitor(instance);
    if(g_movy_present||(g_host&&(g_host->get_clock_status||g_host->get_beat_position))){
        int clock_status=hb_clock_status();
        int is_playing=(clock_status==MOVE_CLOCK_STATUS_RUNNING);
        if(instance->last_transport_playing&&!is_playing){
            /* Pause/stop is a safe one-way boundary: bias toward extra note-offs
               by clearing all held state rather than allowing stuck notes. */
            hb_stop_instance_note_state(instance);
        }
        instance->last_transport_playing=is_playing;
    }
    if(instance->role==3){
        if(instance->player.flushing){
            int emitted=hb_player_tick(instance,output,lengths,max_output);
            if(emitted||instance->player.flushing)return emitted;
        }
        int flushed=hb_emit_role_change_flush(instance,output,lengths,max_output);
        return flushed?flushed:hb_receiver_tick(instance,output,lengths,max_output);
    }
    hb_chord_player *player=&instance->player;
    double elapsed=sample_rate>0?(double)frames/sample_rate:0.0;
    player->seconds+=elapsed;
    double previous_beat=player->beat;
    double tempo=(g_host&&g_host->get_bpm)?g_host->get_bpm():120.0;
    if(tempo<=0.0)tempo=120.0;
    player->release_beats+=elapsed*tempo/60.0;
    player->beat=hb_clock_status()==MOVE_CLOCK_STATUS_RUNNING?hb_current_beat():player->beat+elapsed*tempo/60.0;
    if(player->beat+1e-6<previous_beat)hb_cp_clear(player);
    if(player->flushing){
        int emitted=hb_player_tick(instance,output,lengths,max_output);
        if(emitted||player->flushing)return emitted;
    }
    if(instance->role==1){
        if(hb_cp_enabled(&instance->player))player->render_channel=instance->render_channel;
        int emitted=hb_release_follower_queue(instance,frames,sample_rate,output,lengths,max_output);
        if(hb_cp_enabled(&instance->player)){
            hb_motion_harmony_refresh(instance);
            hb_reharmonize_held_chords(instance);
            return emitted+hb_player_tick(instance,output+emitted,lengths+emitted,max_output-emitted);
        }
        if(emitted>0)return emitted;
        /* The follower queue is the serialization boundary between physical
           input state and sounding output state. Never synthesize retriggers
           while an ON/OFF is still buffered: doing so can create an OFF for a
           note that has not sounded yet, duplicate an ON, or orphan a later
           note-off. */
        if(instance->follower_queue_count>0)return 0;
        hb_motion_harmony_refresh(instance);
        return hb_reharmonize_held_follower(instance,output,lengths,max_output);
    }
    if(instance->role!=0)return 0;
    if(!g_conductor_block_ready)hb_tick_conductor(instance,frames,sample_rate);
    if(hb_cp_enabled(&instance->player)){
        player->render_channel=instance->render_channel;
        return hb_player_tick(instance,output,lengths,max_output);
    }
    return 0;
}

static void hb_motion_tick_routes(Inst *instance){
    hb_rr_prepare(instance,&instance->motion_local);hb_rr_prepare(instance,&instance->motion_render);
    if(instance->last_transport_playing&&hb_clock_status()!=MOVE_CLOCK_STATUS_RUNNING){
        hb_stop_instance_note_state(instance);instance->last_transport_playing=0;
    }
    double beat=hb_motion_position(instance);
    hb_mo_due(&instance->motion_local,beat);hb_mo_due(&instance->motion_render,beat);
    hb_mo_repeat_tick(&instance->motion_local,&instance->motion,instance->motion_beat);
    hb_mo_repeat_tick(&instance->motion_render,&instance->motion,instance->motion_beat);
    int pan_active=0;for(int lane=0;lane<HB_MOTION_USER_LANES;lane++)if(hb_mo_lane_active(&instance->motion,lane)&&hb_mo_condition(&instance->motion,lane,hb_motion_condition_position())&&instance->motion.lanes[lane].operation==HB_MO_PAN)pan_active=1;
    int pan_dirty=0;for(int channel=0;channel<16;channel++)pan_dirty|=instance->motion_local.pan_dirty[channel]|instance->motion_render.pan_dirty[channel];
    if(!pan_active&&pan_dirty){
        for(int pitch=0;pitch<128&&!pan_active;pitch++)if(instance->follower_sounding[pitch])
            for(int lane=0;lane<HB_MOTION_LANES;lane++){
                unsigned long long word=instance->motion_held_events[pitch][lane];
                if((word&HB_MO_RECORDED)&&((word>>32)&31)==HB_MO_PAN)pan_active=1;
            }
        for(int owner=0;owner<HB_CP_KEYS&&!pan_active;owner++)if(instance->player.keys[owner].used)
            for(int lane=0;lane<HB_MOTION_LANES;lane++){
                unsigned long long word=instance->motion_player_events[owner][lane];
                if((word&HB_MO_RECORDED)&&((word>>32)&31)==HB_MO_PAN)pan_active=1;
            }
    }
    if(!pan_active)for(int channel=0;channel<16;channel++){
        hb_motion_route *routes[2]={&instance->motion_local,&instance->motion_render};
        for(int index=0;index<2;index++)if(routes[index]->pan_dirty[channel]&&hb_mo_push(routes[index],0xb0|channel,10,routes[index]->base_pan[channel]))routes[index]->pan_dirty[channel]=0;
    }
    hb_motion_flush_render(instance);
}
static int hb_motion_local_drain(Inst *instance,uint8_t output[][3],int lengths[],int capacity){
    int emitted=0;while(emitted<capacity&&hb_mo_pop(&instance->motion_local,output[emitted])){hb_trail_heard(instance,instance->motion_local.trail_out);lengths[emitted++]=3;}return emitted;
}
static int process_with_actions(void *value,const uint8_t *input,int length,uint8_t output[][3],int lengths[],int capacity){
    Inst *instance=(Inst*)value;if(!instance)return 0;
    if(length!=3||instance->role>=2)return process_core(value,input,length,output,lengths,capacity);
    uint8_t pending[128][3];int sizes[128];
    instance->motion_output_base=pending;memset(instance->motion_output_owner,0,sizeof(instance->motion_output_owner));memset(instance->trail_outputs,0,sizeof(instance->trail_outputs));memset(instance->motion_output_valid,0,sizeof(instance->motion_output_valid));memset(instance->rhythm_output_prewarped,instance->movy_playback!=0,sizeof(instance->rhythm_output_prewarped));
    int count=process_core(value,input,length,pending,sizes,128);
    instance->motion_output_base=0;
    for(int index=0;index<count;index++){
        if(sizes[index]==3){
            instance->motion.event_override=instance->motion_output_valid[index]?instance->motion_output_events[index]:0;
            int origin=instance->movy_playback;instance->movy_playback=instance->rhythm_output_prewarped[index];
            instance->motion_event_owner=instance->motion_output_owner[index];instance->motion_local.trail_in=instance->trail_outputs[index];hb_motion_output(instance,&instance->motion_local,pending[index]);instance->motion_local.trail_in=0;instance->motion_event_owner=0;instance->movy_playback=origin;instance->motion.event_override=0;
        }
        else if(capacity>0){memcpy(output[0],pending[index],(size_t)sizes[index]);lengths[0]=sizes[index];return 1;}
    }
    return hb_motion_local_drain(instance,output,lengths,capacity);
}
/* Chord + Arp one-shots belong to the first physical target after arming.
   Generated notes, clip playback and spatial approaches cannot consume them. */
static void hb_gesture_target_input(Inst *instance,const uint8_t *input,int length){
    if(length<3||instance->role>=2||instance->movy_playback||instance->synthetic_advance)return;
    int type=input[0]&0xf0,source=input[1]&127,channel=input[0]&15;
    int on=type==0x90&&input[2],off=type==0x80||(type==0x90&&!input[2]);
    if((!on&&!off)||!hb_source_channel_matches(instance,channel))return;
    if(on&&instance->player.release_ms!=0&&instance->motion.gesture_target_released){
        /* The resolution was released: any new physical hit interrupts its
           finishing tail and starts a fresh gesture under restored settings. */
        int pending=instance->movy_pad_pending,shift=instance->movy_pad_pending_shift,row=instance->approach_rows.pending_row;
        hb_mo_end_lanes(&instance->motion,instance->motion.gesture_target_released);
        hb_auto_chord_repeat_sync(instance);
        instance->movy_pad_pending=pending;instance->movy_pad_pending_shift=shift;instance->approach_rows.pending_row=row;
    }
    if(on&&(instance->motif.editor.recording>=0||instance->movy_pad_pending==source+1))return;
    unsigned owner=(unsigned)(channel*128+source+1);
    unsigned long long once=(instance->motion.gesture_once|instance->motion.gesture_down)&~instance->motion.gesture_persistent;
    unsigned long long resolve_press=0;
    for(int lane=0;lane<HB_MOTION_LANES;lane++)if((once&(1ULL<<lane))&&instance->motion.lanes[lane].operation==HB_MO_AUTO_CHORD_REPEAT){
        if(on&&hb_mo_chord_arp_choice(instance->motion.lanes[lane].amount)>=3)resolve_press|=1ULL<<lane;
        if(on&&!instance->motion.gesture_target_owner[lane]){
            instance->motion.gesture_target_owner[lane]=(unsigned short)owner;
            if(hb_mo_chord_arp_choice(instance->motion.lanes[lane].amount)<3)
                instance->target_attack_owner=(unsigned short)owner;
        }
        if(off&&instance->motion.gesture_target_owner[lane]==owner)instance->motion.gesture_target_released|=1ULL<<lane;
    }
    /* Resolve before rendering the target: it plays with underlying settings,
       rather than beginning an arp that is immediately cut off afterward. */
    if(resolve_press){hb_mo_end_lanes(&instance->motion,resolve_press);hb_auto_chord_repeat_sync(instance);}
}
static void hb_gesture_finish_use(Inst *instance){
    if(instance->synthetic_advance)return;
    unsigned long long once=(instance->motion.gesture_once|instance->motion.gesture_down)&~instance->motion.gesture_persistent;
    unsigned long long target_lanes=0;
    for(int lane=0;lane<HB_MOTION_LANES;lane++)if(instance->motion.lanes[lane].operation==HB_MO_AUTO_CHORD_REPEAT)target_lanes|=1ULL<<lane;
    unsigned long long mask=once&target_lanes&instance->motion.gesture_target_released;
    if(instance->player.release_ms!=0)for(int lane=0;lane<HB_MOTION_LANES;lane++)if(mask&(1ULL<<lane)){
        int owner=instance->motion.gesture_target_owner[lane]-1,waiting=0;
        for(int key=0;key<HB_CP_KEYS;key++){
            const hb_cp_key *held=&instance->player.keys[key];
            if(held->used&&held->channel==owner/128&&held->source==owner%128&&
               (held->held||held->release_end>hb_cp_release_clock(&instance->player,held)))waiting=1;
        }
        for(int event=0;event<instance->follower_queue_count;event++)
            if(instance->follower_queue_channel[event]==owner/128&&instance->follower_queue_note[event]==owner%128)waiting=1;
        if(waiting)mask&=~(1ULL<<lane);
    }
    if(!instance->active_count&&!instance->follower_queue_count)mask|=once&~target_lanes&instance->motion.gesture_once_used;
    if(mask){hb_mo_end_lanes(&instance->motion,mask);hb_auto_chord_repeat_sync(instance);}
}
#include "../../../src/motif_runtime.h"
#include "../../../src/approach_rows_runtime.h"
/* Observe only final local MIDI, after chord/arp, motif and motion/rhythm
   scheduling. Pool membership and queued notes are not sounding notes. */
static void hb_pad_observe_output(Inst *instance,uint8_t output[][3],const int lengths[],int count){
    for(int index=0;index<count;index++){
        const uint8_t *message=output[index];int length=lengths[index];
        if(length<1)continue;
        int type=message[0]&0xf0,channel=message[0]&15;
        uint16_t bit=(uint16_t)(1u<<channel);
        if(message[0]==0xfc){memset(instance->pad_sounding,0,sizeof(instance->pad_sounding));memset(instance->pad_flash_seconds,0,sizeof(instance->pad_flash_seconds));continue;}
        if(length<3)continue;
        int pitch=message[1]&127;
        if(type==0x90&&message[2]){instance->pad_sounding[pitch]|=bit;instance->pad_flash_seconds[pitch]=0.075;}
        else if(type==0x80||type==0x90)instance->pad_sounding[pitch]&=(uint16_t)~bit;
        else if(type==0xb0&&(message[1]==120||message[1]==123))
            for(int note=0;note<128;note++){instance->pad_sounding[note]&=(uint16_t)~bit;if(!instance->pad_sounding[note])instance->pad_flash_seconds[note]=0;}
    }
}
static int process_key_base(void *value,const uint8_t *input,int length,uint8_t output[][3],int lengths[],int capacity){
    Inst *instance=(Inst*)value;
    if(instance&&input&&length>0){
        if(input[0]==0xFC||(length>=3&&(input[0]&0xf0)==0xB0&&(input[1]==120||input[1]==123)))instance->motif.cancel=1;
        if(length>=3&&(input[0]&0xf0)==0x90&&input[2]&&!instance->movy_playback){
            int target=(input[1]&127)+instance->movy_pad_shift[input[1]&127];
            if(target>=0&&target<=127)instance->motif_load_last=target;
        }
        hb_gesture_target_input(instance,input,length);
        if(hb_ar_input(instance,input,length)){hb_gesture_finish_use(instance);return 0;}
        if(hb_mt_input(instance,input,length)){hb_gesture_finish_use(instance);return 0;}
    }
    if(instance&&length>=3&&(input[0]&0xf0)==0x90&&input[2]&&instance->movy_playback&&instance->recorded_action_valid[input[1]&127])
        instance->motion.event_override=instance->recorded_actions[input[1]&127];
    int count=process_with_actions(value,input,length,output,lengths,capacity);
    if(instance&&instance->approach_rows.restore_word){instance->motion.events[HB_MOTION_LANES]=instance->approach_rows.saved_word;instance->approach_rows.restore_word=0;}
    if(instance){instance->motion.event_override=0;hb_gesture_finish_use(instance);hb_pad_observe_output(instance,output,lengths,count);}
    return count;
}
static void hb_key_operations_sync(void){
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *instance=&g_pool[index];if(!instance->used||instance->role>=2)continue;
        int parallel=0,override_scope=0;
        unsigned long long active=0;
        for(int lane=0;lane<HB_MOTION_USER_LANES;lane++){
            int operation=instance->motion.lanes[lane].operation;
            /* Reuse this existing scan: an inactive override adds no
               second all-track/all-lane walk to every MIDI event. */
            if(instance->role==1&&!instance->motion.bypass&&(instance->motion.held&(1ULL<<lane))){
                if(operation==HB_MO_HARMONY_OVERRIDE)override_scope=2;
                else if(operation==HB_MO_LIVE_HARMONY_OVERRIDE&&!override_scope)override_scope=1;
            }
            if(operation!=HB_MO_KEY_CENTER&&operation!=HB_MO_PARALLEL_SCALE&&(operation<HB_MO_RECORD_KEY||operation>HB_MO_KEY_RETURN))continue;
            double value;
            if(!hb_mo_value_at(&instance->motion,lane,hb_motion_position(instance),hb_motion_condition_position(),0,&value))continue;
            active|=1ULL<<lane;
            if(operation!=HB_MO_PARALLEL_SCALE&&!(instance->key_lane_active&(1ULL<<lane))){
                instance->key_action=operation==HB_MO_KEY_CENTER?0:operation-HB_MO_RECORD_KEY+1;
                instance->key_shift=instance->motion.lanes[lane].amount;
                instance->key_return_after=instance->motion.lanes[lane].offset;
                if(instance->key_action>=2){hb_key_commit(instance,hb_key_baseline(instance));instance->motion.gesture_used|=1ULL<<lane;}
                else {g_key_armed=1;g_sc_arm_owner=index;}
            }
            if(operation==HB_MO_PARALLEL_SCALE)parallel=1;
        }
        instance->key_lane_active=active;
        hb_override_set_scope(instance,override_scope);
        if(parallel!=g_sc_lane[index]){
            g_sc_lane[index]=parallel;
            if(parallel||(!g_sc_manual[index]&&!g_sc_latch[index]))hb_key_parallel(instance,parallel);
        }
    }
}
static int process(void *value,const uint8_t *input,int length,uint8_t output[][3],int lengths[],int capacity){
    Inst *instance=(Inst*)value;if(!instance)return 0;
    if(input&&length>=1&&input[0]==0xfc){memset(instance->physical_velocity,0,sizeof(instance->physical_velocity));instance->physical_target=instance->target_attack_owner=0;}
    if(input&&length>=3&&!instance->movy_playback&&hb_source_channel_matches(instance,input[0]&15)){
        int type=input[0]&0xf0,channel=input[0]&15,note=input[1]&127;
        if(type==0xb0&&(note==120||note==123)){memset(instance->physical_velocity[channel],0,128);instance->physical_target=instance->target_attack_owner=0;}
        else if(type==0x80||(type==0x90&&!input[2]))instance->physical_velocity[channel][note]=0;
        else if(type==0x90&&instance->role<2&&instance->motif.editor.recording<0&&instance->movy_pad_pending!=note+1){
            instance->physical_velocity[channel][note]=input[2];instance->physical_target=(unsigned short)(channel*128+note+1);
        }
    }
    hb_key_operations_sync();
    int on=input&&length>=3&&(input[0]&0xf0)==0x90&&input[2];
    if(on){int owner=(input[1]&127)+(instance->movy_playback?128:0);instance->trail_input[owner]=++instance->trail_serial;}
    int land=on&&g_key_armed&&(g_sc_arm_owner<0||g_sc_arm_owner==instance-g_pool)&&!instance->movy_playback&&instance->role<2&&instance->motif.editor.recording<0&&
        hb_source_channel_matches(instance,input[0]&15)&&instance->movy_pad_pending!=(input[1]&127)+1;
    if(on&&g_key_armed&&(g_sc_arm_owner<0||g_sc_arm_owner==instance-g_pool)&&!instance->movy_playback&&instance->role<2&&hb_source_channel_matches(instance,input[0]&15)){
        hb_key_context current=hb_key_baseline(instance),destination=hb_key_destination(instance,input[1]&127);
        snprintf(g_key_preview,sizeof(g_key_preview),"%s%s>%s%s",HB_KEY_NAMES[current.target_root],hb_key_quality(current),HB_KEY_NAMES[destination.target_root],hb_key_quality(destination));
    }
    if(input&&instance->motif.editor.recording<0&&hb_source_channel_matches(instance,input[0]&15)&&
       !(on&&instance->movy_pad_pending==(input[1]&127)+1&&!hb_approach_pad_enabled(instance)))
        hb_override_mark(instance,input,length);
    instance->key_schedule_arm=land;
    if(land&&instance->approach_rows.performance){
        unsigned token=hb_ar_peek(&instance->approach_rows);hb_mt_phrase builtin;
        const hb_mt_phrase *phrase=hb_ar_phrase(token&63,&builtin);
        land=phrase&&((token>>6)&31)==phrase->anchor;
    }
    if(land&&instance->motif.tap_active)land=instance->motif.tap_step==instance->motif.tap_phrase.anchor;
    if(land&&!instance->approach_rows.performance&&!instance->motif.tap_active){
        /* Peek at the exact touch-sequence step without advancing the live
           cursor. A V/ii/connector is an approach, never the key landing. */
        hb_motion_config pending=instance->motion;pending.event_override=0;
        hb_mo_input(&pending,input[1]&127,hb_motion_position(instance),hb_ms_to_beats(25));
        int secondary=hb_mo_held_secondary(&pending);
        if(!secondary)secondary=hb_mo_source_secondary(&pending);
        const hb_cadence_step *cadence=hb_mo_current_cadence(&pending);
        if((secondary&&secondary!=3)||hb_mo_source_modifier(&pending)||hb_mo_held_modifier(&pending)||
           (cadence&&cadence->kind!=HB_CAD_TARGET&&cadence->kind!=HB_CAD_MINOR_TARGET))land=0;
    }
    instance->key_onset=hb_key_for(instance);
    if(on&&instance->movy_playback&&instance->sc_anchor_valid&&instance->sc_anchor_pitch==(input[1]&127)){instance->key_onset=instance->sc_anchor_context;instance->sc_anchor_valid=0;}
    if(on&&instance->movy_playback&&instance->recorded_action_valid[input[1]&127]){
        unsigned long long intent=instance->recorded_actions[input[1]&127][HB_MOTION_LANES];
        if((intent&(1ULL<<20))&&((intent>>21)&3)==3){
            int scale=(intent>>36)&31;instance->key_onset.approach_scale=scale==31?-1:scale;
        }
    }
    instance->key_scope=1;
    int count=process_key_base(value,input,length,output,lengths,capacity);
    if(land&&!instance->key_pending)hb_key_land(instance,input[1]&127);
    if(instance->key_pending&&instance->key_pending_at<=hb_motion_position(instance)+1e-6){hb_key_commit_pending(instance);}
    instance->key_schedule_arm=0;instance->key_scope=0;return count;
}
static int hb_motion_recorded_active(const unsigned long long *events){
    if(events[HB_MOTION_LANES])return 1;
    for(int lane=0;lane<HB_MOTION_LANES;lane++)if((events[lane]&HB_MO_RECORDED)&&((events[lane]>>32)&31))return 1;
    return 0;
}
static int hb_motion_pending_trigger(const Inst *instance){
    for(int index=0;index<instance->follower_queue_count;index++)
        if(hb_motion_recorded_active(instance->motion_follower_events[index]))return 1;
    if(hb_cp_enabled(&instance->player))for(int index=0;index<HB_CP_KEYS;index++)
        if(instance->player.keys[index].used&&hb_motion_recorded_active(instance->motion_player_events[index]))return 1;
    return 0;
}

static int tick_base(void *value,int frames,int sample_rate,uint8_t output[][3],int lengths[],int capacity){
    Inst *instance=(Inst*)value;if(!instance)return 0;
    hb_gesture_finish_use(instance);
    for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used)hb_auto_chord_repeat_sync(&g_pool[index]);
    if(frames>0&&sample_rate>0){double bpm=g_host&&g_host->get_bpm?g_host->get_bpm():120.0;if(bpm<=0)bpm=120.0;instance->motion_beat+=(double)frames*bpm/(60.0*sample_rate);}
    hb_motion_tick_routes(instance);
    /* Receiver already owns shared rendered gates. Only drain this route while
       a previous Conductor/Follower role still has pending output. */
    if(instance->role>=2&&!instance->player.sounding_count&&!instance->motion_local.owned&&
       !instance->motion_local.count&&!hb_rr_active(&instance->motion_local.rhythm))
        return tick_core(value,frames,sample_rate,output,lengths,capacity);
    uint8_t pending[128][3];int sizes[128];
    int room=(HB_MOTION_QUEUE-instance->motion_local.count)/2;if(room>128)room=128;
    instance->motion_output_base=pending;memset(instance->motion_output_owner,0,sizeof(instance->motion_output_owner));memset(instance->trail_outputs,0,sizeof(instance->trail_outputs));memset(instance->motion_output_valid,0,sizeof(instance->motion_output_valid));memset(instance->rhythm_output_prewarped,instance->movy_playback!=0,sizeof(instance->rhythm_output_prewarped));
    int count=tick_core(value,frames,sample_rate,pending,sizes,room);instance->motion_output_base=0;
    for(int index=0;index<count;index++)if(sizes[index]==3){
        instance->motion.event_override=instance->motion_output_valid[index]?instance->motion_output_events[index]:0;
        int origin=instance->movy_playback;instance->movy_playback=instance->rhythm_output_prewarped[index];
            instance->motion_event_owner=instance->motion_output_owner[index];instance->motion_local.trail_in=instance->trail_outputs[index];hb_motion_output(instance,&instance->motion_local,pending[index]);instance->motion_local.trail_in=0;instance->motion_event_owner=0;instance->movy_playback=origin;instance->motion.event_override=0;
    }
    int emitted=hb_motion_local_drain(instance,output,lengths,capacity);hb_gesture_finish_use(instance);return emitted;
}
static int tick(void *value,int frames,int sample_rate,uint8_t output[][3],int lengths[],int capacity){
    hb_key_operations_sync();
    Inst *instance=(Inst*)value;if(!instance)return 0;
    int advanced=0;
    if(instance->advance_pending&&capacity>0){
        unsigned owner=instance->advance_owner;int channel=(owner-1)/128,note=(owner-1)%128;
        if(!owner||!instance->physical_velocity[channel][note]){instance->advance_pending=instance->advance_phase=0;}
        else {
            uint8_t message[3]={(uint8_t)((instance->advance_phase?0x90:0x80)|channel),(uint8_t)note,instance->advance_phase?instance->physical_velocity[channel][note]:0};
            instance->synthetic_advance=1;
            int count=process_key_base(instance,message,3,output,lengths,capacity);
            instance->synthetic_advance=0;
            if(instance->advance_phase){instance->advance_pending--;instance->advance_phase=0;}
            else instance->advance_phase=1;
            advanced=count;
        }
    }
    hb_trail_boundary(instance);
    /* A bounded visual onset survives the 50 ms UI poll without extending MIDI gates. */
    if(frames>0&&sample_rate>0)for(int pitch=0;pitch<128;pitch++){
        double remaining=instance->pad_flash_seconds[pitch]-(double)frames/sample_rate;
        instance->pad_flash_seconds[pitch]=remaining>0?remaining:0;
    }
    int was_playing=instance->last_transport_playing;
    int emitted=advanced+hb_mt_tick(instance,output+advanced,lengths+advanced,capacity-advanced);
    emitted+=tick_base(value,frames,sample_rate,output+emitted,lengths+emitted,capacity-emitted);
    hb_pad_observe_output(instance,output,lengths,emitted);
    if(was_playing&&!instance->last_transport_playing)memset(instance->pad_flash_seconds,0,sizeof(instance->pad_flash_seconds));
    return emitted;
}
static void hb_restore_state(Inst *instance,const char *state);
static int enum_index(const char *value,const char *const *options,int count,int fallback){
    /* Match enum labels BEFORE attempting numeric index parsing.
       Labels such as "1/16", "1/8", "1/4", "1/2", "1 Bar", and "2 Bars"
       all begin with a digit; strtol() previously consumed that leading digit
       and misread them as enum indices 1 or 2. "Free" worked only because it
       is non-numeric. */
    if(value){
        /* Accept the previous inversion label for existing callers. */
        if(!strcmp(value,"Top Note"))value="Played Top Note";
        for(int index=0;index<count;index++)if(!strcmp(value,options[index]))return index;
        char *end=0;
        long parsed=strtol(value,&end,10);
        if(end&&end!=value&&*end=='\0'&&parsed>=0&&parsed<count)return (int)parsed;
    }
    return fallback;
}
#include "../../../src/motion_params.h"
static const char *NEXT_PREDICT_OPTS[]={"Off","On"};static const char *NEXT_LOOKAHEAD_OPTS[]={"Off","Immediate","After 1/32","After 1/16","After 1/8","After 1/4","After 3/8","After 1/2","After 3/4","After 1 Bar","After 1.5 Bars","After 2 Bars","After 3 Bars","After 4 Bars","Before 1/32","Before 1/16","Before 1/8","Before 1/4","Before 3/8","Before 1/2","Before 3/4","Before 1 Bar","Before 1.5 Bars","Before 2 Bars","Before 3 Bars","Before 4 Bars","Late 1/32","Late 1/16","Late 1/8","Late 1/4","Late 3/8","Late 1/2","Late 3/4","Late 1 Bar","Late 1.5 Bars","Late 2 Bars","Late 3 Bars","Late 4 Bars"};
static const int NEXT_LOOKAHEAD_IDS[]={0,25,26,27,28,29,30,31,32,33,34,35,36,37,1,2,3,4,13,5,14,6,15,19,20,23,7,8,9,10,16,11,17,12,18,21,22,24};
static int hb_next_parse_lookahead(const char *value,int fallback){
    for(int option=0;option<HB_LOOKAHEAD_COUNT;option++){
        const char *label=NEXT_LOOKAHEAD_OPTS[option];
        if(!strcmp(value,label)||(!strncmp(label,"Before ",7)&&!strcmp(value,label+7))||
           (!strncmp(label,"Late ",5)&&value[0]=='-'&&!strcmp(value+1,label+5)))return NEXT_LOOKAHEAD_IDS[option];
    }
    char *end=0;long index=strtol(value,&end,10);
    return end!=value&&!*end&&index>=0&&index<HB_LOOKAHEAD_COUNT?(int)index:fallback;
}
static const char *hb_next_lookahead_label(int index){
    for(int option=0;option<HB_LOOKAHEAD_COUNT;option++)if(NEXT_LOOKAHEAD_IDS[option]==index)return NEXT_LOOKAHEAD_OPTS[option];
    return "Off";
}
static const char *ROLE_OPTS[]={"Conductor","Follower","Off","Receiver"};static const char *RETRIGGER_OPTS[]={"Off","On"};static const char *APPROACH_OPTS[]={"Chrom Below","Off","Scale Above"};static const char *APPROACH_MODE_OPTS[]={"Next","Held","Off"};static const char *QUANT_GRID_OPTS[]={"Off","1/16","1/8","1/4","1/2","1 Bar","2 Bars","4 Bars"};static const char *FOLLOWER_SOURCE_POLICY_OPTS[]={"Infer Input","Infer Notes","Explicit"};static const char *CLIP_SLOT_OPTS[]={"Auto","1","2","3","4","5","6","7","8"};static const char *SENSOR_SOURCE_OPTS[]={"Realtime","Realtime + Clip","Clip"};static const char *CLIP_CONTEXT_OPTS[]={"Clip + Realtime","Realtime Only"};static const char *SOURCE_CH_OPTS[]={"Auto","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"};static const char *RENDER_CH_OPTS[]={"Off","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"};static const char *MODE_OPTS[]={"Relative","Smooth","Nearest"};static const char *POLICY_OPTS[]={"Explicit","Current Input Root","Auto-Infer"};static const char *MAP_TARGET_OPTS[]={"Chord","Scale"};static const char *TIMING_OPTS[]={"Observed","1/16","1/8","1/4","1/2","1 Bar","2 Bars","4 Bars"};
#define FOLLOWER_SCALE_OPTS HB_SCALE_NAMES
static const char *ANTICIPATION_OPTS[]={"On Grid","1/64 Early","1/32 Early","1/16 Early","1/8 Early","1/4 Early"};static const char *CONTEXT_OPTS[]={"Live","1/32","1/16","1/8","1/4","1/2","1 Bar"};static const char *TIMESCALE_OPTS[]={"Free","1/16","1/8","1/4","1/2","1 Bar"};static const char *STABILITY_OPTS[]={"Responsive","Balanced","Stable"};static const char *ACCIDENTAL_OPTS[]={"Auto","Sharps","C#D#F#G#Bb","C#EbF#G#Bb","C#EbF#AbBb","DbEbF#AbBb","Flats"};static const char *PC_OPTS[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
static const char *MASTER_ROOT_OPTS[]={"As Played","C","C# / Db","D","D# / Eb","E","F","F# / Gb","G","G# / Ab","A","A# / Bb","B"};
static int hb_timing_to_legacy_timescale(int timing){
    /* Chord Grid is a musical-boundary hint, not a mandatory dwell time. */
    (void)timing;
    return 0;
}
static double hb_chord_grid_beats(void){
    static const double beats[8]={0.0,0.25,0.5,1.0,2.0,4.0,8.0,16.0};
    int index=g_bus.chord_timing;
    return (index>=0&&index<8)?beats[index]:0.0;
}
static int hb_arp_phase_limit(int rate){
    double grid=hb_chord_grid_beats();
    return (int)((grid>0.0?grid:4.0)/hb_cp_division(rate%9));
}
static double hb_quant_grid_beats_for(const Inst *instance){
    static const double beats[8]={0.0,0.25,0.5,1.0,2.0,4.0,8.0,16.0};
    (void)instance;int index=g_bus.quant_timing;
    return (index>=0&&index<8)?beats[index]:0.0;
}
static double hb_anticipation_beats(void){
    static const double beats[6]={0.0,0.0625,0.125,0.25,0.5,1.0};
    int index=g_bus.anticipation;
    return (index>=0&&index<6)?beats[index]:0.0;
}
static double hb_ms_to_beats(int milliseconds){
    double bpm=(g_host&&g_host->get_bpm)?g_host->get_bpm():120.0;
    if(bpm<=0.0)bpm=120.0;
    return ((double)milliseconds*bpm)/(60000.0);
}
static double hb_current_beat(void){
    if(g_movy_present)return (double)g_movy_tick/g_movy_ppqn;
    if(g_host&&g_host->get_beat_position){
        double beat=g_host->get_beat_position();
        if(beat>=0.0)return beat;
    }
    return (double)g_bus.clip_clock_ticks/24.0;
}
static int hb_context_to_legacy_stability(int context){
    if(context<=1)return 0;
    if(context<=3)return 1;
    return 2;
}
static const char *BORROWED_SCALE_OPTS[]={"Minimal","Aeolian","Dorian","Mixolydian b6"};
static const char *DOMINANT_SCALE_OPTS[]={"None","Harmonic Minor","Melodic Minor","Altered V","Major","Harmonic Major","Simplified Target","Minimal"};
static const char *CP_CHORD_MODE[]={"Off","Rendered Note Root","Conductor Chord"};
static const char *CP_CHORD_INVERSION[]={"Auto","Root","First","Second","Third","Fourth","Fifth","Sixth","Played Top Note"};
static const char *CP_CHORD_VOICING[]={"Close","Root + Fifth Low","Alternate Up","Shell"};
static const char *CP_ARP_PHASE[]={"Free","On Grid","First Note Free"};
static const char *CP_CHORD_QUALITY[]={"Auto","Major","Minor","Dim","Aug","Maj7","Dom7","Min7","Half Dim7","Dim7","MinMaj7","AugMaj7","Dom7b5"};
static const char *CP_CHROMATIC_QUALITY[]={"Scale","Major / Maj7","Major / Dom7","Dim / Dim7","Minor / Min7","Dim / Min7b5","Auto Dim7 / Min7b5"};
static const char *CP_ARP_PLAYBACK[]={"Together","Repeat Arp","Once"};
static const char *CP_ARP_HOLD[]={"Momentary","Latch - Overlap","Latch with Off - Overlap","Latch Acc. with Off","Latch - Single","Latch with Off - Single"};
static const char *CP_ARP_START[]={"Order","Lowest / Pad","Highest / Pad","Lowest / Chord","Highest / Chord","Played / Pad","Played / Chord"};
static const char *CP_ARP_ORDER[]={"Up","Down","Up-Down","Played","Random","Shuffle","Shuffle Cycle Pin"};
static const char *CP_ARP_RATE[]={"1/64","1/32","1/16","1/8","1/4","1/2","1 Bar","2 Bars","4 Bars","Cycle 1/64","Cycle 1/32","Cycle 1/16","Cycle 1/8","Cycle 1/4","Cycle 1/2","Cycle 1 Bar","Cycle 2 Bars","Cycle 4 Bars"};
static const char *CP_ARP_GATE[]={"25%","50%","75%","90%"};
#include "../../../src/role_policy_params.h"
static const char CHAIN_PARAMS[]="["
"{\\\"key\\\":\\\"role\\\",\\\"name\\\":\\\"Role\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Conductor\\\",\\\"Follower\\\",\\\"Off\\\"],\\\"options_as_string\\\":true},"
"{\\\"key\\\":\\\"clip_context\\\",\\\"name\\\":\\\"Clip Context\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Clip + Realtime\\\",\\\"Realtime Only\\\"],\\\"options_as_string\\\":true},"
"{\\\"key\\\":\\\"mode\\\",\\\"name\\\":\\\"Follow Mode\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Transpose\\\",\\\"Chord\\\",\\\"Nearest\\\"],\\\"options_as_string\\\":true},"
"{\\\"key\\\":\\\"root_policy\\\",\\\"name\\\":\\\"Root Policy\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Explicit\\\",\\\"Current Input Root\\\",\\\"Auto-Infer\\\"],\\\"options_as_string\\\":true},"
"{\\\"key\\\":\\\"explicit_root\\\",\\\"name\\\":\\\"Explicit Root\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"C\\\",\\\"C#\\\",\\\"D\\\",\\\"Eb\\\",\\\"E\\\",\\\"F\\\",\\\"F#\\\",\\\"G\\\",\\\"Ab\\\",\\\"A\\\",\\\"Bb\\\",\\\"B\\\"],\\\"options_as_string\\\":true},"
"{\\\"key\\\":\\\"content_map\\\",\\\"name\\\":\\\"Follower Content\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Chord\\\",\\\"Scale\\\",\\\"Free\\\"],\\\"options_as_string\\\":true,\\\"default\\\":\\\"Chord\\\"},"
"{\\\"key\\\":\\\"travel_map\\\",\\\"name\\\":\\\"Follower Travel\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Relative\\\",\\\"Closest\\\",\\\"Up\\\"],\\\"options_as_string\\\":true,\\\"default\\\":\\\"Relative\\\"},"
"{\\\"key\\\":\\\"follower_scale\\\",\\\"name\\\":\\\"Follower Scale\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Infer\\\",\\\"Major\\\",\\\"Natural Minor\\\",\\\"Dorian\\\",\\\"Phrygian\\\",\\\"Lydian\\\",\\\"Mixolydian\\\",\\\"Locrian\\\",\\\"Harmonic Minor\\\",\\\"Melodic Minor\\\"],\\\"options_as_string\\\":true,\\\"default\\\":\\\"Infer\\\"},"
"{\\\"key\\\":\\\"quant_timing\\\",\\\"name\\\":\\\"Quant Grid\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Off\\\",\\\"1/16\\\",\\\"1/8\\\",\\\"1/4\\\",\\\"1/2\\\",\\\"1 Bar\\\",\\\"2 Bars\\\",\\\"4 Bars\\\"],\\\"options_as_string\\\":true,\\\"default\\\":\\\"Off\\\"},"
"{\\\"key\\\":\\\"transpose\\\",\\\"name\\\":\\\"Transpose\\\",\\\"type\\\":\\\"int\\\",\\\"min\\\":-24,\\\"max\\\":24,\\\"step\\\":1},"
"{\\\"key\\\":\\\"window_ms\\\",\\\"name\\\":\\\"Inference Window\\\",\\\"type\\\":\\\"int\\\",\\\"min\\\":10,\\\"max\\\":500,\\\"step\\\":5},"
"{\\\"key\\\":\\\"harmony\\\",\\\"name\\\":\\\"Harmony\\\",\\\"type\\\":\\\"string\\\",\\\"access\\\":\\\"read\\\"},"
"{\\\"key\\\":\\\"detected_root\\\",\\\"name\\\":\\\"Root\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"C\\\",\\\"C#\\\",\\\"D\\\",\\\"Eb\\\",\\\"E\\\",\\\"F\\\",\\\"F#\\\",\\\"G\\\",\\\"Ab\\\",\\\"A\\\",\\\"Bb\\\",\\\"B\\\"],\\\"options_as_string\\\":true,\\\"access\\\":\\\"read\\\"},"
"{\\\"key\\\":\\\"detected_bass\\\",\\\"name\\\":\\\"Bass\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"C\\\",\\\"C#\\\",\\\"D\\\",\\\"Eb\\\",\\\"E\\\",\\\"F\\\",\\\"F#\\\",\\\"G\\\",\\\"Ab\\\",\\\"A\\\",\\\"Bb\\\",\\\"B\\\"],\\\"options_as_string\\\":true,\\\"access\\\":\\\"read\\\"},"
"{\\\"key\\\":\\\"confidence\\\",\\\"name\\\":\\\"Confidence\\\",\\\"type\\\":\\\"int\\\",\\\"min\\\":0,\\\"max\\\":100,\\\"access\\\":\\\"read\\\"}"
"]";
static void hb_set_master_transpose(int semitones){
    if(semitones==g_bus.global_transpose)return;
    int delta=semitones-g_bus.global_transpose;
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *voice=&g_pool[index];if(!voice->used)continue;
        uint8_t pending[128];memcpy(pending,voice->role_flush_pending,sizeof(pending));
        hb_prepare_role_change_flush(voice);
        for(int pitch=0;pitch<128;pitch++)voice->role_flush_pending[pitch]|=pending[pitch];
        hb_cp_key retained[HB_CP_KEYS];
        int keep=hb_cp_settings(&voice->player)->latch&&hb_cp_enabled(&voice->player);
        if(keep)memcpy(retained,voice->player.keys,sizeof(retained));
        /* Transpose flushes sounding notes, but is not a release of a
           persistent harmonic authority. Preserve only its latched lanes. */
        unsigned long long override_latches=0;
        for(int lane=0;lane<HB_MOTION_USER_LANES;lane++)
            if(voice->motion.lanes[lane].operation==HB_MO_LIVE_HARMONY_OVERRIDE||voice->motion.lanes[lane].operation==HB_MO_HARMONY_OVERRIDE)
                override_latches|=voice->motion.gesture_persistent&(1ULL<<lane);
        hb_clear_instance_note_state(voice);
        voice->motion.held|=override_latches;
        voice->motion.gesture_persistent|=override_latches;
        voice->motion.gesture_latched|=override_latches;
        if(keep){
            memcpy(voice->player.keys,retained,sizeof(retained));
            for(int owner=0;owner<HB_CP_KEYS;owner++){
                hb_cp_key *key=&voice->player.keys[owner];if(!key->used)continue;
                key->fresh=1;key->started=0;
                for(int note=0;note<key->count;note++)key->notes[note]=hb_cp_clamp(key->notes[note]+delta,0,127);
            }
        }
        voice->candidate_harmony=hb_transpose_harmony(voice->candidate_harmony,delta);
    }
    hb_key_context *contexts[]={&g_key_context,&g_parallel_previous};
    for(int index=0;index<2;index++)if(contexts[index]->active){
        contexts[index]->source_root=mod12(contexts[index]->source_root+delta);
        contexts[index]->target_root=mod12(contexts[index]->target_root+delta);
        contexts[index]->source_mask=hb_transpose_mask(contexts[index]->source_mask,delta);
        contexts[index]->target_mask=hb_transpose_mask(contexts[index]->target_mask,delta);
    }
    g_key_preview[0]=0;
    for(int owner=0;owner<HB_MAX_INSTANCES;owner++)if(g_override[owner].harmony.valid||g_override[owner].pending){g_override[owner].harmony=hb_transpose_harmony(g_override[owner].harmony,delta);g_override[owner].next=hb_transpose_harmony(g_override[owner].next,delta);for(int origin=0;origin<2;origin++)for(int note=0;note<128;note++)g_override[owner].masks[origin][note]=hb_transpose_mask(g_override[owner].masks[origin][note],delta);}
    g_bus.global_transpose=semitones;
    if(g_sc_ready){g_sc_dirty=1;hb_sc_resolve(&g_pool[0]);}
    hb_effective_write(hb_transpose_harmony(bus_read(),delta));
    g_bus.observed_harmony=hb_transpose_harmony(g_bus.observed_harmony,delta);
    for(int i=0;i<g_bus.next_model_count;i++)g_bus.next_model[i].harmony=hb_transpose_harmony(g_bus.next_model[i].harmony,delta);
    for(int i=0;i<g_bus.next_learning_count;i++)g_bus.next_learning[i].harmony=hb_transpose_harmony(g_bus.next_learning[i].harmony,delta);
    next_pending=hb_transpose_harmony(next_pending,delta);
    next_configuration=hb_next_configuration();
}
static void hb_set_scale_exceptions(int dominant,int borrowed){
    if(dominant!=hb_shared_dominant_scale()||borrowed!=hb_shared_borrowed_scale()){
        for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used){
            hb_prepare_role_change_flush(&g_pool[index]);hb_clear_instance_note_state(&g_pool[index]);
        }
        hb_store_scale_exceptions(dominant,borrowed);
    }
    for(int role=0;role<2;role++){hb_role_store(role,HB_P_DOMINANT,dominant);hb_role_store(role,HB_P_BORROWED,borrowed);}
    g_scale_exceptions_restored=1;
}
/* Both surfaces share the selected input mode, including its gesture reset. */
static void hb_surface_layout(Inst *instance){
    int layout=instance->surface_enabled&&instance->surface_count[0]?
        instance->surface_approach_layout:instance->move_approach_layout;
    if(layout!=instance->approach_layout){
        /* Layout changes invalidate the old gesture mode, but keep saved
           row assignments. Entering spatial mode clears permanent motif latches. */
        instance->approach_rows.down=instance->approach_rows.knob_down=instance->approach_rows.step_down=0;
        instance->approach_rows.performance=instance->approach_rows.latch=instance->approach_rows.motif_latch=instance->approach_rows.selected=0;
        instance->approach_rows.turned=0;instance->approach_rows.bank_armed=-1;
        if(layout)instance->approach_rows.latch_slots=0;
    }
    instance->approach_layout=layout;
}
static void set_param_base(void *value,const char *key,const char *parameter){Inst *instance=(Inst*)value;if(!instance||!key||!parameter)return;
if(!strcmp(key,"trail_enable")){instance->trail_enabled=!strcmp(parameter,"1");return;}
if(!strcmp(key,"trail_clear")){memset(instance->trail_valid,0,sizeof(instance->trail_valid));memset(instance->trail_previous_valid,0,sizeof(instance->trail_previous_valid));return;}
if(!strcmp(key,"surface_enabled")){
    instance->surface_enabled=!strcmp(parameter,"1");
    hb_surface_layout(instance);return;
}
int surface_bank=!strcmp(key,"surface_preview0")?0:!strcmp(key,"surface_preview1")?1:-1;
if(!strcmp(key,"pad_preview_inputs")||surface_bank>=0){
    size_t size=strlen(parameter);int notes[32],targets[32],rows[32],layout=0;
    if(size>=2&&parameter[size-2]==';'&&(parameter[size-1]=='0'||parameter[size-1]=='1')){layout=parameter[size-1]-'0';size-=2;}
    if(size!=64&&size!=129&&size!=162)return;if(size>=129&&parameter[64]!=':')return;if(size==162&&parameter[129]!=':')return;
    for(int slot=0;slot<32;slot++){
        unsigned note=0,target=0;
        if(sscanf(parameter+slot*2,"%2x",&note)!=1||(note>127&&note!=255))return;
        if(size>=129&&(sscanf(parameter+65+slot*2,"%2x",&target)!=1||target>128))return;
        rows[slot]=size==162?parameter[130+slot]-'0':0;if(rows[slot]<0||rows[slot]>3)return;
        notes[slot]=note==255?-1:(int)note;targets[slot]=target?(int)target-1:-1;
    }
    if(surface_bank>=0){
        memcpy(instance->surface_notes[surface_bank],notes,sizeof(notes));
        memcpy(instance->surface_targets[surface_bank],targets,sizeof(targets));
        memcpy(instance->surface_rows[surface_bank],rows,sizeof(rows));
        instance->surface_count[surface_bank]=32;instance->surface_approach_layout=layout;
        hb_surface_layout(instance);return;
    }
    instance->move_approach_layout=layout;
    memcpy(instance->preview_notes,notes,sizeof(notes));memcpy(instance->preview_targets,targets,sizeof(targets));memcpy(instance->preview_rows,rows,sizeof(rows));
    instance->preview_count=32;hb_surface_layout(instance);return;
}

if(!strcmp(key,"hb_tempo")){
    /* Control-message path only: request the host's shared Link tempo.
       Never alter an independent DSP clock or save tempo in track presets. */
    int bpm=hb_cp_clamp(parse_i(parameter,120),20,300);
    FILE *request=fopen("/data/UserData/schwung/desired-tempo","w");
    if(request){char text[24];int bytes=snprintf(text,sizeof(text),"%d\n",bpm);fwrite(text,1,(size_t)bytes,request);fclose(request);}
    return;
}
if(!strcmp(key,"humanize_timing")||!strcmp(key,"humanize_velocity")||!strcmp(key,"humanize_gate")){
    int index=!strcmp(key,"humanize_timing")?0:!strcmp(key,"humanize_velocity")?1:2;
    g_humanize[index]=hb_cp_clamp(parse_i(parameter,0),0,30);g_humanize_restored=1;return;
}

if(!strcmp(key,"render_velocity_percent")){instance->render_velocity_gain=hb_cp_clamp(parse_i(parameter,100),0,400)*100;return;}
if(!strcmp(key,"pad_adjacent_shading")){g_pad_adjacent_shading=!strcmp(parameter,"On");g_pad_restored=1;return;}
if(!strcmp(key,"pad_next_pulse")){g_pad_next_pulse=enum_index(parameter,PAD_NEXT_PULSE,12,g_pad_next_pulse);g_pad_restored=1;return;}
if(!strcmp(key,"pad_chord_form")){g_pad_chord_form=enum_index(parameter,CP_CHORD_FORM,HB_CP_FORMS,g_pad_chord_form);g_pad_restored=1;return;}
if(!strcmp(key,"pad_next_chord_form")){g_pad_next_chord_form=enum_index(parameter,CP_CHORD_FORM,HB_CP_FORMS,g_pad_next_chord_form);g_pad_restored=1;return;}
if(!strcmp(key,"pad_tonic_color")){g_pad_tonic_color=enum_index(parameter,PAD_TONIC_COLORS,10,g_pad_tonic_color);g_pad_restored=1;return;}
if(!strcmp(key,"pad_both_color")){g_pad_both_color=enum_index(parameter,PAD_BOTH_COLORS,10,g_pad_both_color);g_pad_restored=1;return;}
if(!strcmp(key,"pad_display")&&(!strcmp(parameter,"Effective")||!strcmp(parameter,"Standard"))){g_pad_settings[0]=2;g_pad_restored=1;return;}
if(!strcmp(key,"pad_play_color")){g_pad_play_color=enum_index(parameter,PAD_PLAY_COLORS,12,g_pad_play_color);g_pad_restored=1;return;}
/* Legacy automation aliases the shared harmony color. */
if(!strcmp(key,"pad_effective_color")){g_pad_settings[3]=enum_index(parameter,PAD_COLORS,9,g_pad_settings[3]);g_pad_restored=1;return;}
if(!strcmp(key,"render_velocity_gain")){
    char *end=0;double gain=strtod(parameter,&end);
    if(end!=parameter&&!*end&&gain>=0&&gain<=4)instance->render_velocity_gain=(int)(gain*10000+0.5);
    return;
}

int touch_slot=hb_mo_slot_key(key,"follow_touch_");if(touch_slot>=0&&touch_slot<9){int lanes[9];memcpy(lanes,g_touch_lanes,sizeof(lanes));lanes[touch_slot]=hb_cp_clamp(parse_i(parameter,lanes[touch_slot]),1,16);hb_touch_publish(lanes);return;}
hb_next_touch_clear_expired(instance);
int next_lane=hb_mo_slot_key(key,"motion_gesture_");if(next_lane<0)next_lane=hb_mo_slot_key(key,"motion_hold_");
unsigned long long next_before=instance->motion.held|instance->motion.gesture_down;
if(hb_mo_set(&instance->motion,key,parameter)){
    int assigned_lane=hb_mo_slot_key(key,"motion_operation_");
    if(!strcmp(key,"motion_operation")||(assigned_lane>=0&&assigned_lane<HB_MOTION_USER_LANES)){
        hb_motion_lane *lane=&instance->motion.lanes[assigned_lane>=0?assigned_lane:instance->motion.selected];
        if(lane->operation==HB_MO_CHORD_STATE&&!lane->chord_state_valid){lane->chord_state=instance->player.config;lane->chord_state_valid=1;lane->amount=0;}
    }
    if(!strcmp(key,"performance_reset")){instance->dominant_color_held=instance->dominant_color_latched=0;for(int index=0;index<HB_MAX_INSTANCES;index++)g_pool[index].key_pending=0;hb_sc_reset();memset(&g_key_context,0,sizeof(g_key_context));g_key_armed=g_parallel_on=g_parallel_latch=g_parallel_manual=g_key_lane_parallel=0;instance->motif.cancel=1;instance->motif.tap_active=0;instance->motif.editor.armed=-1;}
    if(next_lane>=0&&instance->motion.gesture_mode[next_lane]<3&&((instance->motion.lanes[next_lane].operation>=HB_MO_HARMONY&&instance->motion.lanes[next_lane].operation<=HB_MO_ENCLOSE_BA)||instance->motion.lanes[next_lane].operation>=HB_MO_SECONDARY_II)&&instance->motion.lanes[next_lane].auto_off==1&&
       !(next_before&(1ULL<<next_lane))&&((instance->motion.held|instance->motion.gesture_down|hb_mo_pending_lanes(&instance->motion))&(1ULL<<next_lane)))hb_next_touch_arm(instance,next_lane);
    if(next_lane>=0&&instance->motion.gesture_mode[next_lane]>=3&&(instance->motion.gesture_down&(1ULL<<next_lane)))instance->next_touch_mask&=~(1ULL<<next_lane);
    if(next_lane>=0&&instance->motion.gesture_mode[next_lane]>=3&&!(instance->motion.gesture_down&(1ULL<<next_lane))&&
       ((instance->motion.gesture_once|hb_mo_pending_lanes(&instance->motion))&(1ULL<<next_lane))&&
       !(instance->motion.gesture_persistent&(1ULL<<next_lane))&&
       (instance->motion.lanes[next_lane].auto_off==1||instance->motion.lanes[next_lane].operation==HB_MO_HARMONY||
        (instance->motion.lanes[next_lane].operation>=HB_MO_REPEAT&&instance->motion.lanes[next_lane].operation<=HB_MO_SPEED))&&
       !(instance->next_touch_mask&(1ULL<<next_lane)))hb_next_touch_arm(instance,next_lane);
    instance->next_touch_mask&=(instance->motion.held|instance->motion.gesture_down|instance->motion.gesture_latched|hb_mo_pending_lanes(&instance->motion))&~instance->motion.gesture_persistent;
    if(!strncmp(key,"motion_",7)&&strncmp(key,"motion_gesture_",15)&&strncmp(key,"motion_hold_",12)&&
       strcmp(key,"motion_host")&&strcmp(key,"motion_release"))hb_motion_publish_settings(instance);
    if(next_lane>=0&&instance->motion.lanes[next_lane].operation>=HB_MO_RELATIVE_KEY)hb_key_operations_sync();
    hb_mo_repeat_cancel(&instance->motion_local,&instance->motion,0);
    hb_mo_repeat_cancel(&instance->motion_render,&instance->motion,0);hb_motion_flush_render(instance);return;
}
for(int index=0;index<5;index++)if(!strcmp(key,PAD_KEYS[index])){g_pad_settings[index]=enum_index(parameter,PAD_OPTIONS[index],PAD_LIMITS[index],g_pad_settings[index]);g_pad_restored=1;return;}

if(!strcmp(key,"play_reset")){instance->play=(hb_fp_config){0};instance->play_revision++;return;}
if(!strcmp(key,"play_rotate")){int selected=hb_cp_clamp(parse_i(parameter,instance->play.rotate),-24,24);if(selected!=instance->play.rotate){instance->play.rotate=selected;instance->play_revision++;}return;}
if(!strcmp(key,"play_wrap")){static const char *options[]={"Off","On"};int selected=enum_index(parameter,options,2,instance->play.wrap);if(selected!=instance->play.wrap){instance->play.wrap=selected;instance->play_revision++;}return;}
if(!strcmp(key,"play_mirror")){static const char *options[]={"Off","On"};int selected=enum_index(parameter,options,2,instance->play.mirror);if(selected!=instance->play.mirror){instance->play.mirror=selected;instance->play_revision++;}return;}
if(!strcmp(key,"play_octave")){int selected=hb_cp_clamp(parse_i(parameter,instance->play.octave),-3,3);if(selected!=instance->play.octave){instance->play.octave=selected;instance->play_revision++;}return;}
if(!strcmp(key,"play_range")){static const char *options[]={"1 Oct","2 Oct","3 Oct","4 Oct"};int selected=enum_index(parameter,options,4,instance->play.range);if(selected!=instance->play.range){instance->play.range=selected;instance->play_revision++;}return;}
if(!strcmp(key,"play_scope")){static const char *options[]={"Both","Clip","Live"};int selected=enum_index(parameter,options,3,instance->play.scope);if(selected!=instance->play.scope){instance->play.scope=selected;instance->play_revision++;}return;}
if(!strcmp(key,"play_bypass")){static const char *options[]={"Off","On"};int selected=enum_index(parameter,options,2,instance->play.bypass);if(selected!=instance->play.bypass){instance->play.bypass=selected;instance->play_revision++;}return;}
if(!strcmp(key,"receive_channel")){
    int channel=enum_index(parameter,RENDER_CH_OPTS,17,instance->source_channel+1)-1;
    if(channel!=instance->source_channel){hb_receiver_reset(instance);instance->source_channel=channel;}
    return;
}

if(!strcmp(key,"master_transpose")){
    int selected=enum_index(parameter,MASTER_ROOT_OPTS,13,-1);
    if(selected==0){hb_set_master_transpose(0);return;}
    int reference=0;
    if(selected>0 && hb_resolve_follower_reference_root(instance,&reference)){
        int delta=mod12(selected-1-reference);
        if(delta>6)delta-=12;
        hb_set_master_transpose(delta);
    }
    return;
}
if(!strcmp(key,"borrowed_scale")){
    int selected=enum_index(parameter,BORROWED_SCALE_OPTS,4,hb_shared_borrowed_scale());
    hb_set_scale_exceptions(hb_shared_dominant_scale(),selected);
    return;
}
if(!strcmp(key,"dominant_minor_scale")){
    int selected=enum_index(!strcmp(parameter,"Off")?"None":!strcmp(parameter,"Parent / Minimal")?"Minimal":parameter,DOMINANT_SCALE_OPTS,8,hb_role_default(0,HB_P_DOMINANT_MINOR));
    for(int role=0;role<2;role++)hb_role_store(role,HB_P_DOMINANT_MINOR,selected);
    g_role_restored=1;return;
}
if(!strcmp(key,"dominant_scale")){
    int selected=enum_index(!strcmp(parameter,"Off")?"None":!strcmp(parameter,"Parent / Minimal")?"Minimal":parameter,DOMINANT_SCALE_OPTS,8,hb_shared_dominant_scale());
    hb_set_scale_exceptions(selected,hb_shared_borrowed_scale());
    return;
}
if(!strcmp(key,"chord_mode")){
    if(!strcmp(parameter,"Scale Root")||!strcmp(parameter,"Scale Degree"))parameter="Rendered Note Root"; /* Legacy presets/scripts. */
    int selected=enum_index(parameter,CP_CHORD_MODE,3,instance->player.config.mode);
    if(selected!=instance->player.config.mode){hb_prepare_role_change_flush(instance);hb_clear_instance_note_state(instance);instance->player.config.mode=selected;}
    return;
}
if(!strcmp(key,"chord_quality")){
    int selected=enum_index(parameter,CP_CHORD_QUALITY,13,instance->player.config.quality);
    instance->player.config.quality=selected; /* Applied at the next source onset. */
    return;
}
if(!strcmp(key,"chromatic_quality")){
    int selected=enum_index(parameter,CP_CHROMATIC_QUALITY,7,instance->player.config.chromatic_quality);
    instance->player.config.chromatic_quality=selected; /* Applied at the next source onset. */
    return;
}
if(!strcmp(key,"chord_form")){
    int selected=enum_index(parameter,CP_CHORD_FORM,HB_CP_FORMS,instance->player.config.size);
    instance->player.config.size=selected; /* Applied at the next source onset. */
    return;
}
if(!strcmp(key,"chord_inversion")){
    int selected=enum_index(parameter,CP_CHORD_INVERSION,9,instance->player.config.inversion);
    instance->player.config.inversion=selected; /* Applied at the next source onset. */
    return;
}
if(!strcmp(key,"chord_voicing")){
    int selected=enum_index(parameter,CP_CHORD_VOICING,4,instance->player.config.voicing);
    instance->player.config.voicing=selected; /* Applied at the next source onset. */
    return;
}
if(!strcmp(key,"arp_playback")){
    int selected=enum_index(parameter,CP_ARP_PLAYBACK,3,instance->player.config.playback);
    if(selected!=instance->player.config.playback){instance->player.config.playback=selected;
        instance->player.flushing=1;instance->player.running=0;
        instance->player.shuffle_count=instance->player.shuffle_position=0;
        for(int index=0;index<HB_CP_KEYS;index++)if(instance->player.keys[index].used){
            instance->player.keys[index].fresh=1;instance->player.keys[index].started=0;
        }}
    return;
}
if(!strcmp(key,"arp_clear_harmony")){instance->player.config.clear_harmony=!strcmp(parameter,"On")||!strcmp(parameter,"1");return;}
if(!strcmp(key,"arp_hold")){
    int selected=!strcmp(parameter,"Latch")?1:!strcmp(parameter,"Latch with Off")?2:enum_index(parameter,CP_ARP_HOLD,6,instance->player.config.latch);
    if(selected!=instance->player.config.latch){instance->player.config.latch=selected;}
    return;
}
if(!strcmp(key,"arp_start")){instance->player.config.start=enum_index(parameter,CP_ARP_START,7,instance->player.config.start);instance->player.anchor_pending=1;instance->player.shuffle_count=0;return;}
if(!strcmp(key,"arp_order")){
    int selected=enum_index(parameter,CP_ARP_ORDER,7,instance->player.config.order);
    if(selected!=instance->player.config.order){instance->player.config.order=selected;instance->player.shuffle_count=instance->player.shuffle_position=0;}
    return;
}
if(!strcmp(key,"arp_phase")){
    int selected=enum_index(parameter,CP_ARP_PHASE,3,instance->player.config.phase);
    if(selected!=instance->player.config.phase){instance->player.config.phase=selected;instance->player.running=0;}
    return;
}
if(!strcmp(key,"arp_note_phase")){
    int limit=hb_arp_phase_limit(instance->player.config.rate);
    int selected=hb_cp_clamp(parse_i(parameter,instance->player.config.note_phase),-limit,limit);
    if(selected!=instance->player.config.note_phase){instance->player.step+=instance->player.config.note_phase-selected;instance->player.config.note_phase=selected;}
    return;
}
if(!strcmp(key,"arp_rate")){
    int selected=enum_index(parameter,CP_ARP_RATE,18,instance->player.config.rate);
    if(selected!=instance->player.config.rate){double old_rate=hb_cp_step_beats(&instance->player);
        instance->player.config.rate=selected;
        double ratio=hb_cp_step_beats(&instance->player)/old_rate;
        if(instance->player.next_beat>instance->player.beat)instance->player.next_beat=instance->player.beat+(instance->player.next_beat-instance->player.beat)*ratio;
        if(instance->player.gate_beat>instance->player.beat)instance->player.gate_beat=instance->player.beat+(instance->player.gate_beat-instance->player.beat)*ratio;int limit=hb_arp_phase_limit(selected);instance->player.config.note_phase=hb_cp_clamp(instance->player.config.note_phase,-limit,limit);}
    return;
}
if(!strcmp(key,"arp_gate")){
    int selected=enum_index(parameter,CP_ARP_GATE,4,instance->player.config.gate);
    if(selected!=instance->player.config.gate){instance->player.config.gate=selected;}
    return;
}
if(!strcmp(key,"strum_spread")){
    int selected=parse_i(parameter,instance->player.config.spread);
    for(int index=0;index<9;index++)if(!strcmp(parameter,BUFFER_DIVISIONS[index]))selected=-index-1;
    if(selected< -9)selected=0;if(selected>1000)selected=1000;
    if(selected!=instance->player.config.spread){instance->player.config.spread=selected;}
    return;
}
if(!strcmp(key,"arp_clear")){if(!strcmp(parameter,"Clear")||parameter[0]=='1'){hb_prepare_role_change_flush(instance);hb_clear_instance_note_state(instance);}return;}
if(hb_cp_enabled(&instance->player)&&(!strcmp(key,"render_channel")||!strcmp(key,"source_channel"))){
    int next=!strcmp(key,"render_channel")?enum_index(parameter,RENDER_CH_OPTS,17,instance->render_channel+1)-1:enum_index(parameter,SOURCE_CH_OPTS,17,instance->source_channel+1)-1;
    int previous=!strcmp(key,"render_channel")?instance->render_channel:instance->source_channel;
    if(next!=previous){hb_prepare_role_change_flush(instance);hb_clear_instance_note_state(instance);}
}
if(!strcmp(key,"hb_movy_block")){
    unsigned long long block=0;int frames=0,rate=0;
    if(sscanf(parameter,"%llu,%d,%d",&block,&frames,&rate)==3&&frames>0&&rate>0&&
       (!g_conductor_block_ready||block!=g_conductor_block_id)){
        hb_movy_refresh();
        g_conductor_block_id=block;g_conductor_block_ready=1;
        hb_prepare_conductors(frames,rate);
    }
    return;
}
if(!strcmp(key,"hb_opening_preview")){
    size_t size=strlen(parameter);
    if(size<sizeof(instance->opening_preview))memcpy(instance->opening_preview,parameter,size+1);
    return;
}
if(!strcmp(key,"hb_movy_clip")){
    hb_movy_clip_t clip={0};clip.slot=-1;
    int track=-1,slot=-1;
    int fields=sscanf(parameter,"%llu,%llu,%llu,%llu,%u,%u,%u,%d,%d",
        &clip.tick,&clip.period,&clip.origin,&clip.revision,&clip.active,&clip.running,&clip.ppqn,&track,&slot);
    if(fields>=7&&clip.ppqn==96&&clip.active<=2&&clip.running<=1&&(!clip.active||clip.period>0)&&clip.period<=9007199254740991ULL){
        clip.present=1;
        if(fields==9&&slot>=0&&slot<128)clip.slot=slot;
        g_movy_clips[instance-g_pool]=clip;
        if(fields>=8&&track>=0&&track<16)instance->movy_track=track;
    }
    return;
}
if(!strcmp(key,"hb_movy_actions")){
    char *end=0;long note=strtol(parameter,&end,10);if(note<0||note>127||*end!=',')return;
    unsigned long long words[HB_MOTION_LANES+1]={0};
    int word_count=0;for(const char *scan=end+1;*scan;scan++)if(*scan==',')word_count++;word_count++;
    if(word_count!=17&&word_count!=34&&word_count!=38&&word_count!=HB_MOTION_LANES+1)return;
    int source_lanes=word_count-1;
    for(int lane=0;lane<=source_lanes;lane++){
        const char *start=end+1;words[lane]=strtoull(start,&end,10);
        if(end==start||(lane<source_lanes?*end!=',':*end!=0))return;
        if(lane==source_lanes&&((words[lane]&~HB_MO_SOURCE_MASK)||!hb_ar_alias_valid(words[lane])||(((words[lane]>>4)&7)|((words[lane]>>5)&8)|((words[lane]>>59)&16))>17))return;
        if(lane==source_lanes){unsigned token=(words[lane]>>43)&2047,code=token&63;
            if(token&&(!code||(code>63)||((code<16||code>=59)&&(token>>6))))return;
            if((words[lane]&(1ULL<<54))&&(code<16||code>=59))return;
        }
        if(lane==source_lanes&&(words[lane]&HB_MO_INTENT_MASK)){
            unsigned long long intent=words[lane];
            int mode=(intent>>21)&3,scale=(intent>>36)&31;
            int valid_scale=mode==3?((scale>0&&scale<HB_SCALE_COUNT)||scale==31):scale<=17;
            if(!(intent&(1ULL<<20))||((intent>>23)&15)>12||((intent>>27)&31)>16||((intent>>32)&15)>11||!valid_scale||((intent>>41)&3)>2)return;
        }
        if(lane==source_lanes&&(words[lane]&HB_MO_CADENCE_MASK)&&!hb_cadence_decode((unsigned)((words[lane]&HB_MO_CADENCE_MASK)>>13)))return;
        if(lane<source_lanes&&!(words[lane]&HB_MO_RECORDED)&&words[lane]>0xffffffffULL)return;
        if(lane<source_lanes&&(words[lane]&HB_MO_RECORDED)&&(((hb_mo_word_operation(words[lane])>HB_MO_MIXED_LAST&&hb_mo_word_operation(words[lane])!=HB_MO_LEADING_TONE&&hb_mo_word_operation(words[lane])!=HB_MO_UPPER_DIM)||(hb_mo_word_operation(words[lane])==HB_MO_AUTO_CHORD_REPEAT||hb_mo_word_operation(words[lane])==HB_MO_CHORD_STATE))||((words[lane]>>37)&15)>8))return;
    }
    if(source_lanes<HB_MOTION_LANES){
        unsigned long long marker=words[source_lanes]|(source_lanes<37?HB_MO_LEGACY_II:0);words[source_lanes]=0;
        for(int lane=0;source_lanes<37&&lane<source_lanes;lane++)if((words[lane]&HB_MO_RECORDED)&&hb_mo_word_operation(words[lane])==HB_MO_CHROM_ABOVE)
            words[lane]=(words[lane]&~((31ULL<<32)|(1ULL<<51)))|hb_mo_operation_word(HB_MO_TRITONE_V);
        words[HB_MOTION_LANES]=marker;
    }
    memcpy(instance->recorded_actions[note],words,sizeof(words));instance->recorded_action_valid[note]=1;return;
}
if(!strcmp(key,"hb_movy_input_approach")){
    int note=-1,shift=0,row=3;char trailing;int fields=sscanf(parameter,"%d,%d,%d%c",&note,&shift,&row,&trailing);
    if((fields==2||fields==3)&&note>=0&&note<128&&row>=0&&row<4&&note+shift>=0&&note+shift<128&&hb_ar_alias_valid(hb_ar_alias_word(shift))&&shift){
        instance->movy_pad_pending=note+1;instance->movy_pad_pending_shift=shift;instance->approach_rows.pending_row=row;
    }return;
}
if(!strcmp(key,"hb_movy_input_role")){
    int note=-1,degree=-1,target=-1;
    if(sscanf(parameter,"%d,%d,%d",&note,&degree,&target)==3&&note>=0&&note<128&&degree>=-1&&degree<7&&target>=-1&&target<128){
        instance->movy_input_degree[note]=(uint8_t)(degree+1);
        instance->movy_input_target[note]=(uint8_t)(target+1);
    }
    return;
}
if(!strcmp(key,"hb_movy_actions_clear")){int note=parse_i(parameter,-1);if(note>=0&&note<128)instance->recorded_action_valid[note]=0;return;}
if(!strcmp(key,"hb_movy_playback")){instance->movy_playback=parameter[0]=='1';return;}
if(!strcmp(key,"hb_movy_passthrough")){instance->movy_passthrough=parameter[0]=='1';return;}
if(!strcmp(key,"next_predict")){instance->next_predict=enum_index(parameter,NEXT_PREDICT_OPTS,2,instance->next_predict);if(!instance->next_predict){g_bus.next_shift_active=0;hb_effective_write(g_bus.observed_harmony);}else if(g_bus.next_model_locked)hb_next_apply_effective(hb_clip_playhead());return;}
if(!strcmp(key,"next_anti_buffer_ms")){
    int setting=instance->next_anti_buffer_ms;
    sscanf(parameter,"%d",&setting);
    for(int index=0;index<9;index++)if(!strcmp(parameter,BUFFER_DIVISIONS[index]))setting=-index-1;
    if(setting>=-9&&setting<=1000){instance->next_anti_buffer_ms=setting;instance->lookahead_restored=1;hb_next_apply_effective(hb_clip_playhead());}
    return;
}
if(!strcmp(key,"next_lookahead")){instance->lookahead_restored=1;instance->next_lookahead=hb_next_parse_lookahead(parameter,instance->next_lookahead);if(instance->next_predict&&g_bus.next_model_locked)hb_next_apply_effective(hb_clip_playhead());return;}
if(!strcmp(key,"next_reset")){if(parameter[0]=='1'||!strcmp(parameter,"Reset")){hb_timeline_reset_active();hb_clip_cache_evict_active();hb_next_reset_knowledge();hb_effective_write(g_bus.observed_harmony);}return;}
if(!strcmp(key,"live_press")){if(parameter[0]=='1')hb_receive_live_vouch(instance);return;}
if(!strcmp(key,"approach_below_pad")){if(parameter[0]=='1'){instance->approach_pad_armed=HB_APPROACH_CHROM_BELOW;}return;}
if(!strcmp(key,"approach_above_pad")){if(parameter[0]=='1'){instance->approach_pad_armed=HB_APPROACH_SCALE_ABOVE;}return;}
if(!strcmp(key,"approach_reset")){if(parameter[0]=='1'||!strcmp(parameter,"Reset")){instance->approach_control=HB_APPROACH_OFF;instance->approach_pad_armed=HB_APPROACH_OFF;instance->approach_below_held=0;instance->approach_above_held=0;hb_mo_gesture_reset(&instance->motion);instance->motion.enclosure=0;}return;}
if(!strcmp(key,"mod_scale_above")){int on=parameter[0]=='1'||!strcmp(parameter,"On");instance->approach_control=hb_approach_toggle(instance->approach_control,HB_APPROACH_SCALE_ABOVE,on);instance->approach_pad_armed=HB_APPROACH_OFF;return;}
if(!strcmp(key,"mod_chrom_below")){int on=parameter[0]=='1'||!strcmp(parameter,"On");instance->approach_control=hb_approach_toggle(instance->approach_control,HB_APPROACH_CHROM_BELOW,on);instance->approach_pad_armed=HB_APPROACH_OFF;return;}
if(!strcmp(key,"approach_scale_next")){if(parameter[0]=='1'||!strcmp(parameter,"Scale +"))instance->approach_pad_armed=instance->approach_pad_armed==HB_APPROACH_SCALE_ABOVE?HB_APPROACH_OFF:HB_APPROACH_SCALE_ABOVE;return;}
if(!strcmp(key,"approach_chrom_next")){if(parameter[0]=='1'||!strcmp(parameter,"Chrom -"))instance->approach_pad_armed=instance->approach_pad_armed==HB_APPROACH_CHROM_BELOW?HB_APPROACH_OFF:HB_APPROACH_CHROM_BELOW;return;}
if(!strcmp(key,"track_role")||!strcmp(key,"role")){
    int new_role=enum_index(parameter,ROLE_OPTS,4,instance->role);
    if(new_role!=instance->role){
        /* Role changes are a hard voice boundary. Flush what the OLD role
           actually sounded before clearing its ledgers, otherwise an ON can
           survive after the instance becomes Follower/Conductor/Off. */
        hb_prepare_role_change_flush(instance);
        hb_clear_instance_note_state(instance);
        memset(instance->source_seen,0,sizeof(instance->source_seen));
        instance->role=new_role;
        instance->resolved_source_channel=-1;
    }
    if(instance->role==0){if(!hb_load_clip_cache())hb_clear_clip_cache();}
}else if(!strcmp(key,"clip_slot")){g_bus.clip_slot=enum_index(parameter,CLIP_SLOT_OPTS,9,g_bus.clip_slot);if(instance->role==0){if(!hb_load_clip_cache())hb_clear_clip_cache();}}else if(!strcmp(key,"sensor_sources")){g_bus.sensor_sources=0;instance->dirty=1;instance->frames_since_change=0;}else if(!strcmp(key,"clip_context")){g_bus.clip_context=0;g_bus.sensor_sources=0;instance->dirty=1;instance->frames_since_change=0;}else if(!strcmp(key,"retrigger_held")){int previous=instance->retrigger_held;instance->retrigger_held=enum_index(parameter,RETRIGGER_OPTS,2,previous);if(!previous&&instance->retrigger_held)instance->follower_bus_seq=~__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);}else if(!strcmp(key,"follow_lookahead_ms")){int parsed=parse_i(parameter,instance->follow_lookahead_ms);if(parsed<0)parsed=0;if(parsed>100)parsed=100;instance->follow_lookahead_ms=parsed;}else if(!strcmp(key,"follower_root_policy")||!strcmp(key,"follower_source_policy")){hb_set_global_root_policy(enum_index(parameter,FOLLOWER_SOURCE_POLICY_OPTS,3,hb_global_root_policy()));}else if(!strcmp(key,"follower_explicit_root")||!strcmp(key,"follower_source_root")){hb_set_global_explicit_root(enum_index(parameter,PC_OPTS,12,hb_global_explicit_root()));}else if(!strcmp(key,"mode")){instance->mode=enum_index(parameter,MODE_OPTS,3,instance->mode);instance->follower_bus_seq=0;}else if(!strcmp(key,"content_map")){
    if(!strncmp(parameter,"In ",3))parameter+=3; /* legacy label alias */
    int value=instance->content_map;
    if(!strcmp(parameter,"Chord"))value=0;
    else if(!strcmp(parameter,"Scale"))value=1;
    else if(!strcmp(parameter,"Free"))value=2;
    else if(!strcmp(parameter,"135"))value=3;
    else if(!strcmp(parameter,"1357"))value=4;
    else if(!strcmp(parameter,"12357"))value=5;
    else if(!strcmp(parameter,"12356"))value=6;
    else if(!strcmp(parameter,"Non-Avoid"))value=7;
    else if(!strcmp(parameter,"123567"))value=8;
    instance->content_map=value;
}else if(!strcmp(key,"travel_map")){static const char *opts[]={"Relative","Closest Scale Tone","Upward","Closest Split","Downward","Direct","Closest Split Chromatic","None","Nearest Octave","Closest Chord Tone"};if(instance->travel_map==6)instance->chromatic_map=1;instance->travel_map=!strcmp(parameter,"Closest Split 2")?6:(!strcmp(parameter,"Closest")?1:enum_index(parameter,opts,10,instance->travel_map));}else if(!strcmp(key,"chromatic_map")){instance->chromatic_map=enum_index(parameter,MO_SWITCH,2,hb_chromatic_travel(instance));if(instance->travel_map==6)instance->travel_map=3;}else if(!strcmp(key,"split_map")){static const char *opts[]={"Harm. / Out","135 / 2467","1357 / 246","Act. / Out"};instance->follower_split_map=enum_index(parameter,opts,4,instance->follower_split_map);}else if(!strcmp(key,"approach")){instance->approach_control=enum_index(parameter,APPROACH_OPTS,3,HB_APPROACH_OFF);instance->approach_pad_armed=HB_APPROACH_OFF;}else if(!strcmp(key,"approach_mode")){instance->approach_mode=0;instance->approach_pad_armed=HB_APPROACH_OFF;instance->approach_below_held=0;instance->approach_above_held=0;}else if(!strcmp(key,"map_target"))instance->map_target=enum_index(parameter,MAP_TARGET_OPTS,2,instance->map_target);else if(!strcmp(key,"source_channel")){int idx=enum_index(parameter,SOURCE_CH_OPTS,17,instance->source_channel+1);instance->source_channel=idx-1;instance->resolved_source_channel=-1;}else if(!strcmp(key,"render_channel")){hb_mo_panic(&instance->motion_render);hb_motion_flush_render(instance);hb_receiver_remove_source(instance);int idx=enum_index(parameter,RENDER_CH_OPTS,17,instance->render_channel+1);instance->render_channel=idx-1;}else if(!strcmp(key,"quant_timing")){/* follower-render timing only */g_bus.quant_timing=enum_index(parameter,QUANT_GRID_OPTS,8,g_bus.quant_timing);g_quant_restored=1;}else if(!strcmp(key,"chord_timing")){/* follower-render timing only */g_bus.chord_timing=!strcmp(parameter,"Free")?0:enum_index(parameter,TIMING_OPTS,8,g_bus.chord_timing);g_bus.chord_timescale=0;for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used){int limit=hb_arp_phase_limit(g_pool[index].player.config.rate);g_pool[index].player.config.note_phase=hb_cp_clamp(g_pool[index].player.config.note_phase,-limit,limit);}}else if(!strcmp(key,"anticipation")){/* follower-render timing only */g_bus.anticipation=enum_index(parameter,ANTICIPATION_OPTS,6,g_bus.anticipation);}else if(!strcmp(key,"boundary_buffer_ms")){/* follower-render capture window only */int parsed=parse_i(parameter,instance->boundary_buffer_ms);for(int division=0;division<9;division++)if(!strcmp(parameter,BUFFER_DIVISIONS[division]))parsed=-division-1;if(parsed< -9)parsed=0;if(parsed>1000)parsed=1000;instance->boundary_buffer_ms=parsed;g_buffer_restored=1;}else if(!strcmp(key,"analysis_release_ms")){int parsed=parse_i(parameter,g_bus.analysis_release_ms);if(parsed<0)parsed=0;if(parsed>500)parsed=500;g_bus.analysis_release_ms=parsed;}else if(!strcmp(key,"follower_scale")){hb_set_shared_follower_scale(enum_index(parameter,FOLLOWER_SCALE_OPTS,HB_SCALE_COUNT,hb_shared_follower_scale()));}else if(!strcmp(key,"context")){g_bus.context=enum_index(parameter,CONTEXT_OPTS,7,g_bus.context);g_bus.stability=hb_context_to_legacy_stability(g_bus.context);}else if(!strcmp(key,"chord_timescale"))g_bus.chord_timescale=enum_index(parameter,TIMESCALE_OPTS,6,g_bus.chord_timescale);else if(!strcmp(key,"stability"))g_bus.stability=enum_index(parameter,STABILITY_OPTS,3,g_bus.stability);else if(!strcmp(key,"accidentals")){int previous=g_bus.accidentals;g_bus.accidentals=enum_index(parameter,ACCIDENTAL_OPTS,7,g_bus.accidentals);if(g_bus.accidentals==0&&previous!=0)g_bus.auto_spell_locked=0;}else if(!strcmp(key,"root_policy"))g_bus.global_root_policy=enum_index(parameter,POLICY_OPTS,3,g_bus.global_root_policy);else if(!strcmp(key,"explicit_root"))g_bus.global_explicit_root=enum_index(parameter,PC_OPTS,12,g_bus.global_explicit_root);else if(!strcmp(key,"input_root"))g_bus.global_input_root=enum_index(parameter,PC_OPTS,12,g_bus.global_input_root);else if(!strcmp(key,"transpose")){int parsed=parse_i(parameter,g_bus.global_transpose);if(parsed<-24)parsed=-24;if(parsed>24)parsed=24;hb_set_master_transpose(parsed);}else if(!strcmp(key,"window_ms")){int parsed=parse_i(parameter,g_bus.inference_window_ms);if(parsed<0)parsed=0;if(parsed>500)parsed=500;g_bus.inference_window_ms=parsed;}else if(!strcmp(key,"state"))hb_restore_state(instance,parameter);}
static void hb_restore_state(Inst *instance,const char *state){
    if(!instance||!state)return;
    int values[25];for(int i=0;i<25;i++)values[i]=-999;
    int parsed=sscanf(state,"hb16,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20],&values[21],&values[22],&values[23],&values[24]);
    int is_hb16=(parsed==25);
    if(!is_hb16)parsed=sscanf(state,"hb15,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20],&values[21],&values[22],&values[23],&values[24]);
    int is_hb15=(!is_hb16&&parsed==25);
    if(!is_hb16&&!is_hb15)parsed=sscanf(state,"hb14,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20],&values[21],&values[22],&values[23]);
    int is_hb14=(!is_hb16&&!is_hb15&&parsed==24);
    if(!is_hb16&&!is_hb15&&!is_hb14)parsed=sscanf(state,"hb13,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20],&values[21],&values[22],&values[23]);
    int is_hb13=(!is_hb15&&!is_hb14&&parsed==24);
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13)parsed=sscanf(state,"hb12,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20],&values[21],&values[22]);
    int is_hb12=(!is_hb13&&parsed==23);
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12)parsed=sscanf(state,"hb11,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20]);
    int is_hb11=(!is_hb13&&!is_hb12&&parsed==21);
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11)parsed=sscanf(state,"hb10,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],&values[18],&values[19]);
    int is_hb10=(!is_hb13&&!is_hb12&&!is_hb11&&parsed==20);
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10)parsed=sscanf(state,"hb9,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],&values[18]);
    int is_hb9=(!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&parsed==19);
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9)parsed=sscanf(state,"hb8,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17]);
    if(parsed==18){
        /* Follower root policy/root are global live state. Per-track restore
           must not overwrite them; otherwise changing tracks makes "last
           visited track wins". */
    }
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=18){
        parsed=sscanf(state,"hb7,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14],&values[15]);
    }
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=16&&parsed!=18){
        parsed=sscanf(state,"hb6,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14]);
    }
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=15&&parsed!=16&&parsed!=18){
        parsed=sscanf(state,"hb5,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14],&values[15]);
    }
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=16&&parsed!=15&&parsed!=18){
        parsed=sscanf(state,"hb4,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14]);
    }
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=15&&parsed!=16&&parsed!=18){
        parsed=sscanf(state,"hb3,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],&values[12]);
    }
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=13&&parsed!=15&&parsed!=16&&parsed!=18){
        parsed=sscanf(state,"hb2,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11]);
    }
    if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=12&&parsed!=13&&parsed!=15&&parsed!=16&&parsed!=18){
        parsed=sscanf(state,"hb1,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10]);
        if(parsed!=11)return;
    }
    if(((values[0]>=0&&values[0]<=3&&values[0]!=instance->role)||
        (values[11]>=-1&&values[11]<16&&values[11]!=instance->render_channel)||
        (values[12]>=-1&&values[12]<16&&values[12]!=instance->source_channel))){
        hb_prepare_role_change_flush(instance);hb_clear_instance_note_state(instance);
    }
    if(values[0]>=0&&values[0]<=3)instance->role=values[0];
    if(values[1]>=0&&values[1]<=2)instance->mode=values[1];
    instance->map_target=0;
    if(values[3]>=0&&values[3]<=500)instance->window_ms=values[3];
    if(values[0]==0&&!is_hb10&&!is_hb9){
        if(values[4]>=0&&values[4]<=2)g_bus.global_root_policy=values[4];
        if(values[5]>=0&&values[5]<12)g_bus.global_explicit_root=values[5];
        if(values[6]>=0&&values[6]<12)g_bus.global_input_root=values[6];
        if(values[7]>=-24&&values[7]<=24)g_bus.global_transpose=values[7];
        if(values[8]>=0&&values[8]<=5)g_bus.chord_timescale=values[8];
        if(values[9]>=0&&values[9]<=2)g_bus.stability=values[9];
        if(values[10]>=0&&values[10]<=6)g_bus.accidentals=values[10];
        if(parsed>=15){
            if(values[13]>=0&&values[13]<13)g_bus.chord_timing=values[13];
            g_bus.context=0; /* Context is fixed to Live. */
            g_bus.clip_context=0;
            g_bus.sensor_sources=0;
            g_bus.chord_timescale=hb_timing_to_legacy_timescale(g_bus.chord_timing);
            g_bus.stability=hb_context_to_legacy_stability(g_bus.context);
        }else{
            /* Migrate old settings into the new two-control model. */
            if(g_bus.chord_timescale==0)g_bus.chord_timing=0;
            else if(g_bus.chord_timescale==1)g_bus.chord_timing=1;
            else if(g_bus.chord_timescale==2)g_bus.chord_timing=3;
            else if(g_bus.chord_timescale==3)g_bus.chord_timing=5;
            else if(g_bus.chord_timescale==4)g_bus.chord_timing=7;
            else g_bus.chord_timing=9;
            g_bus.context=g_bus.stability==0?1:(g_bus.stability==1?3:5);
        }
    }
    if((is_hb16||is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10||is_hb9||parsed>=12)&&values[11]>=-1&&values[11]<16)instance->render_channel=values[11];
    if((is_hb16||is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10||is_hb9||parsed==13||parsed>=15)&&values[12]>=-1&&values[12]<16){
        instance->source_channel=values[12];
        /* Restoring opaque state must never leave a cached owning-track
           channel from a previous UI/instance lifetime. Re-resolve direct
           MIDI routing from the current track tag/fallback. */
        instance->resolved_source_channel=-1;
    }
    if((is_hb16||is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10||is_hb9||parsed>=18)&&values[16]>=0&&values[16]<=100)instance->follow_lookahead_ms=values[16];
    if((is_hb16||is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10||is_hb9||parsed>=18)&&values[17]>=0&&values[17]<=1)instance->retrigger_held=values[17];
    /* Content/travel/split stay per instance. Input scale is restored once
       globally; track selection must never change the physical keyboard. */
    instance->next_touch_mask=0;
    instance->chromatic_map=0;
    const char *chromatic_state=strstr(state,";ct1,");int chromatic_value;
    if(chromatic_state&&sscanf(chromatic_state,";ct1,%d",&chromatic_value)==1&&(chromatic_value==0||chromatic_value==1))instance->chromatic_map=chromatic_value;
    memcpy(instance->touch_lanes,g_touch_lanes,sizeof(g_touch_lanes));
    if(!g_touch_restored){
        const char *touch_state=strstr(state,";ft2,");int lanes[9];memcpy(lanes,g_touch_lanes,sizeof(lanes));
        int count=touch_state?sscanf(touch_state,";ft2,%d,%d,%d,%d,%d,%d,%d,%d,%d",&lanes[0],&lanes[1],&lanes[2],&lanes[3],&lanes[4],&lanes[5],&lanes[6],&lanes[7],&lanes[8]):0;
        if(!touch_state){touch_state=strstr(state,";ft1,");if(touch_state)count=sscanf(touch_state,";ft1,%d,%d,%d,%d,%d,%d,%d,%d",&lanes[0],&lanes[1],&lanes[2],&lanes[3],&lanes[4],&lanes[5],&lanes[6],&lanes[7]);}
        int valid=count==8||count==9;for(int k=0;k<9;k++)if(lanes[k]<1||lanes[k]>16)valid=0;
        if(valid)hb_touch_publish(lanes);
    }
    instance->follow_lookahead_ms=0;
    if((is_hb16||is_hb15||is_hb14)){
        if(values[8]>=0&&values[8]<4)instance->follower_split_map=values[8];
        /* First restored copy wins; later UI rehydration cannot undo a live global edit.
           New states serialize the same shared value from every instance. */
        if(values[19]>=-9&&values[19]<=1000){instance->boundary_buffer_ms=values[19];g_buffer_restored=1;}
        if(values[21]>=0&&values[21]<9)instance->content_map=values[21];
        if(values[22]>=0&&values[22]<10)instance->travel_map=values[22];
        /* Old conductor defaults must not win over the first saved follower.
           Once restored/edited, stale track copies cannot change the input key. */
        if(!g_scale_restored&&values[23]>=0&&values[23]<HB_SCALE_COUNT&&(values[0]==1||values[23]>0))
            hb_set_shared_follower_scale(values[23]);
        if(!g_quant_restored&&values[24]>=0&&values[24]<8){g_bus.quant_timing=values[24];g_quant_restored=1;}
    }
    if((is_hb16||is_hb15||is_hb14)&&values[0]==0&&!g_follower_globals_restored){
        /* Only source-root semantics remain global. Restore their canonical
           conductor copy once; follower voice settings above remain local. */
        if(values[4]>=0&&values[4]<3)hb_set_global_root_policy(values[4]);
        if(values[5]>=0&&values[5]<12)hb_set_global_explicit_root(values[5]);
        g_follower_globals_restored=1;
    }
    if((is_hb16||is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10||is_hb9)&&values[0]==0&&!instance->global_timing_restored){
        /* Restore shared timing only once for the conductor instance. Schwung
           may reapply opaque module state while navigating the UI; allowing
           that stale copy to overwrite live globals made Chord Grid snap back. */
        g_bus.inference_window_ms=25;
        if(values[13]>=0&&values[13]<8)g_bus.chord_timing=values[13];
        g_bus.context=0;
        if(values[18]>=0&&values[18]<6)g_bus.anticipation=values[18];
        if((is_hb16||is_hb15||is_hb14||is_hb13||is_hb12||is_hb11)&&values[20]>=0&&values[20]<=500)g_bus.analysis_release_ms=values[20];
        instance->global_timing_restored=1;
    }else if(!is_hb16&&!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed>=15&&values[0]==0&&!instance->global_timing_restored){
        /* Migrate old combined Timing choices: Ant variants collapse to the
           same Grid; anticipation defaults Off until explicitly selected. */
        int old_timing=values[13];
        if(old_timing<=0)g_bus.chord_timing=0;
        else if(old_timing<=2)g_bus.chord_timing=1;
        else if(old_timing<=4)g_bus.chord_timing=2;
        else if(old_timing<=6)g_bus.chord_timing=3;
        else if(old_timing<=8)g_bus.chord_timing=4;
        else if(old_timing<=10)g_bus.chord_timing=5;
        else g_bus.chord_timing=6;
        g_bus.inference_window_ms=25; /* Internal conductor clustering tolerance. */
        g_bus.anticipation=0;
        instance->global_timing_restored=1;
    }
    g_bus.chord_timescale=0;
    g_bus.context=0; /* Context is fixed to Live regardless of legacy state. */
    hb_mo_repeat_cancel(&instance->motion_local,&instance->motion,1);
    hb_mo_repeat_cancel(&instance->motion_render,&instance->motion,1);hb_motion_flush_render(instance);
    if(!g_motion_settings_restored){
        hb_mo_restore(&instance->motion,state);
        if(strstr(state,";m"))hb_motion_publish_settings(instance);
    }else {int host=instance->motion.host_capabilities;hb_mo_defaults(&instance->motion);instance->motion.host_capabilities=host;hb_motion_copy_settings(&instance->motion,&g_motion_settings);}
    instance->motion_local.enclosure_revision=0;instance->motion_local.performance_valid=0;
    instance->motion_render.enclosure_revision=0;instance->motion_render.performance_valid=0;
    if(!g_humanize_restored){
        int timing=0,velocity=0,gate=0;const char *human=strstr(state,";hu1,");
        if(!human||(sscanf(human,";hu1,%d,%d,%d",&timing,&velocity,&gate)==3&&
            timing>=0&&timing<=30&&velocity>=0&&velocity<=30&&gate>=0&&gate<=30)){
            g_humanize[0]=timing;g_humanize[1]=velocity;g_humanize[2]=gate;g_humanize_restored=1;
        }
    }
    const char *pad_suffix=strstr(state,";pd1,");
    const char *play_color_suffix=strstr(state,";pp1,");int restored_play=-1;
    int has_pad_state=pad_suffix||play_color_suffix||strstr(state,";pc2,")||strstr(state,";pt1,")||strstr(state,";pb1,")||strstr(state,";pf1,")||strstr(state,";pnp1,");
    int restored_pads[5],restore_colors=!g_pad_restored&&has_pad_state;
    if(restore_colors){
        /* Older saves omitted their defaults. Bare factory presets instead
           inherit the new defaults and do not claim the global restore guard. */
        const int legacy_defaults[5]={2,3,3,4,2};
        memcpy(g_pad_settings,legacy_defaults,sizeof(legacy_defaults));
        g_pad_play_color=8;g_pad_both_color=0;g_pad_tonic_color=9;
    }
    if(restore_colors){
        const char *shade_suffix=strstr(state,";pas1,");int restored_shade=0;
        g_pad_adjacent_shading=shade_suffix&&sscanf(shade_suffix,";pas1,%d",&restored_shade)==1&&restored_shade==1;
        const char *pulse_suffix=strstr(state,";pnp1,");int restored_pulse=0;
        g_pad_next_pulse=0;
        if(pulse_suffix&&sscanf(pulse_suffix,";pnp1,%d",&restored_pulse)==1&&restored_pulse>=0&&restored_pulse<12)g_pad_next_pulse=restored_pulse;
    }
    const char *form_suffix=strstr(state,";pf1,");int restored_form=0;
    if(restore_colors){
        g_pad_chord_form=0;
        if(form_suffix&&sscanf(form_suffix,";pf1,%d",&restored_form)==1&&restored_form>=0&&restored_form<HB_CP_FORMS)g_pad_chord_form=restored_form;
        g_pad_next_chord_form=g_pad_chord_form;
        const char *next_form_suffix=strstr(state,";pnf1,");
        if(next_form_suffix&&sscanf(next_form_suffix,";pnf1,%d",&restored_form)==1&&restored_form>=0&&restored_form<HB_CP_FORMS)g_pad_next_chord_form=restored_form;
    }
    const char *tonic_suffix=strstr(state,";pt1,");int restored_tonic=-1;
    if(restore_colors&&tonic_suffix&&sscanf(tonic_suffix,";pt1,%d",&restored_tonic)==1&&restored_tonic>=0&&restored_tonic<10){g_pad_tonic_color=restored_tonic;g_pad_restored=1;}
    const char *both_suffix=strstr(state,";pb1,");int restored_both=-1;
    if(restore_colors&&both_suffix&&sscanf(both_suffix,";pb1,%d",&restored_both)==1&&restored_both>=0&&restored_both<10)g_pad_both_color=restored_both;
    const char *color_suffix=strstr(state,";pc2,");int restored_color=-1;
    if(color_suffix)sscanf(color_suffix,";pc2,%d",&restored_color);
    if(restore_colors&&pad_suffix&&sscanf(pad_suffix,";pd1,%d,%d,%d,%d,%d",&restored_pads[0],&restored_pads[1],&restored_pads[2],&restored_pads[3],&restored_pads[4])==5){
        int valid=1;for(int index=0;index<5;index++)if(restored_pads[index]<0||restored_pads[index]>=PAD_LIMITS[index])valid=0;
        if(valid){memcpy(g_pad_settings,restored_pads,sizeof(restored_pads));if(g_pad_settings[0]==0)g_pad_settings[0]=2;g_pad_restored=1;}
    }
    if(restore_colors){
        /* Keep the visible color of old Standard/Effective presets. New states
           have pp1 even at its default, so old pc2 never remigrates them. */
        if(!play_color_suffix&&!pad_suffix)g_pad_settings[2]=0;
        if(!play_color_suffix&&!tonic_suffix)g_pad_tonic_color=8;
        if(!play_color_suffix&&(g_pad_settings[0]==0||g_pad_settings[0]==2))
            g_pad_settings[3]=restored_color>=0&&restored_color<9?restored_color:8;
        if(play_color_suffix&&sscanf(play_color_suffix,";pp1,%d",&restored_play)==1&&restored_play>=0&&restored_play<12)
            g_pad_play_color=restored_play;
        g_pad_restored=1;
    }
    if(!g_scale_exceptions_restored){
        int dominant=0,borrowed=0;
        const char *shared_suffix=strstr(state,";ss1,");
        if(shared_suffix){
            if(sscanf(shared_suffix,";ss1,%d,%d",&dominant,&borrowed)==2&&dominant>=0&&dominant<8&&borrowed>=0&&borrowed<4){
                /* Modern role defaults are restored separately; the legacy
                   suffix must not overwrite an already selected role family. */
                if(strstr(state,";rp2,")||strstr(state,";rp1,")){
                    hb_store_scale_exceptions(dominant,borrowed);g_scale_exceptions_restored=1;
                }else hb_set_scale_exceptions(dominant,borrowed);
            }
        }else{
            /* Legacy per-track presets: first non-default choice seeds the
               global setting. Defaults on earlier tracks cannot hide it. */
            const char *legacy=strstr(state,";ds1,");
            if(legacy&&sscanf(legacy,";ds1,%d",&dominant)==1&&dominant>0&&dominant<8)
                hb_set_scale_exceptions(dominant,0);
        }
    }
    hb_fp_config play={0};
    const char *play_suffix=strstr(state,";fp1,");
    hb_fp_config parsed_play={0};
    if(play_suffix&&sscanf(play_suffix,";fp1,%d,%d,%d,%d,%d,%d,%d",&parsed_play.rotate,&parsed_play.wrap,&parsed_play.mirror,&parsed_play.octave,&parsed_play.range,&parsed_play.scope,&parsed_play.bypass)==7&&
       parsed_play.rotate>=-24&&parsed_play.rotate<=24&&parsed_play.wrap>=0&&parsed_play.wrap<=1&&
       parsed_play.mirror>=0&&parsed_play.mirror<=1&&parsed_play.octave>=-3&&parsed_play.octave<=3&&
       parsed_play.range>=0&&parsed_play.range<=3&&parsed_play.scope>=0&&parsed_play.scope<=2&&parsed_play.bypass>=0&&parsed_play.bypass<=1)play=parsed_play;
    instance->play=play;instance->play_revision++;
    hb_cp_config config;hb_cp_defaults(&config);
    const char *suffix=strstr(state,";cp1,");
    if(suffix){
        hb_cp_config parsed_config;hb_cp_defaults(&parsed_config);
        int parsed_count=sscanf(suffix,";cp1,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",&parsed_config.mode,&parsed_config.size,&parsed_config.inversion,&parsed_config.voicing,&parsed_config.playback,&parsed_config.latch,&parsed_config.order,&parsed_config.rate,&parsed_config.gate,&parsed_config.spread);
        if(parsed_count==10&&parsed_config.mode>=0&&parsed_config.mode<3&&parsed_config.size>=0&&parsed_config.size<HB_CP_FORMS&&
           parsed_config.inversion>=0&&parsed_config.inversion<9&&parsed_config.voicing>=0&&parsed_config.voicing<4&&
           parsed_config.playback>=0&&parsed_config.playback<3&&parsed_config.latch>=0&&parsed_config.latch<6&&
           parsed_config.order>=0&&parsed_config.order<7&&parsed_config.rate>=0&&parsed_config.rate<18&&
           parsed_config.gate>=0&&parsed_config.gate<4&&parsed_config.spread>=-9&&parsed_config.spread<=1000)config=parsed_config;
    }
    instance->player.release_ms=instance->player.release_follow_harmony=0;
    instance->player.release_held=instance->player.release_armed=instance->player.release_latched=instance->player.release_used=instance->player.release_turned=0;
    const char *release_suffix=strstr(state,";hr1,");int release_ms,release_follow=0;
    if(release_suffix&&sscanf(release_suffix,";hr1,%d,%d",&release_ms,&release_follow)>=1){instance->player.release_ms=hb_cp_clamp(release_ms,-20,1000);instance->player.release_follow_harmony=release_follow==1;}
    instance->render_velocity_gain=10000;
    const char *velocity_suffix=strstr(state,";rv1,");int render_gain=10000;
    if(velocity_suffix&&sscanf(velocity_suffix,";rv1,%d",&render_gain)==1&&render_gain>=0&&render_gain<=40000)
        instance->render_velocity_gain=render_gain;
    const char *lookahead_suffix=strstr(state,";la1,");
    int lookahead=0,anti_buffer=25;
    if(!instance->lookahead_restored&&lookahead_suffix&&sscanf(lookahead_suffix,";la1,%d,%d",&lookahead,&anti_buffer)==2&&
       lookahead>=0&&lookahead<HB_LOOKAHEAD_COUNT&&anti_buffer>=-9&&anti_buffer<=1000){
        instance->next_lookahead=lookahead;instance->next_anti_buffer_ms=anti_buffer;instance->lookahead_restored=1;
        hb_next_apply_effective(hb_clip_playhead());
    }
    const char *start_suffix=strstr(state,";as1,");
    int arp_start=5;
    if(start_suffix)sscanf(start_suffix,";as1,%d",&arp_start);
    config.start=hb_cp_clamp(arp_start,0,6);instance->player.config.start=config.start;
    const char *clear_suffix=strstr(state,";ac1,");
    config.clear_harmony=clear_suffix&&clear_suffix[5]=='1';
    instance->player.config.clear_harmony=config.clear_harmony;
    const char *phase_suffix=strstr(state,";ph1,");
    int phase=0;
    if(phase_suffix&&sscanf(phase_suffix,";ph1,%d",&phase)==1&&phase>=0&&phase<3)config.phase=phase;
    const char *note_phase_suffix=strstr(state,";np1,");
    int note_phase=0;
    int phase_limit=hb_arp_phase_limit(config.rate);
    if(note_phase_suffix&&sscanf(note_phase_suffix,";np1,%d",&note_phase)==1)
        config.note_phase=hb_cp_clamp(note_phase,-phase_limit,phase_limit);
    const char *quality_suffix=strstr(state,";cq1,");
    int quality=0,chromatic=0;
    if(quality_suffix&&sscanf(quality_suffix,";cq1,%d,%d",&quality,&chromatic)==2&&
       quality>=0&&quality<13&&chromatic>=0&&chromatic<7){
        config.quality=quality;config.chromatic_quality=chromatic;
    }
    if(config.note_phase!=instance->player.config.note_phase||config.phase!=instance->player.config.phase||config.mode!=instance->player.config.mode||config.size!=instance->player.config.size||config.inversion!=instance->player.config.inversion||config.voicing!=instance->player.config.voicing||config.playback!=instance->player.config.playback||config.latch!=instance->player.config.latch||config.order!=instance->player.config.order||config.rate!=instance->player.config.rate||config.gate!=instance->player.config.gate||config.spread!=instance->player.config.spread||config.quality!=instance->player.config.quality||config.chromatic_quality!=instance->player.config.chromatic_quality){
        hb_prepare_role_change_flush(instance);hb_clear_instance_note_state(instance);instance->player.config=config;
    }
    if(instance->role==0){if(!hb_load_clip_cache())hb_clear_clip_cache();}
}
/* A row has ONE owner. Never pair globally deduplicated notes with roles
   enumerated in instance order. Reads inspect captured voices, not a new map. */
static int hb_follower_path(Inst *instance,const char *key,char *buffer,int length){
    int global=0,ordinal=0,field=0;
    if(sscanf(key,"fpath_%d_%d_%d",&global,&ordinal,&field)!=3)return -2;
    if(global<0||global>1||ordinal<0||ordinal>3||field<0||field>3)return -2;
    const Inst *owner=instance;
    int raw=global?hb_nth_aggregate_follower_observation(ordinal,&owner):hb_nth_local_follower_note(instance,ordinal);
    if(raw<0||!owner)return snprintf(buffer,(size_t)length,"--");
    const hb_follower_voice *plain=hb_follower_display_voice(owner,raw);
    const hb_cp_key *chord_voice=0;
    if(!plain)for(int index=0;index<HB_CP_KEYS;index++){
        const hb_cp_key *voice=&owner->player.keys[index];
        if(voice->used&&voice->source==raw&&(!chord_voice||!voice->playback_origin))chord_voice=voice;
    }
    hb_input_intent intent=plain?plain->input:chord_voice?
        (hb_input_intent){chord_voice->input_degree,chord_voice->input_target,chord_voice->input_shift,chord_voice->input_token}:hb_input_get((Inst*)owner,raw);
    hb_harmony_t harmony=plain?plain->harmony:owner->follower_path_harmony[raw];
    int piano=intent.shift;
    if(field==0){char name[12];int shown=piano?raw+piano-1:raw;while(shown<0)shown+=12;
        return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(shown,harmony,name,sizeof(name)));}
    int input_root=0;
    int have_root=hb_resolve_follower_reference_root((Inst*)owner,&input_root);
    uint16_t input=have_root?hb_follower_input_scale((Inst*)owner,input_root):0;
    int chromatic=intent.target||(input&&!(input&(1u<<mod12(raw))));
    int approach=piano||(chromatic&&hb_chromatic_travel(owner)&&raw<127);
    if(field==1){
        if(piano)return snprintf(buffer,(size_t)length,"%s-1",hb_follower_degree_role_for_note((Inst*)owner,raw+piano));
        if(approach){
            int next=intent.target?intent.target-1:raw+1;while(next<127&&!(input&(1u<<mod12(next))))next++;
            if(input&(1u<<mod12(next)))return snprintf(buffer,(size_t)length,"%s-1",hb_follower_degree_role_for_note((Inst*)owner,next));
        }
        const char *role=intent.degree?hb_role_name_for_degree(intent.degree-1):have_root?
            hb_role_name_for_degree(hb_source_degree_from_parent_scale(mod12(raw-input_root),input_root,input)):"--";
        return snprintf(buffer,(size_t)length,"%s%s",role,chromatic?"*":"");
    }
    int pitches[HB_CP_VOICES],count=0;
    if(plain)pitches[count++]=plain->pitch;
    else if(chord_voice)for(int note=0;note<chord_voice->count&&count<HB_CP_VOICES;note++)pitches[count++]=chord_voice->notes[note];
    if(!count&&owner->mapped[raw]>=0)pitches[count++]=owner->mapped[raw];
    if(!count)return snprintf(buffer,(size_t)length,"--"); /* queued, not rendered */
    int rendered_approach=approach&&count==1&&hb_cp_mode(&owner->player)==0;
    /* Operation owners retain the actual emitted pitch; do not reevaluate a
       random/probabilistic lane while drawing a diagnostic. */
    for(int note=0;note<count;note++)for(int index=HB_MOTION_OWNERS-1;index>=0;index--){
        const hb_motion_owner *motion=&owner->motion_local.owners[index];
        int identity=plain?hb_fv_identity(raw,plain->channel,plain->origin):0;
        if(motion->used&&motion->source==pitches[note]&&motion->input_owner==identity&&!motion->generated){
            if(pitches[note]!=motion->pitch)rendered_approach=0;
            pitches[note]=motion->pitch;break;
        }
    }
    char name[24];
    const char *display;
    if(field==2&&harmony.valid){
        uint16_t output=hb_follower_scale_target((Inst*)owner,harmony).pitch_mask;
        int pitch=pitches[0],target=rendered_approach?pitch+1:pitch;
        if(output&(1u<<mod12(target))){
            int degree=hb_source_degree_from_parent_scale(mod12(target-harmony.root_pc),harmony.root_pc,output);
            snprintf(name,sizeof(name),"%s%s",hb_role_name_for_degree(degree),rendered_approach?"-1":"");
            display=name;
        }else display=hb_role_name_for_interval(mod12(pitch-harmony.root_pc));
    }else display=field==2?"--":hb_note_name_with_octave(pitches[0],harmony,name,sizeof(name));
    return count>1?snprintf(buffer,(size_t)length,"%s+%d",display,count-1):snprintf(buffer,(size_t)length,"%s",display);
}
static void hb_capture_harmony_display(Inst *instance){
    hb_harmony_t rendered=hb_render_harmony(instance);
    hb_harmony_t detected=hb_transpose_harmony(g_bus.observed_harmony,-g_bus.global_transpose);
    hb_harmony_t selected=hb_transpose_harmony(rendered,-g_bus.global_transpose);
    unsigned seen=0;int count=0,used=0;
    uint8_t notes[64];int note_count=hb_observed_notes(0,notes,64);
    for(int ordinal=0;ordinal<note_count;ordinal++){
        int note=notes[ordinal];
        int pc=mod12(note);if(seen&(1u<<pc))continue;seen|=1u<<pc;
        if(count==4){snprintf(instance->harmony_display[0]+used,48-used," +");break;}
        used+=snprintf(instance->harmony_display[0]+used,48-used,"%s%s",count?" ":"",hb_pc_display(pc,detected));
        count++;
    }
    if(!count)snprintf(instance->harmony_display[0],48,"--");
    hb_format_harmony(instance->harmony_display[1],48,detected);
    hb_format_harmony(instance->harmony_display[2],48,selected);
    hb_format_harmony(instance->harmony_display[3],48,rendered);
    instance->harmony_display_valid=1;
}
static void hb_capture_follower_display(Inst *instance){
    for(int index=0;index<8;index++){
        char key[24];snprintf(key,sizeof(key),"fpath_0_%d_%d",index/4,index%4);
        hb_follower_path(instance,key,instance->follower_display[index],24);
    }
    instance->follower_display_valid=1;
}
/* One-based bar:beat within the conductor cycle; 4 quarter notes per bar,
   matching the timing controls. Keep hundredths so off-grid events are visible. */
static int hb_timing_position(char *buffer,int length,double phase){
    long hundredths=(long)(phase*100.0+0.5);
    return snprintf(buffer,(size_t)length,"%ld:%ld.%02ld",hundredths/400+1,
        (hundredths%400)/100+1,hundredths%100);
}
/* Return actual registered changes. The first entry
   of an incomplete pass is an initial observation, not a known transition.
   Once locked, compare cyclic predecessors, including the loop wrap. */
static int hb_timing_events(hb_loop_harmony_event_t *events){
    if(g_movy_blocked)return 0;
    int locked=g_bus.next_model_locked;
    int count=locked?g_bus.next_model_count:g_bus.next_learning_count;
    hb_loop_harmony_event_t *source=locked?g_bus.next_model:g_bus.next_learning;
    int used=0;
    for(int index=locked?0:1;index<count;index++){
        int previous=(index+count-1)%count;
        if(hb_harmony_equal_effective(source[index].harmony,source[previous].harmony))continue;
        events[used++]=source[index];
    }
    return used;
}
/* Tests compare both traversal paths against the same musical state. */
#ifdef HB_PAD_PREVIEW_TEST
#define HB_PAD_PREVIEW_CAN_COMPARE 1
static int hb_pad_preview_reuse=1;
#else
#define hb_pad_preview_reuse 1
#endif
/* Reuse the note renderer on a private preview under the requested harmony.
   Display membership compares the current rendered pitch with each target
   chord; a future color target must not change this rendering context. */
static unsigned hb_pad_render_mask(Inst *preview,const Inst *instance,
                                  hb_harmony_t harmony,int source_note,int root_only,
                                  unsigned long long *output_low,unsigned long long *output_high){
    preview->render_harmony=harmony;preview->render_harmony_active=1;
    unsigned rendered_mask=0;preview->preview_gap_mask=0;preview->preview_target=-1;
    preview->preview_single_low=preview->preview_single_high=0;
    hb_cp_config config=hb_cp_effective_config(&instance->player);
    /* Follower colors describe the single note before Auto Chord expansion.
       Render that note through the ordinary mapper, including approaches and
       Follow Play, instead of substituting the generated chord's root. */
    int follower_single=root_only&&(instance->role==1||root_only==2);
    if(follower_single)config.mode=0;
    memset(&preview->player,0,sizeof(preview->player));preview->player.config=config;
    preview->approach_pad_armed=instance->approach_pad_armed;
    preview->motion=instance->motion;preview->motion.event_override=0;preview->next_touch_mask=instance->next_touch_mask;hb_next_touch_clear_expired(preview);
    hb_mo_input(&preview->motion,source_note,hb_motion_position(preview),hb_ms_to_beats(25));
    /* Every pad preview starts from the live cursor, then simulates its own
       identity switch privately; scanning other pads cannot consume a step. */
    preview->approach_rows.sequence_pad=instance->approach_rows.sequence_pad;
    preview->approach_rows.sequence_cursor=instance->approach_rows.sequence_cursor;
    preview->approach_rows.sequence_event=instance->approach_rows.sequence_event;
    preview->approach_rows.row_event=instance->approach_rows.row_event;
    hb_ar_pad_press(&preview->approach_rows,source_note,
        preview->movy_pad_shift[source_note]?preview->approach_rows.preview_row:-1,
        preview->movy_pad_shift[source_note]);
    if((preview->approach_rows.enabled||preview->movy_pad_shift[source_note])&&hb_ar_live_peek(&preview->approach_rows,preview->movy_pad_shift[source_note]?preview->approach_rows.preview_row:-1)){
        unsigned token=hb_ar_live_peek(&preview->approach_rows,preview->movy_pad_shift[source_note]?preview->approach_rows.preview_row:-1);preview->approach_rows.tokens[source_note]=token;
        if((token&63)>=15&&(token&63)!=60)return hb_ar_preview(preview,source_note,token,root_only,output_low,output_high);
        preview->motion.events[HB_MOTION_LANES]=(preview->motion.events[HB_MOTION_LANES]&~0x573ULL)|hb_ar_intent(token);
    }else preview->approach_rows.tokens[source_note]=0;
    if(follower_single)hb_player_note_on_config(preview,source_note,0,100,&config);
    else hb_player_note_on(preview,source_note,0,100);
    for(int owner=0;owner<HB_CP_KEYS;owner++)if(preview->player.keys[owner].used){
        hb_cp_key *voice=&preview->player.keys[owner];
        if(preview->trail_enabled&&root_only){uint8_t target_message[3]={0x90,(uint8_t)voice->played_pitch,100};int pitch,gain,pan,skip;double off;
        preview->motion.event_override=preview->motion_player_events[owner];
        hb_motion_resolve_output(preview,target_message,&pitch,&gain,&pan,&off,&skip);
        if(!skip)preview->preview_target=pitch;}
        preview->preview_gap_mask|=preview->role==0?hb_key_mask(hb_key_for(preview),voice->gap_mask|voice->semantic_mask):voice->gap_mask|voice->semantic_mask;
        preview->motion.event_override=preview->motion_player_events[owner];
        /* A chord gesture is represented by its generated root,
           independent of inversion, extensions and voice count. */
        int single=root_only&&config.mode;
        int include_outputs=output_low&&output_high&&!follower_single;
        int count=single?(include_outputs?voice->count+1:1):voice->count;
        for(int index=0;index<count;index++){
            int color_sample=!single||index==0;
            int representative=single?(index==0?(preview->role==0?hb_key_pitch(preview,60+voice->root_pc):60+voice->root_pc):voice->notes[index-1]):voice->notes[index];
            uint8_t message[3]={0x90,(uint8_t)representative,100};
            int pitch,velocity,pan,skip;double off;
            hb_motion_values(preview,message,&pitch,&velocity,&pan,&off,&skip);
            int modifier=hb_mo_held_modifier(&preview->motion);
            if(modifier&&!hb_mo_chord_approach_done(&preview->motion))pitch=hb_apply_approach(preview,pitch,modifier<0?HB_APPROACH_CHROM_BELOW:modifier==2?HB_APPROACH_CHROM_ABOVE:HB_APPROACH_SCALE_ABOVE);
            if(!skip){
                if(color_sample){
                    rendered_mask|=1u<<mod12(pitch);
                    if(pitch>=0&&pitch<64)preview->preview_single_low|=1ULL<<pitch;
                    else if(pitch>=64&&pitch<128)preview->preview_single_high|=1ULL<<(pitch-64);
                }
                if(include_outputs&&(!single||index>0)&&pitch>=0&&pitch<128){
                    if(pitch<64)*output_low|=1ULL<<pitch;
                    else *output_high|=1ULL<<(pitch-64);
                }
            }
        }
    }
    /* Output grouping still compares the actual expanded voicing. Reset and
       render it separately on the same private preview; no live state changes. */
    if(follower_single&&output_low&&output_high){
        /* Without expansion, the color voice already is the complete
           output. Replaying it repeats all mapping and operation scans. */
        if(!hb_cp_effective_config(&instance->player).mode){
            *output_low|=preview->preview_single_low;
            *output_high|=preview->preview_single_high;
            return rendered_mask;
        }
        unsigned long long low=preview->preview_single_low,high=preview->preview_single_high;
        unsigned gap_mask=preview->preview_gap_mask;int target=preview->preview_target;
        hb_pad_render_mask(preview,instance,harmony,source_note,0,output_low,output_high);
        preview->preview_single_low=low;preview->preview_single_high=high;preview->preview_gap_mask=gap_mask;preview->preview_target=target;
    }
    return rendered_mask;
}

/* Color membership can use a different form from the sounding voicing.
   Reuse the chord-form engine against the rendered harmony and parent scale. */
static unsigned hb_pad_chord_mask_base_form(Inst *instance,hb_harmony_t harmony,int form){
    if(!harmony.valid)return 0;
    unsigned chord=hb_harmony_chord_mask(harmony);
    if(form==HB_CP_FOLLOW_DETECTED)return hb_harmony_detected_mask(harmony);
    if(!form)return chord;
    hb_cp_config config;hb_cp_defaults(&config);
    config.mode=2;config.size=form;config.inversion=1;
    int notes[12];unsigned mask=0;
    int count=hb_cp_voice(config,60+harmony.root_pc,harmony.root_pc,chord,
        hb_follower_scale_target(instance,harmony).pitch_mask,notes);
    for(int index=0;index<count;index++)mask|=1u<<mod12(notes[index]);
    return mask;
}

static unsigned hb_pad_chord_mask_form(Inst *instance,hb_harmony_t harmony,int form){
    return hb_pad_chord_mask_base_form(instance,hb_key_harmony(instance,harmony),form);
}
static unsigned hb_pad_chord_mask(Inst *instance,hb_harmony_t harmony){return hb_pad_chord_mask_form(instance,harmony,g_pad_chord_form);}

/* Pulse the same form used for color membership, including added tensions. */
static unsigned hb_pad_next_mask_roles(Inst *instance,hb_harmony_t harmony,unsigned roles){
    if(!harmony.valid||!g_pad_next_pulse)return 0;
    harmony=hb_key_harmony(instance,harmony);
    int form=(g_pad_settings[0]==0||g_pad_settings[0]==2)?g_pad_chord_form:g_pad_next_chord_form;
    unsigned chord=hb_pad_chord_mask_base_form(instance,harmony,form),relative=0,mask=0;
    /* Retain detected quality context when the display omits root/fifth. */
    unsigned context=chord|hb_harmony_chord_mask(harmony);
    for(int interval=0;interval<12;interval++)if(context&(1u<<mod12(harmony.root_pc+interval)))relative|=1u<<interval;
    for(int interval=0;interval<12;interval++)if((chord&(1u<<mod12(harmony.root_pc+interval)))&&
        (roles&(1u<<hb_cp_interval_role(interval,relative))))mask|=1u<<mod12(harmony.root_pc+interval);
    return mask;
}

static unsigned hb_pad_next_mask(Inst *instance,hb_harmony_t harmony){return hb_pad_next_mask_roles(instance,harmony,PAD_NEXT_ROLES[g_pad_next_pulse]);}
/* Ordered selections: 3+7, 1+5, 9+11+13. Single tones and All stay uniform. */
static unsigned hb_pad_next_rank(Inst *instance,hb_harmony_t harmony,int rank){
    unsigned roles=g_pad_next_pulse==3?(rank==2?64:0):g_pad_next_pulse==6?(rank==2?16:0):g_pad_next_pulse==10?(rank==2?8:32):0;
    return roles?hb_pad_next_mask_roles(instance,harmony,roles):0;
}

#include "../../../src/render_rhythm_params.h"
#include "../../../src/motif_params.h"
#include "../../../src/motif_loader.h"
#include "../../../src/approach_rows_params.h"
#include "../../../src/chord_state_params.h"
#include "../../../src/display_snapshot.h"
/* Stopped-transport preview is separate from observed harmony and learning.
   Never feed these notes to MIDI, recording, held owners or conductor sensing. */
static hb_harmony_t hb_opening_harmony(void){
    hb_harmony_t empty={0};
    if(g_movy_running)return empty;
    uint8_t observed[64];if(hb_observed_notes(0,observed,64)>0)return empty;
    uint8_t pitches[128]={0},notes[128];int count=0,known_root=-1,root_conflict=0;unsigned quality_mask=0;
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *instance=&g_pool[index];
        if(!instance->used||instance->role!=0||instance->opening_preview[0]!='1')continue;
        unsigned long long hash=hb_timeline_rendering(instance)^hb_next_configuration();
        for(const unsigned char *word=(const unsigned char*)instance->opening_preview;*word;word++)hash=(hash^*word)*1099511628211ULL;
        hash=hb_clip_hash(hash,g_bus.seq);
        if(hash!=instance->opening_hash){
            uint8_t rendered[128]={0};int valid=1,source_root=-1,source_conflict=0;unsigned source_quality=0;
            Inst preview=*instance;
            hb_role_sync(&preview);
            hb_cp_config config=hb_cp_effective_config(&preview.player);
            memset(&preview.player,0,sizeof(preview.player));preview.player.config=config;
            preview.movy_playback=1;preview.chord_pair_input=preview.chord_pair_render=0;
            const char *cursor=instance->opening_preview+1;
            while(*cursor){
                if(*cursor++!=';'){valid=0;break;}
                char *end;long pitch=strtol(cursor,&end,10);
                if(end==cursor||pitch<0||pitch>127||*end!=','){valid=0;break;}cursor=end+1;
                long baked=strtol(cursor,&end,10);
                if(end==cursor||baked<0||baked>1){valid=0;break;}cursor=end;
                unsigned long long words[HB_MOTION_LANES+1]={0};int word_count=0;
                while(*cursor==','){
                    if(word_count>HB_MOTION_LANES){valid=0;break;}
                    cursor++;words[word_count]=strtoull(cursor,&end,16);
                    if(end==cursor){valid=0;break;}
                    if(word_count<HB_MOTION_LANES&&(words[word_count]&HB_MO_RECORDED)&&hb_mo_word_operation(words[word_count])>HB_MO_CHORD_STATE){valid=0;break;}
                    word_count++;cursor=end;
                }
                if(!valid||(word_count&&word_count!=HB_MOTION_LANES+1)){valid=0;break;}
                if(baked){rendered[hb_cp_clamp((int)pitch+g_bus.global_transpose,0,127)]=1;continue;}
                memset(preview.player.keys,0,sizeof(preview.player.keys));
                preview.motion=instance->motion;preview.motion.event_override=word_count?words:0;
                hb_player_note_on(&preview,(int)pitch,0,100);
                for(int owner=0;owner<HB_CP_KEYS;owner++)if(preview.player.keys[owner].used){
                    const hb_cp_key *voice=&preview.player.keys[owner];
                    source_quality|=voice->semantic_mask;
                    if(source_root>=0&&source_root!=voice->root_pc)source_conflict=1;
                    source_root=voice->root_pc;
                    for(int n=0;n<voice->count;n++)rendered[preview.key_reference[owner][n]]=1;
                }
            }
            memset(instance->opening_pitches,0,sizeof(instance->opening_pitches));
            if(valid)memcpy(instance->opening_pitches,rendered,sizeof(rendered));
            instance->opening_quality=valid?source_quality:0;instance->opening_root=valid&&!source_conflict?source_root:-1;
            instance->opening_hash=hash;
        }
        for(int pitch=0;pitch<128;pitch++)if(instance->opening_pitches[pitch])pitches[pitch]=1;
        quality_mask|=instance->opening_quality;
        if(instance->opening_root>=0){
            if(known_root>=0&&known_root!=instance->opening_root)root_conflict=1;
            known_root=instance->opening_root;
        }
    }
    unsigned form=0;
    for(int pitch=0;pitch<128;pitch++)if(pitches[pitch]){form|=1u<<mod12(pitch);notes[count++]=(uint8_t)pitch;}
    if(!count)return empty;
    int bass=mod12(notes[0]);
    quality_mask|=form;count=0;
    for(int pc=0;pc<12;pc++)if(quality_mask&(1u<<pc))notes[count++]=(uint8_t)(60+pc);
    hb_harmony_t result=hb_infer_harmony(notes,count);
    if(known_root>=0&&!root_conflict){result.valid=1;result.root_pc=known_root;result=hb_refine_harmony_with_root(notes,count,result);}
    result.detected_mask=(uint16_t)form;result.bass_pc=bass;
    hb_format_harmony(result.name,sizeof(result.name),result);
    return result;
}
#include "../../../src/shared_context_runtime.h"
static int get_param(void *value,const char *key,char *buffer,int length){Inst *instance=(Inst*)value;if(!instance||!key||!buffer||length<2)return -1;
/* External previews have their own geometry, but use the production pad renderer.
   This callback runs serially on the owning audio thread; restore every transient
   field before returning, including failures and recursive pad_view reads. */
int surface_bank=!strcmp(key,"surface_view0")?0:!strcmp(key,"surface_view1")?1:-1;
if(surface_bank>=0){
    if(!instance->surface_enabled||instance->surface_count[surface_bank]!=32)return -1;
    int count=instance->preview_count,notes[32],targets[32],rows[32];
    memcpy(notes,instance->preview_notes,sizeof(notes));memcpy(targets,instance->preview_targets,sizeof(targets));memcpy(rows,instance->preview_rows,sizeof(rows));
    instance->preview_count=32;
    memcpy(instance->preview_notes,instance->surface_notes[surface_bank],sizeof(notes));
    memcpy(instance->preview_targets,instance->surface_targets[surface_bank],sizeof(targets));
    memcpy(instance->preview_rows,instance->surface_rows[surface_bank],sizeof(rows));
    int result=get_param(instance,"pad_view",buffer,length);
    instance->preview_count=count;
    memcpy(instance->preview_notes,notes,sizeof(notes));memcpy(instance->preview_targets,targets,sizeof(targets));memcpy(instance->preview_rows,rows,sizeof(rows));
    return result;
}

if(!strcmp(key,"harm_play_release_control"))return snprintf(buffer,(size_t)length,"%s",instance->player.release_latched?"Latch":instance->player.release_held?"Hold":instance->player.release_armed?"Armed":"Off");
if(!strcmp(key,"harm_play_release_harmony"))return snprintf(buffer,(size_t)length,"%s",instance->player.release_follow_harmony?"Follow Harmony":"Freeze at Release");
if(!strcmp(key,"harm_play_release")){int setting=instance->player.release_ms;return setting<0?snprintf(buffer,(size_t)length,"%s",(setting<=-10?HB_CP_RELEASE_ARP[-setting-10]:BUFFER_DIVISIONS[-setting-1])):snprintf(buffer,(size_t)length,"%d ms",setting);}
int shared_result=hb_sc_get(instance,key,buffer,length);if(shared_result>=0)return shared_result;
int display_result=hb_display_snapshot(instance,key,buffer,length);if(display_result>=0)return display_result;
int rhythm_result=hb_rr_get(instance,key,buffer,length);if(rhythm_result>=0)return rhythm_result;
int state_result=hb_cs_get(instance,key,buffer,length);if(state_result>=0||!strcmp(key,"chain_params"))return state_result;
if(!strcmp(key,"sounding_key"))return hb_sc_get(instance,"shared_context_0",buffer,length);
if(!strcmp(key,"key_center_view")){
    hb_key_context current=hb_key_baseline(instance);
    char local[32];get_param(instance,"key_center",local,sizeof(local));
    const char *state=!strcmp(local,"Armed")?"Armed":!strcmp(local,"Off")?"Off":"On";
    char preview_label[48]="";
    if(!strcmp(state,"Armed")){
        if(g_key_preview[0])snprintf(preview_label,sizeof(preview_label),"Arm %s",g_key_preview);
        else snprintf(preview_label,sizeof(preview_label),"Armed");
        unsigned owner=instance->physical_target;
        int held=owner&&instance->physical_velocity[(owner-1)/128][(owner-1)%128];
        if(instance->key_pending||held){
            hb_key_context destination=instance->key_pending_context;
            if(!instance->key_pending){
                /* A held target previews the next LIVE hit, even while clip
                   events are arriving. Never consume a gesture or emit MIDI. */
                Inst preview=*instance;preview.movy_playback=preview.key_scope=0;
                destination=hb_key_destination(&preview,(owner-1)%128);
            }
            snprintf(preview_label,sizeof(preview_label),"Arm>%s%s",PC_OPTS[destination.target_root],hb_key_quality(destination));
        }
    }
    return snprintf(buffer,(size_t)length,"%s|%s%s|%s|%s|%s",state,PC_OPTS[current.target_root],hb_key_quality(current),preview_label,instance->dominant_color_latched?"Latch":instance->dominant_color_held?"Hold":"Off",DOMINANT_SCALE_OPTS[instance->dominant_color_family]);
}
if(!strcmp(key,"key_center")){int owner=(int)(instance-g_pool);int active=g_sc_live[owner][HB_SC_KEY].on;return snprintf(buffer,(size_t)length,"%s",g_key_armed&&(g_sc_arm_owner<0||g_sc_arm_owner==owner)?"Armed":active?PC_OPTS[g_key_context.target_root]:"Off");}
if(!strcmp(key,"follower_scale")&&instance->movy_track==g_sc_record_track&&g_sc_record_track>=0&&g_sc_live[instance-g_pool][HB_SC_PARENT].on)return snprintf(buffer,(size_t)length,"%s",FOLLOWER_SCALE_OPTS[g_sc_live[instance-g_pool][HB_SC_PARENT].a]);
if(!strcmp(key,"parallel_mode")){int owner=(int)(instance-g_pool);int active=g_sc_live[owner][HB_SC_PARALLEL].on||(instance->movy_track>=0&&instance->role==0&&g_sc_replay[instance->movy_track][HB_SC_PARALLEL].on);return snprintf(buffer,(size_t)length,"%s",active?((g_sc_latch[owner]||(instance->movy_track>=0&&instance->role==0&&g_sc_replay[instance->movy_track][HB_SC_PARALLEL].c==2))?"Latch":"Hold"):"Off");}
if(!strcmp(key,"conductor_key_travel"))return snprintf(buffer,(size_t)length,"%s",HB_KEY_TRAVEL[g_key_conductor_travel]);
if(!strcmp(key,"follower_key_travel")||!strcmp(key,"follower_recorded_key_travel")||!strcmp(key,"follower_live_key_travel")){
    int travel=g_key_follower_travel;
    return snprintf(buffer,(size_t)length,"%s",travel<0?"Same as Conductor":HB_KEY_TRAVEL[travel]);
}
if(!strcmp(key,"key_center_scale"))return snprintf(buffer,(size_t)length,"%s",g_key_scale_mode==2?"Use Parallel Scale":g_key_scale_mode==0?"Simplified Major/Minor":"Mode from Parent");
if(!strcmp(key,"parallel_scale"))return snprintf(buffer,(size_t)length,"%s",HB_PARALLEL_NAMES[g_parallel_scale]);
for(int field=0;field<4;field++)if(!strcmp(key,HB_TARGET_KEYS[field]))
    return snprintf(buffer,(size_t)length,"%s",HB_TARGET_OPTIONS[field][instance->target_scale_policy[field]]);
int approach_result=hb_ar_get(instance,key,buffer,length);if(approach_result>=0)return approach_result;
int loader_result=hb_ml_get(instance,key,buffer,length);if(loader_result>=0)return loader_result;
int motif_result=hb_mt_get(instance,key,buffer,length);if(motif_result>=0)return motif_result;
hb_role_sync(instance);int policy_result=hb_policy_get(instance,key,buffer,length);if(policy_result>=0)return policy_result;
if(!strcmp(key,"scale_status")){
    hb_harmony_t selected=hb_render_harmony(instance);int minor=0,destination=-1;
    int mode=hb_policy_value(instance,HB_P_GAP);
    if(mode==2||selected.intent_kind)destination=hb_context_destination(instance,selected,&minor);
    return destination>=0?snprintf(buffer,(size_t)length,"%s: %s %s",selected.intent_kind?"Intent":"Context",hb_pc_name(destination),minor?"minor":"major"):
        snprintf(buffer,(size_t)length,"%s",mode==0?"Parent":mode==1?"Strict local":"Auto local: fallback");
}

if(!strcmp(key,"motion_editor")){
    /* One control-thread snapshot replaces a page of synchronous IPC reads. */
    static const char *keys[]={"motion_lane","motion_operation","motion_pattern","motion_amount","motion_offset","motion_enabled","motion_grid","motion_cycle","motion_phase","motion_probability","motion_group","motion_evolve","motion_advance","motion_every","motion_from","motion_through","motion_auto_off","motion_touch_mode","motion_condition_range","motion_condition_status","motion_overview","motion_bypass","motion_punch","touch_hold_ms","chord_edit_target","chord_mode","chord_form","chord_quality","chromatic_quality","chord_inversion","chord_voicing","arp_playback","arp_hold","arp_order","arp_rate","arp_gate","arp_start","arp_phase","arp_note_phase","arp_clear_harmony","strum_spread","chord_input","chord_state_copy","defaults_editor","defaults_control_1","defaults_control_2","defaults_control_3","defaults_control_4","defaults_control_5","defaults_control_6","defaults_control_7","local_palette"};
    int used=snprintf(buffer,(size_t)length,"{\"params\":");
    if(used>=length)return -1;
    int count=hb_cs_metadata(instance,buffer+used,length-used);
    if(count<0||count>=length-used)return -1;used+=count;
    count=snprintf(buffer+used,(size_t)(length-used),",\"values\":{");
    if(count>=length-used)return -1;used+=count;
    for(unsigned index=0;index<sizeof(keys)/sizeof(keys[0]);index++){
        char result[128];count=get_param(value,keys[index],result,sizeof(result));
        if(count<0||count>=(int)sizeof(result))return -1;
        /* These enum/status values are generated locally, never arbitrary text. */
        count=snprintf(buffer+used,(size_t)(length-used),"%s\"%s\":\"%s\"",index?",":"",keys[index],result);
        if(count>=length-used)return -1;used+=count;
    }
    count=snprintf(buffer+used,(size_t)(length-used),"}}");
    return count>=length-used?-1:used+count;
}

/* Optional visible-pad list: 32 hex MIDI notes (ff means a layout gap).
   Preview only visible notes, in the same snapshot as their harmony colors. */
const char *pad_request=0;int pad_notes[32],pad_targets[32],pad_count=0;
if(!strncmp(key,"pad_view@",9)||!strncmp(key,"pad_render@",11)){
    pad_request=key+(!strncmp(key,"pad_view@",9)?9:11);
    size_t request_length=strlen(pad_request);
    if(request_length!=64&&(request_length!=129||pad_request[64]!=':'))return -1;
    for(int index=0;index<32;index++){
        unsigned note=0;
        if(sscanf(pad_request+index*2,"%2x",&note)!=1||(note>127&&note!=255))return -1;
        unsigned target=0;
        if(request_length==129&&(sscanf(pad_request+65+index*2,"%2x",&target)!=1||target>128))return -1;
        pad_targets[pad_count]=target?(int)target-1:-1;
        pad_notes[pad_count++]=note==255?-1:(int)note;
    }
    key=!strncmp(key,"pad_view",8)?"pad_view":"pad_render";
}
if(!pad_request&&instance->preview_count&&(!strcmp(key,"pad_view")||!strcmp(key,"pad_render"))){
    pad_count=instance->preview_count;
    memcpy(pad_notes,instance->preview_notes,sizeof(pad_notes));
    memcpy(pad_targets,instance->preview_targets,sizeof(pad_targets));
}
hb_harmony_t harmony=bus_read();
if(!strcmp(key,"trail_history")){
    int used=snprintf(buffer,(size_t)length,"th2,%.6f,%u",instance->motion_beat,instance->trail_chord);
    for(int pitch=0;pitch<128&&used>=0&&used<length;pitch++)if(instance->trail_valid[pitch])
        used+=snprintf(buffer+used,(size_t)(length-used),";%d,%.6f,%u,%.6f,%d",pitch,instance->trail_at[pitch],instance->trail_chord_at[pitch],instance->trail_previous_at[pitch],instance->trail_previous_valid[pitch]?(int)instance->trail_previous_chord[pitch]:-1);
    return used;
}
if(!strcmp(key,"render_velocity_percent"))return snprintf(buffer,(size_t)length,"%d",(instance->render_velocity_gain+50)/100);
if(!strcmp(key,"pad_tonic_color"))return snprintf(buffer,(size_t)length,"%s",PAD_TONIC_COLORS[g_pad_tonic_color]);
if(!strcmp(key,"pad_both_color"))return snprintf(buffer,(size_t)length,"%s",PAD_BOTH_COLORS[g_pad_both_color]);
if(!strcmp(key,"pad_play_color"))return snprintf(buffer,(size_t)length,"%s",PAD_PLAY_COLORS[g_pad_play_color]);
if(!strcmp(key,"pad_adjacent_shading"))return snprintf(buffer,(size_t)length,"%s",g_pad_adjacent_shading?"On":"Off");
if(!strcmp(key,"pad_next_pulse"))return snprintf(buffer,(size_t)length,"%s",PAD_NEXT_PULSE[g_pad_next_pulse]);
if(!strcmp(key,"pad_chord_form"))return snprintf(buffer,(size_t)length,"%s",CP_CHORD_FORM[g_pad_chord_form]);
if(!strcmp(key,"pad_next_chord_form"))return snprintf(buffer,(size_t)length,"%s",CP_CHORD_FORM[g_pad_next_chord_form]);
if(!strcmp(key,"pad_effective_color"))return snprintf(buffer,(size_t)length,"%s",PAD_COLORS[g_pad_settings[3]]);
if(!strcmp(key,"chord_grid_status")){
    if(g_movy_blocked)return snprintf(buffer,(size_t)length,"%s",g_movy_blocked==1?"No clips":g_movy_blocked==2?"Cycle too long":g_movy_blocked==3?"Non-repeating":"Too many changes");
    hb_loop_harmony_event_t events[HB_MAX_LOOP_HARMONIES];
    int count=hb_timing_events(events);
    return snprintf(buffer,(size_t)length,"%s %d",g_bus.next_model_locked?"Locked":"Learning",count);
}
if(!strcmp(key,"timing_position")){
    if(g_movy_blocked||hb_next_loop_length()<=0.0)return snprintf(buffer,(size_t)length,"--");
    return hb_timing_position(buffer,length,hb_next_phase(hb_clip_playhead()));
}
if(!strcmp(key,"timing_last_at")||!strcmp(key,"timing_last_chord")){
    if(!timing_have_transition||g_movy_blocked)return snprintf(buffer,(size_t)length,"--");
    return !strcmp(key,"timing_last_at")?hb_timing_position(buffer,length,timing_last_phase):
        hb_format_harmony(buffer,length,timing_last_harmony);
}
if(!strcmp(key,"timing_next_at")||!strcmp(key,"timing_next_chord")){
    if(!g_bus.next_model_locked||g_movy_blocked)return snprintf(buffer,(size_t)length,"--");
    hb_loop_harmony_event_t events[HB_MAX_LOOP_HARMONIES];
    int count=hb_timing_events(events),selected=-1;
    double phase=hb_next_phase(hb_clip_playhead()),cycle=hb_next_loop_length(),best=1e99;
    for(int index=0;index<count;index++){
        double distance=events[index].phase-phase;
        if(distance<=1e-6)distance+=cycle;
        if(distance<best){selected=index;best=distance;}
    }
    if(selected<0)return snprintf(buffer,(size_t)length,"--");
    return !strcmp(key,"timing_next_at")?hb_timing_position(buffer,length,events[selected].phase):
        hb_format_harmony(buffer,length,events[selected].harmony);
}
if(!strcmp(key,"render_velocity_gain"))return snprintf(buffer,(size_t)length,"%.4f",instance->render_velocity_gain/10000.0);
if(!strcmp(key,"hb_tempo")){
    double bpm=(g_host&&g_host->get_bpm)?g_host->get_bpm():120.0;
    return snprintf(buffer,(size_t)length,"%.0f",bpm>0?bpm:120.0);
}
if(!strcmp(key,"humanize_timing")||!strcmp(key,"humanize_velocity")||!strcmp(key,"humanize_gate")){
    int index=!strcmp(key,"humanize_timing")?0:!strcmp(key,"humanize_velocity")?1:2;
    return snprintf(buffer,(size_t)length,"%d",g_humanize[index]);
}
if(!strcmp(key,"humanize_scope"))return snprintf(buffer,(size_t)length,"Clips");
if(!strcmp(key,"follower_input_context_v2")){
    int used=get_param(value,"follower_input_context",buffer,length);if(used<0||used>=length)return -1;
    unsigned mask=0;
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *source=&g_pool[index];
        if(source->used&&source->role<=1&&source->movy_track>=0&&source->movy_track<16)mask|=1u<<source->movy_track;
    }
    used+=snprintf(buffer+used,(size_t)(length-used),"|hu1,%d,%d,%d,%u",g_humanize[0],g_humanize[1],g_humanize[2],mask);
    return hb_sc_append_events(buffer,length,used);
}
if(!strcmp(key,"used_scale")){
    int root=0;if(!hb_resolve_follower_reference_root(instance,&root))return snprintf(buffer,(size_t)length,"--");
    int selected=hb_shared_follower_scale(),resolved=hb_follower_input_scale_index(instance,root);
    return snprintf(buffer,(size_t)length,"%s%s",FOLLOWER_SCALE_OPTS[resolved],selected?"":!g_infer_has_evidence?" (default)":g_infer_ambiguous?" ?":"");
}
if(!strcmp(key,"follower_input_context")){
    int root=hb_global_explicit_root();hb_resolve_follower_reference_root(instance,&root);
    unsigned mask=0;
    for(int index=0;index<HB_MAX_INSTANCES;index++){
        Inst *follower=&g_pool[index];
        if(follower->used&&follower->role==1&&follower->movy_track>=0&&follower->movy_track<16)
            mask|=1u<<follower->movy_track;
    }
    return snprintf(buffer,(size_t)length,"fic1,%d,%d,%u",root,hb_follower_input_scale_index(instance,root),mask);
}

if(!strcmp(key,"harmony_snapshot")){
    hb_capture_harmony_display(instance);
    return snprintf(buffer,(size_t)length,"hp1|%s|%s|%s|%s",instance->harmony_display[0],
        instance->harmony_display[1],instance->harmony_display[2],instance->harmony_display[3]);
}
if(!strncmp(key,"hpath_",6)){
    int field=-1;
    if(sscanf(key,"hpath_%d",&field)==1&&field>=0&&field<4){
        if(field==0||!instance->harmony_display_valid)hb_capture_harmony_display(instance);
        return snprintf(buffer,(size_t)length,"%s",instance->harmony_display[field]);
    }
}
if(!strcmp(key,"follower_snapshot")){
    hb_capture_follower_display(instance);
    return snprintf(buffer,(size_t)length,"fp1|%s|%s|%s|%s|%s|%s|%s|%s",
        instance->follower_display[0],instance->follower_display[1],
        instance->follower_display[2],instance->follower_display[3],
        instance->follower_display[4],instance->follower_display[5],
        instance->follower_display[6],instance->follower_display[7]);
}
/* Stock hosts poll cells in knob order. Freeze sibling fields from the first
   cell so a note change mid-sweep cannot pair G with another note's role. */
if(!strncmp(key,"fpath_0_",8)){
    int ordinal=0,field=0;
    if(sscanf(key,"fpath_0_%d_%d",&ordinal,&field)==2&&ordinal<2&&ordinal>=0&&field>=0&&field<4){
        if((ordinal==0&&field==0)||!instance->follower_display_valid)hb_capture_follower_display(instance);
        return snprintf(buffer,(size_t)length,"%s",instance->follower_display[ordinal*4+field]);
    }
}
int path_result=hb_follower_path(instance,key,buffer,length);if(path_result!=-2)return path_result;
if(!strcmp(key,"performance_status")||!strcmp(key,"motion_row")){
    unsigned mask=0;for(int lane=0;lane<HB_MOTION_USER_LANES;lane++)if(hb_mo_lane_active(&instance->motion,lane)&&hb_mo_condition(&instance->motion,lane,hb_motion_condition_position()))mask|=1ULL<<lane;
    for(int lane=0;lane<HB_MOTION_LANES;lane++){
        int operation=instance->motion.lanes[lane].operation;
        unsigned pending=instance->motion.tap_started?0:instance->motion.tap_mask;
        if((operation==HB_MO_ABOVE&&(pending&2))||(operation==HB_MO_BELOW&&(pending&1)))mask|=1ULL<<lane;
    }
    int used=snprintf(buffer,(size_t)length,"%u",mask);
    if(!strcmp(key,"motion_row"))for(int lane=0;lane<HB_MOTION_USER_LANES;lane++){
        if(used<0||used>=length)return -1;
        used+=snprintf(buffer+used,(size_t)(length-used),",%d",instance->motion.lanes[lane].operation);
    }
    return used;
}
if(!strcmp(key,"motion_condition_status")){
    const hb_motion_lane *lane=&instance->motion.lanes[instance->motion.selected];
    double beat=hb_motion_condition_position();
    const char *status="Ready";
    if(lane->operation==HB_MO_OFF)status="Off";
    else if(lane->operation>=HB_MO_REPEAT&&lane->operation<=HB_MO_SPEED&&instance->motion.host_capabilities<2)status=instance->motion.host_capabilities?"Update Movy":"Requires Movy";
    else if(hb_mo_enclosure_mask(lane->operation))status="Trigger";
    else if(instance->motion.held&(1u<<instance->motion.selected))status="Held";
    else if(instance->motion.bypass)status="Bypassed";
    else if(!lane->enabled)status="Auto Off";
    else if(beat<0&&lane->every>1)status="Stopped";
    else if(!hb_mo_condition(&instance->motion,instance->motion.selected,beat))status="Waiting";
    else if(!lane->probability)status="Prob 0";
    if(beat<0)return snprintf(buffer,(size_t)length,"-/%d %s",lane->every,status);
    return snprintf(buffer,(size_t)length,"%d/%d %s",hb_mo_condition_cycle(lane,beat),lane->every,status);
}
if(!strcmp(key,"follow_touch_labels")){
    int used=0;
    for(int slot=0;slot<10;slot++){
        int lane=slot==9?6:instance->touch_lanes[slot];const hb_motion_lane *operation=&instance->motion.lanes[lane-1];
        const char *label=hb_mo_operation_name(operation->operation);
        if(operation->operation==HB_MO_HARMONY)label=operation->amount>=50?
            "Next Harmony":"Current Harmony";
        if(used<0||used>=length)return -1;
        used+=snprintf(buffer+used,(size_t)(length-used),"%s%d:%s",slot?"|":"",lane,label);
    }
    return used;
}
int touch_slot=hb_mo_slot_key(key,"follow_touch_");if(touch_slot>=0&&touch_slot<9)return snprintf(buffer,(size_t)length,"%d",instance->touch_lanes[touch_slot]);
int motion_value=hb_mo_get(&instance->motion,key,buffer,length);if(motion_value>=0||!strcmp(key,"chain_params"))return motion_value;
for(int index=0;index<5;index++)if(!strcmp(key,PAD_KEYS[index]))return snprintf(buffer,(size_t)length,"%s",PAD_OPTIONS[index][g_pad_settings[index]]);
if(!strcmp(key,"state")){
    int used=get_param(value,"pad_state_base",buffer,length);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";pd1,%d,%d,%d,%d,%d",g_pad_settings[0],g_pad_settings[1],g_pad_settings[2],g_pad_settings[3],g_pad_settings[4]);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";dc1,%d",instance->dominant_color_family);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";ts1,%d,%d,%d,%d",instance->target_scale_policy[0],instance->target_scale_policy[1],instance->target_scale_policy[2],instance->target_scale_policy[3]);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";pas1,%d",g_pad_adjacent_shading);
    if(used<0||used>=length)return used;
    if(g_pad_next_pulse)used+=snprintf(buffer+used,(size_t)(length-used),";pnp1,%d",g_pad_next_pulse);
    if(used<0||used>=length)return used;
    if(g_pad_chord_form)used+=snprintf(buffer+used,(size_t)(length-used),";pf1,%d",g_pad_chord_form);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";pnf1,%d",g_pad_next_chord_form);
    if(used<0||used>=length)return used;
    if(g_pad_tonic_color!=9)used+=snprintf(buffer+used,(size_t)(length-used),";pt1,%d",g_pad_tonic_color);
    used+=snprintf(buffer+used,(size_t)(length-used),";pb1,%d",g_pad_both_color);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";pc2,%d;pp1,%d",g_pad_settings[3],g_pad_play_color);
    if(used<0||used>=length)return used;
    if(instance->next_lookahead||instance->next_anti_buffer_ms!=25)
        used+=snprintf(buffer+used,(size_t)(length-used),";la1,%d,%d",instance->next_lookahead,instance->next_anti_buffer_ms);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";hu1,%d,%d,%d;ss1,%d,%d",g_humanize[0],g_humanize[1],g_humanize[2],hb_shared_dominant_scale(),hb_shared_borrowed_scale());
    if(used<0||used>=length)return used;
    if(instance->player.release_ms||instance->player.release_follow_harmony)used+=snprintf(buffer+used,(size_t)(length-used),";hr1,%d,%d",instance->player.release_ms,instance->player.release_follow_harmony);
    if(used<0||used>=length)return used;
    if(instance->render_velocity_gain!=10000)used+=snprintf(buffer+used,(size_t)(length-used),";rv1,%d",instance->render_velocity_gain);
    if(used<0||used>=length)return used;
    if(instance->player.config.start!=5)used+=snprintf(buffer+used,(size_t)(length-used),";as1,%d",instance->player.config.start);
    if(used<0||used>=length)return used;
    if(instance->player.config.clear_harmony)used+=snprintf(buffer+used,(size_t)(length-used),";ac1,1");
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";ct1,%d;ft1,%d,%d,%d,%d,%d,%d,%d,%d",hb_chromatic_travel(instance),instance->touch_lanes[0],instance->touch_lanes[1],instance->touch_lanes[2],instance->touch_lanes[3],instance->touch_lanes[4],instance->touch_lanes[5],instance->touch_lanes[6],instance->touch_lanes[7]);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";ft2,%d,%d,%d,%d,%d,%d,%d,%d,%d",g_touch_lanes[0],g_touch_lanes[1],g_touch_lanes[2],g_touch_lanes[3],g_touch_lanes[4],g_touch_lanes[5],g_touch_lanes[6],g_touch_lanes[7],g_touch_lanes[8]);
    used=hb_role_save(instance,buffer,length,used);
    used=hb_mo_save(&instance->motion,buffer,length,used);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";kc1,%d,%d,%d;kt2,%d,%d,%d;kt3,%d,%d",g_key_scale_mode,g_key_conductor_travel,g_parallel_scale,g_key_conductor_travel,g_key_follower_travel,g_key_follower_travel,g_key_conductor_travel,g_key_follower_travel);
    return hb_timeline_save(instance,buffer,length,hb_ar_save(instance,buffer,length,hb_mt_save(instance,buffer,length,hb_ml_save(instance,buffer,length,used))));
}
if(!strcmp(key,"hb_record_action")){
    if(!instance->action_count)return snprintf(buffer,(size_t)length,"-");
    int slot=instance->action_head,used=snprintf(buffer,(size_t)length,"ra4,%d",instance->action_pitch[slot]);
    for(int lane=0;lane<=HB_MOTION_LANES&&used<length;lane++)used+=snprintf(buffer+used,(size_t)(length-used),",%llu",instance->action_queue[slot][lane]);
    if(used>=length)return used;
    instance->action_head=(slot+1)%64;instance->action_count--;return used;
}
if(!strcmp(key,"pad_view")){
    char render_key[160];snprintf(render_key,sizeof(render_key),"pad_render%s%s",pad_request?"@":"",pad_request?pad_request:"");
    int used=get_param(value,render_key,buffer,length);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),"|adjshade1,%d",g_pad_adjacent_shading);
    if(used>=length)return used;
    int active=hb_cp_enabled(&instance->player);
    used+=snprintf(buffer+used,(size_t)(length-used),"|colors2,%d|both1,%d|toniccolor1,%d|playcolor1,%d|arp1,%d",g_pad_settings[3],g_pad_both_color,g_pad_tonic_color,g_pad_play_color,active);
    uint8_t seen[128]={0};
    if(active)for(int index=0;index<HB_CP_KEYS;index++){
        const hb_cp_key *key=&instance->player.keys[index];
        if(!key->used||seen[key->source])continue;
        seen[key->source]=1;
        if(used<0||used>=length)return used;
        used+=snprintf(buffer+used,(size_t)(length-used),",%d",key->source);
    }
    int input_root=0;
    if(instance->role==1&&hb_resolve_follower_reference_root(instance,&input_root)){
        uint16_t input=hb_follower_input_scale(instance,input_root),roles=1u<<input_root;
        unsigned rendered_current=0,rendered_effective=0;
        if(sscanf(buffer,"%u,%u",&rendered_current,&rendered_effective)==2)roles=(uint16_t)rendered_effective;
        if(used<0||used>=length)return used;
        used+=snprintf(buffer+used,(size_t)(length-used),"|input1,%d,%d,%d,%u,%u",input_root,
            hb_shared_follower_scale(),hb_follower_input_scale_index(instance,input_root),input,roles);
    }
    /* Global keyboard scale is visible even on conductor/off tracks. */
    int scale_root=hb_global_explicit_root();
    hb_resolve_follower_reference_root(instance,&scale_root);
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),"|key1,%d,%d|piano1,%d",
        hb_shared_follower_scale(),hb_follower_input_scale_index(instance,scale_root),hb_approach_pad_enabled(instance));
    /* Footer shares this bounded snapshot; no independent UI polling. */
    hb_key_context footer_key=hb_key_baseline(instance);
    int footer_scale=0;
    for(int choice=1;choice<HB_SCALE_COUNT;choice++)
        if(hb_explicit_scale_mask(footer_key.target_root,choice)==footer_key.target_mask&&!HB_SCALE_DOMINANT[choice]){footer_scale=choice;break;}
    hb_harmony_t footer_next={0};
    if(g_bus.next_model_locked&&!g_movy_blocked){
        hb_loop_harmony_event_t events[HB_MAX_LOOP_HARMONIES];
        int count=hb_timing_events(events);double phase=hb_next_phase(hb_clip_playhead()),cycle=hb_next_loop_length(),nearest=1e99;
        for(int index=0;index<count;index++){
            double distance=events[index].phase-phase;if(distance<=1e-6)distance+=cycle;
            if(distance<nearest){nearest=distance;footer_next=events[index].harmony;}
        }
    }
    /* Analyze resolved chords against the resolved key, as playback does. */
    hb_harmony_t footer_current=hb_key_harmony(instance,harmony);
    footer_next=hb_key_harmony(instance,footer_next);
    if(used>=0&&used<length)used+=snprintf(buffer+used,(size_t)(length-used),"|footer1,%d,%d,%d,%u,%d,%u",
        footer_key.target_root,footer_key.blues?-1:footer_scale,footer_current.valid?footer_current.root_pc:-1,
        footer_current.valid?hb_harmony_chord_mask(footer_current):0,footer_next.valid?footer_next.root_pc:-1,
        footer_next.valid?hb_harmony_chord_mask(footer_next):0);
    if(instance->trail_enabled&&used>=0&&used<length){
        used+=snprintf(buffer+used,(size_t)(length-used),"|th2,%.6f,%u",instance->motion_beat,instance->trail_chord);
        for(int pitch=0;pitch<128&&used<length;pitch++)if(instance->trail_valid[pitch])
            used+=snprintf(buffer+used,(size_t)(length-used),";%d,%.6f,%u,%.6f,%d",pitch,instance->trail_at[pitch],instance->trail_chord_at[pitch],instance->trail_previous_at[pitch],instance->trail_previous_valid[pitch]?(int)instance->trail_previous_chord[pitch]:-1);
        /* Conductor and Harmony Off have fast color paths. Resolve only the
           requested targets when Trails is enabled, without changing colors. */
        if((instance->role!=1||g_pad_settings[0]==7)&&pad_count&&used<length){
            Inst preview=*instance;preview.movy_playback=0;
            memset(preview.movy_input_degree,0,sizeof(preview.movy_input_degree));
            memset(preview.movy_input_target,0,sizeof(preview.movy_input_target));
            memset(preview.movy_pad_shift,0,sizeof(preview.movy_pad_shift));
            used+=snprintf(buffer+used,(size_t)(length-used),"|targets1");
            for(int slot=0;slot<pad_count&&used<length;slot++){
                int source=pad_notes[slot],target=pad_targets[slot],gap=target>=0&&hb_approach_pad_enabled(instance);
                if(gap){int row=instance->preview_rows[slot];source=row?(target+32*row)%128:target<64?target+36:target-36;
                    preview.approach_rows.preview_row=row?row-1:3;preview.movy_pad_shift[source]=target-source;}
                int resolved=-1;
                if(source>=0&&instance->role<2){hb_pad_render_mask(&preview,instance,hb_render_harmony(instance),source,1,0,0);resolved=preview.preview_target;}
                if(gap)preview.movy_pad_shift[source]=0;
                used+=snprintf(buffer+used,(size_t)(length-used),",%d",resolved);
            }
        }
    }
    return used;
}
/* Harmony Off exposes input geometry and source-owner feedback only.
   Avoid opening/next-harmony analysis and every per-pad voice simulation. */
if(!strcmp(key,"pad_render")&&g_pad_settings[0]==7){
    int root=hb_global_explicit_root();hb_resolve_follower_reference_root(instance,&root);
    unsigned scale=hb_follower_input_scale(instance,root);
    return snprintf(buffer,(size_t)length,"0,0,%u,0,0,7,0,3,%d,%d|input1,%d,%d,%d,%u,0",
        scale,g_pad_settings[3],g_pad_settings[4],root,hb_shared_follower_scale(),hb_follower_input_scale_index(instance,root),scale);
}
if(!strcmp(key,"pad_render")&&instance->role!=1){
    unsigned playing=0,flashing=0;unsigned long long low=0,high=0,flash_low=0,flash_high=0;
    for(int pitch=0;pitch<128;pitch++)if(instance->pad_flash_seconds[pitch]>0){if(pitch<64)flash_low|=1ULL<<pitch;else flash_high|=1ULL<<(pitch-64);}
    for(int pitch=0;pitch<128;pitch++)if(instance->pad_sounding[pitch]){
        if(pitch<64)low|=1ULL<<pitch;else high|=1ULL<<(pitch-64);
    }
    if((low||high||flash_low||flash_high)&&pad_count){
        Inst preview=*instance;
        for(int slot=0;slot<pad_count;slot++)if(pad_notes[slot]>=0){
            if(instance->role==0){
                hb_pad_render_mask(&preview,instance,hb_render_harmony(instance),pad_notes[slot],2,0,0);
                if((low&preview.preview_single_low)||(high&preview.preview_single_high))playing|=1u<<slot;
                if((flash_low&preview.preview_single_low)||(flash_high&preview.preview_single_high))flashing|=1u<<slot;
            }else {if(instance->pad_sounding[pad_notes[slot]])playing|=1u<<slot;if(instance->pad_flash_seconds[pad_notes[slot]]>0)flashing|=1u<<slot;}
        }
    }
    return snprintf(buffer,(size_t)length,"0,0,0,0,0,%d,%d,%d,%d,%d|playpads1,%u|playflash1,%u",0,g_pad_settings[1],g_pad_settings[2],g_pad_settings[3],g_pad_settings[4],playing,flashing);
}

if(!strcmp(key,"play_rotate")){return snprintf(buffer,(size_t)length,"%d",instance->play.rotate);}
if(!strcmp(key,"play_wrap")){static const char *options[]={"Off","On"};return snprintf(buffer,(size_t)length,"%s",options[instance->play.wrap]);}
if(!strcmp(key,"play_mirror")){static const char *options[]={"Off","On"};return snprintf(buffer,(size_t)length,"%s",options[instance->play.mirror]);}
if(!strcmp(key,"play_octave")){return snprintf(buffer,(size_t)length,"%d",instance->play.octave);}
if(!strcmp(key,"play_range")){static const char *options[]={"1 Oct","2 Oct","3 Oct","4 Oct"};return snprintf(buffer,(size_t)length,"%s",options[instance->play.range]);}
if(!strcmp(key,"play_scope")){static const char *options[]={"Both","Clip","Live"};return snprintf(buffer,(size_t)length,"%s",options[instance->play.scope]);}
if(!strcmp(key,"play_bypass")){static const char *options[]={"Off","On"};return snprintf(buffer,(size_t)length,"%s",options[instance->play.bypass]);}
if(!strcmp(key,"pad_harmony")||!strcmp(key,"pad_render")){
    /* Read-only preview of current conductor and effective rendering harmony.
       Never queue a note, consume an approach, or advance the learned model. */
    hb_harmony_t opening=hb_opening_harmony();
    hb_harmony_t current=opening.valid?opening:g_bus.observed_harmony;
    hb_harmony_t effective=opening.valid?opening:hb_render_harmony(instance);
    hb_harmony_t lookahead=effective;
    int ready=!opening.valid&&hb_render_shift_ready_for(instance);
    int known=!opening.valid&&hb_harmony_knowledge_ready_for(instance);
    double playhead=hb_clip_playhead(),display_phase=hb_next_phase(playhead);
    /* Current and full-next share one learned phase, even at Lookahead Off.
       Live inference may still be settling when the timeline crosses a chord. */
    if(known){
        int current_event=hb_next_model_event_for_phase_for(instance,display_phase,0);
        if(current_event>=0)current=g_bus.next_model[current_event].harmony;
    }
    if(ready){
        double now=hb_current_beat();
        double phase=display_phase;
        int look_event=hb_next_model_event_for_phase_for(instance,phase,1);
        if(look_event>=0)lookahead=g_bus.next_model[look_event].harmony;
        if(hb_cp_playback(&instance->player)!=1&&!hb_next_elapsed_for(instance)){
            double capture=hb_follower_capture_beats_for(instance);
            double boundary=hb_next_effective_boundary_for(instance,now);
            if(boundary>=now-1e-6&&boundary-now<=capture+1e-6&&
               (hb_next_allows_precapture_for(instance)||boundary-now<=1e-6))
                phase=hb_next_phase(playhead+boundary-now+1e-7);
        }
        int event=hb_next_model_event_for_phase_for(instance,phase,1);
        if(event>=0)effective=g_bus.next_model[event].harmony;
    }
    /* Full preview uses the next observed loop event, independently of the
       render offset. It never changes effective harmony or playback timing. */
    hb_harmony_t full_lookahead=opening;
    if(known){
        double phase=display_phase,length=hb_next_loop_length(),nearest=1e99;
        for(int index=0;index<g_bus.next_model_count;index++){
            double distance=g_bus.next_model[index].phase-phase;
            if(distance<=0.0)distance+=length;
            if(distance<nearest){nearest=distance;full_lookahead=g_bus.next_model[index].harmony;}
        }
    }
    for(int lane=0;lane<HB_MOTION_LANES;lane++){
        double choice;
        if(hb_mo_operation(&instance->motion,lane)==HB_MO_HARMONY&&
           hb_mo_value_at(&instance->motion,lane,hb_motion_position(instance),hb_motion_condition_position(),0,&choice)){
            effective=hb_render_harmony(instance);break;
        }
    }
    ready=ready||opening.valid;
    hb_harmony_t scale=hb_follower_scale_target(instance,hb_key_harmony(instance,effective));
    if(!strcmp(key,"pad_render")){
        /* Render on a private instance: real mapping/voicing, no emitted MIDI,
           no live owner changes and no consumption of one-shot modifiers. */
        Inst preview=*instance;
        /* Opening notes describe a stopped clip's color target only. They
           never replace the harmony used by an actual live note-on. */
        hb_harmony_t rendering=opening.valid?hb_render_harmony(instance):effective;
        preview.render_harmony=rendering;preview.render_harmony_active=1;
        preview.movy_playback=0;
        /* Real live note-ons clear replay-only source coordinates. A pad
           preview must do the same on its private copy: clip notes can carry
           different roles as the conductor changes, even at the same pitch. */
        memset(preview.movy_input_degree,0,sizeof(preview.movy_input_degree));
        memset(preview.movy_input_target,0,sizeof(preview.movy_input_target));
        memset(preview.movy_pad_shift,0,sizeof(preview.movy_pad_shift));
        unsigned current_inputs=0,effective_inputs=0,lookahead_inputs=0,scale_inputs=0,tonic_inputs=0,full_inputs=0;
        hb_harmony_t pulse_harmony=g_pad_settings[0]>=5?full_lookahead:
            g_pad_settings[0]==0||g_pad_settings[0]==2?effective:
            g_pad_settings[0]!=1&&ready?lookahead:(hb_harmony_t){0};
        unsigned next_mask=hb_pad_next_mask(instance,pulse_harmony),next_inputs=0,next_pads=0;
        unsigned secondary_mask=hb_pad_next_rank(instance,pulse_harmony,2),tertiary_mask=hb_pad_next_rank(instance,pulse_harmony,3);
        unsigned secondary_inputs=0,secondary_pads=0,tertiary_inputs=0,tertiary_pads=0;
        unsigned full_mask=full_lookahead.valid?hb_pad_chord_mask_form(instance,full_lookahead,g_pad_next_chord_form):0;
        unsigned current_mask=current.valid?hb_pad_chord_mask(instance,current):0;
        unsigned effective_mask=effective.valid?hb_pad_chord_mask(instance,effective):0;
        unsigned lookahead_mask=ready&&lookahead.valid?hb_pad_chord_mask_form(instance,lookahead,g_pad_next_chord_form):0;
        unsigned long long output_low[32]={0},output_high[32]={0};int output_group[32],gap_colors[32],trail_targets[32];
        for(int i=0;i<32;i++)trail_targets[i]=-1;
        unsigned playing=0,flashing=0;unsigned long long sounding_low=0,sounding_high=0,flash_low=0,flash_high=0;
        for(int pitch=0;pitch<128;pitch++)if(instance->pad_flash_seconds[pitch]>0){if(pitch<64)flash_low|=1ULL<<pitch;else flash_high|=1ULL<<(pitch-64);}
        for(int pitch=0;pitch<128;pitch++)if(instance->pad_sounding[pitch]){
            if(pitch<64)sounding_low|=1ULL<<pitch;else sounding_high|=1ULL<<(pitch-64);
        }
        /* Identical inputs in this snapshot share their render. Pad slots go
           first because they also need full output groups and trail targets;
           the twelve pitch-class probes can then reuse those richer results.
           Nothing survives this read, so key/chord/motion changes cannot stale it. */
        struct {int valid,source,shift,row,target;unsigned mask,gap;
            unsigned long long single_low,single_high;} pad_preview[32]={0};
        for(int pass=0;pass<12+pad_count;pass++){
            int sample=hb_pad_preview_reuse?(pass<pad_count?pass+12:pass-pad_count):pass;
            /* Only actual pad slots consume trail targets. Pitch-class color
               probes and alternative harmony previews need colors alone. */
            preview.trail_enabled=instance->trail_enabled&&sample>=12;
            int source_note=sample<12?60+sample:pad_notes[sample-12];
            int gap_target=sample>=12?pad_targets[sample-12]:-1;
            int gap=gap_target>=0&&hb_approach_pad_enabled(instance);
            if(sample>=12)gap_colors[sample-12]=-1;
            if(gap){
                int row=instance->preview_rows[sample-12];source_note=row?(gap_target+32*row)%128:gap_target<64?gap_target+36:gap_target-36;preview.approach_rows.preview_row=row?row-1:3;
                preview.movy_pad_shift[source_note]=gap_target-source_note;
            }
            if(source_note<0){output_group[sample-12]=-1;continue;}
            int pitch_class=mod12(source_note);
            unsigned rendered_mask=0;
            int row=gap?preview.approach_rows.preview_row:-1,shift=gap?gap_target-source_note:0,cached=-1;
            for(int previous=0;hb_pad_preview_reuse&&previous<(sample>=12?sample-12:pad_count);previous++)
                if(pad_preview[previous].valid&&pad_preview[previous].source==source_note&&
                   pad_preview[previous].shift==shift&&pad_preview[previous].row==row){cached=previous;break;}
            if(cached>=0){
                rendered_mask=pad_preview[cached].mask;preview.preview_gap_mask=pad_preview[cached].gap;
                preview.preview_target=pad_preview[cached].target;
                preview.preview_single_low=pad_preview[cached].single_low;
                preview.preview_single_high=pad_preview[cached].single_high;
                if(sample>=12){output_low[sample-12]=output_low[cached];output_high[sample-12]=output_high[cached];}
            }else if(instance->role==0||instance->role==1){
                rendered_mask=hb_pad_render_mask(&preview,instance,rendering,source_note,1,
                    sample>=12?&output_low[sample-12]:0,sample>=12?&output_high[sample-12]:0);
            }
            if(sample>=12){
                int slot=sample-12;
                pad_preview[slot].valid=1;pad_preview[slot].source=source_note;
                pad_preview[slot].shift=shift;pad_preview[slot].row=row;
                pad_preview[slot].mask=rendered_mask;pad_preview[slot].gap=preview.preview_gap_mask;
                pad_preview[slot].target=preview.preview_target;
                pad_preview[slot].single_low=preview.preview_single_low;pad_preview[slot].single_high=preview.preview_single_high;
            }
            if(sample>=12){
                int slot=sample-12,effective_target=preview.preview_target;output_group[slot]=-1;
                preview.trail_enabled=0;
                if((sounding_low&preview.preview_single_low)||(sounding_high&preview.preview_single_high))playing|=1u<<slot;
                if((flash_low&preview.preview_single_low)||(flash_high&preview.preview_single_high))flashing|=1u<<slot;
                if(output_low[slot]||output_high[slot]){
                    output_group[slot]=slot;
                    for(int previous=0;previous<slot;previous++)
                        if(output_low[previous]==output_low[slot]&&output_high[previous]==output_high[slot]){output_group[slot]=output_group[previous];break;}
                }
                if(gap){
                    unsigned effective_gap=preview.preview_gap_mask;
                    if(rendered_mask&&next_mask&&!(rendered_mask&~next_mask)){
                        next_pads|=1u<<slot;
                        if(!(rendered_mask&~secondary_mask))secondary_pads|=1u<<slot;
                        if(!(rendered_mask&~tertiary_mask))tertiary_pads|=1u<<slot;
                    }
                    gap_colors[slot]=(rendered_mask&&current_mask&&!(rendered_mask&~current_mask)?1:0)
                        |(rendered_mask&&effective_mask&&!(rendered_mask&~effective_mask)?2:0)
                        |(rendered_mask&&effective_gap&&!(rendered_mask&~effective_gap)?4:0)
                        |(rendered_mask&&lookahead_mask&&!(rendered_mask&~lookahead_mask)?8:0)
                        |(rendered_mask&&full_mask&&!(rendered_mask&~full_mask)?16:0);
                    preview.movy_pad_shift[source_note]=0;
                }
                if(instance->trail_enabled){
                    /* History is heard pitch, projected through what each pad
                       plays now. Full-next background colors never relocate
                       trails; actual render lookahead already changes effective. */
                    trail_targets[slot]=effective_target;
                }
                continue;
            }
            unsigned input_bit=1u<<pitch_class;
            /* One current output serves every color, pulse and play overlay.
               Only the chord-tone set changes between current/next views. */
            if(rendered_mask&&!(rendered_mask&~current_mask))current_inputs|=input_bit;
            if(rendered_mask&&!(rendered_mask&~effective_mask))effective_inputs|=input_bit;
            if(rendered_mask&&!(rendered_mask&~lookahead_mask))lookahead_inputs|=input_bit;
            if(rendered_mask&&!(rendered_mask&~full_mask))full_inputs|=input_bit;
            if(rendered_mask&&!(rendered_mask&~next_mask)){
                next_inputs|=input_bit;
                if(!(rendered_mask&~secondary_mask))secondary_inputs|=input_bit;
                if(!(rendered_mask&~tertiary_mask))tertiary_inputs|=input_bit;
            }
            if(rendered_mask&&preview.preview_gap_mask&&!(rendered_mask&~preview.preview_gap_mask))scale_inputs|=input_bit;
            if(rendered_mask&&scale.valid&&rendered_mask==(1u<<mod12(scale.root_pc)))tonic_inputs|=input_bit;
        }
        int used=snprintf(buffer,(size_t)length,"%u,%u,%u,%d,%u,%d,%d,%d,%d,%d|tonic1,%u|full1,%d,%u",current_inputs,effective_inputs,scale_inputs,ready,lookahead_inputs,g_pad_settings[0],g_pad_settings[1],g_pad_settings[2],g_pad_settings[3],g_pad_settings[4],tonic_inputs,full_lookahead.valid!=0,full_inputs);
        if(used>=0&&used<length)used+=snprintf(buffer+used,(size_t)(length-used),"|learn1,%u",g_timeline_generation);
        /* Zero is an explicit Off state. Omitting this field selects the
           legacy whole-harmony animation in compatible Movy versions. */
        if(used>=0&&used<length)used+=snprintf(buffer+used,(size_t)(length-used),"|nextpulse1,%u,%u",next_inputs,next_pads);
        if((secondary_mask||tertiary_mask)&&used>=0&&used<length)used+=snprintf(buffer+used,(size_t)(length-used),"|nextranks1,%u,%u,%u,%u",secondary_inputs,secondary_pads,tertiary_inputs,tertiary_pads);
        if(used>=0&&used<length)used+=snprintf(buffer+used,(size_t)(length-used),"|playpads1,%u|playflash1,%u",playing,flashing);
        if(instance->trail_enabled&&pad_count&&used>=0&&used<length){
            used+=snprintf(buffer+used,(size_t)(length-used),"|targets1");
            for(int slot=0;slot<pad_count&&used<length;slot++)used+=snprintf(buffer+used,(size_t)(length-used),",%d",trail_targets[slot]);
        }
        if(pad_count&&used>=0&&used<length){
            used+=snprintf(buffer+used,(size_t)(length-used),"|outputs1");
            for(int slot=0;slot<pad_count&&used<length;slot++)used+=snprintf(buffer+used,(size_t)(length-used),",%d",output_group[slot]);
        }
        if(pad_count&&used>=0&&used<length){
            used+=snprintf(buffer+used,(size_t)(length-used),"|gapcolors1");
            for(int slot=0;slot<pad_count&&used<length;slot++)used+=snprintf(buffer+used,(size_t)(length-used),",%d",gap_colors[slot]);
        }
        /* Retain only pure assignment results from the private simulation.
           Repeated UI reads need no new solve until their musical inputs change. */
        instance->closest_assignments=preview.closest_assignments;
        return used;
    }
    return snprintf(buffer,(size_t)length,"%u,%u,%u,%d",
        current.valid?(unsigned)hb_pad_chord_mask(instance,current):0u,
        effective.valid?(unsigned)hb_pad_chord_mask(instance,effective):0u,
        scale.valid?(unsigned)scale.pitch_mask:0u,ready);
}
if(!strcmp(key,"receive_channel"))return snprintf(buffer,(size_t)length,"%s",RENDER_CH_OPTS[instance->source_channel+1]);

if(!strcmp(key,"master_transpose")){
    int reference=0;
    if(!hb_resolve_follower_reference_root(instance,&reference))return snprintf(buffer,(size_t)length,"--");
    if(!g_bus.global_transpose)return snprintf(buffer,(size_t)length,"As Played");
    return snprintf(buffer,(size_t)length,"%s",MASTER_ROOT_OPTS[1+mod12(reference+g_bus.global_transpose)]);
}

if((!strcmp(key,"state")||!strcmp(key,"pad_state_base"))&&(instance->play.rotate||instance->play.wrap||instance->play.mirror||instance->play.octave||instance->play.range||instance->play.scope||instance->play.bypass)){
    int used=get_param(value,"cp_state",buffer,length);
    if(used<0||used>=length)return used;
    return used+snprintf(buffer+used,(size_t)(length-used),";fp1,%d,%d,%d,%d,%d,%d,%d",instance->play.rotate,instance->play.wrap,instance->play.mirror,instance->play.octave,instance->play.range,instance->play.scope,instance->play.bypass);
}
if(((!strcmp(key,"state")||!strcmp(key,"pad_state_base"))||!strcmp(key,"cp_state"))&&(instance->player.config.note_phase||instance->player.config.phase!=2||hb_shared_dominant_scale()||instance->player.config.mode!=0||instance->player.config.size!=0||instance->player.config.inversion!=0||instance->player.config.voicing!=0||instance->player.config.playback!=0||instance->player.config.latch!=0||instance->player.config.order!=0||instance->player.config.rate!=2||instance->player.config.gate!=1||instance->player.config.spread!=0||instance->player.config.quality||instance->player.config.chromatic_quality)){
    int used=get_param(value,"base_state",buffer,length);
    if(used<0||used>=length)return used;
    return used+snprintf(buffer+used,(size_t)(length-used),";cp1,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d;ds1,%d;cq1,%d,%d;ph1,%d;np1,%d",instance->player.config.mode,instance->player.config.size,instance->player.config.inversion,instance->player.config.voicing,instance->player.config.playback,instance->player.config.latch,instance->player.config.order,instance->player.config.rate,instance->player.config.gate,instance->player.config.spread,hb_shared_dominant_scale(),instance->player.config.quality,instance->player.config.chromatic_quality,instance->player.config.phase,instance->player.config.note_phase);
}
if(!strcmp(key,"borrowed_scale"))return snprintf(buffer,(size_t)length,"%s",BORROWED_SCALE_OPTS[hb_shared_borrowed_scale()]);
if(!strcmp(key,"dominant_color"))return snprintf(buffer,(size_t)length,"%s",instance->dominant_color_latched?"Latch":instance->dominant_color_held?"Hold":"Off");
if(!strcmp(key,"dominant_color_family"))return snprintf(buffer,(size_t)length,"%s",DOMINANT_SCALE_OPTS[instance->dominant_color_family]);
if(!strcmp(key,"dominant_minor_scale"))return snprintf(buffer,(size_t)length,"%s",DOMINANT_SCALE_OPTS[hb_role_default(instance->role==0?0:1,HB_P_DOMINANT_MINOR)]);
if(!strcmp(key,"dominant_scale"))return snprintf(buffer,(size_t)length,"%s",DOMINANT_SCALE_OPTS[hb_shared_dominant_scale()]);
if(!strcmp(key,"chord_mode"))return snprintf(buffer,(size_t)length,"%s",CP_CHORD_MODE[instance->player.config.mode]);
if(!strcmp(key,"chord_quality"))return snprintf(buffer,(size_t)length,"%s",CP_CHORD_QUALITY[instance->player.config.quality]);
if(!strcmp(key,"chromatic_quality"))return snprintf(buffer,(size_t)length,"%s",CP_CHROMATIC_QUALITY[instance->player.config.chromatic_quality]);
if(!strcmp(key,"chord_form"))return snprintf(buffer,(size_t)length,"%s",CP_CHORD_FORM[instance->player.config.size]);
if(!strcmp(key,"chord_inversion"))return snprintf(buffer,(size_t)length,"%s",CP_CHORD_INVERSION[instance->player.config.inversion]);
if(!strcmp(key,"chord_voicing"))return snprintf(buffer,(size_t)length,"%s",CP_CHORD_VOICING[instance->player.config.voicing]);
if(!strcmp(key,"arp_playback"))return snprintf(buffer,(size_t)length,"%s",CP_ARP_PLAYBACK[instance->player.config.playback]);
if(!strcmp(key,"arp_clear_harmony"))return snprintf(buffer,(size_t)length,"%s",instance->player.config.clear_harmony?"On":"Off");
if(!strcmp(key,"arp_hold"))return snprintf(buffer,(size_t)length,"%s",CP_ARP_HOLD[instance->player.config.latch]);
if(!strcmp(key,"arp_start"))return snprintf(buffer,(size_t)length,"%s",CP_ARP_START[instance->player.config.start]);
if(!strcmp(key,"arp_order"))return snprintf(buffer,(size_t)length,"%s",CP_ARP_ORDER[instance->player.config.order]);
if(!strcmp(key,"arp_phase_range")){int limit=hb_arp_phase_limit(instance->player.config.rate);return snprintf(buffer,(size_t)length,"-%d to +%d",limit,limit);}
if(!strcmp(key,"arp_note_phase"))return snprintf(buffer,(size_t)length,"%d",instance->player.config.note_phase);
if(!strcmp(key,"arp_phase"))return snprintf(buffer,(size_t)length,"%s",CP_ARP_PHASE[instance->player.config.phase]);
if(!strcmp(key,"arp_rate"))return snprintf(buffer,(size_t)length,"%s",CP_ARP_RATE[instance->player.config.rate]);
if(!strcmp(key,"arp_gate"))return snprintf(buffer,(size_t)length,"%s",CP_ARP_GATE[instance->player.config.gate]);
if(!strcmp(key,"strum_spread")){int spread=instance->player.config.spread;return spread<0?snprintf(buffer,(size_t)length,"%s",BUFFER_DIVISIONS[-spread-1]):snprintf(buffer,(size_t)length,"%d ms",spread);}
if(!strcmp(key,"arp_clear"))return snprintf(buffer,(size_t)length,"Ready");
if(!strcmp(key,"pretranspose_root")){
    hb_harmony_t final_harmony=hb_render_harmony(instance);
    if(!final_harmony.valid)return snprintf(buffer,(size_t)length,"--");
    hb_harmony_t pre=hb_transpose_harmony(final_harmony,-g_bus.global_transpose);
    return snprintf(buffer,(size_t)length,"%s",hb_pc_display(pre.root_pc,pre));
}
if(!strcmp(key,"final_root")){
    hb_harmony_t final_harmony=hb_render_harmony(instance);
    return snprintf(buffer,(size_t)length,"%s",final_harmony.valid?hb_pc_display(final_harmony.root_pc,final_harmony):"--");
}
if(!strcmp(key,"detected_chord_form")){
    hb_harmony_t detected=hb_render_harmony(instance);
    if(!detected.valid)return snprintf(buffer,(size_t)length,"--");
    char name[48],degrees[16];hb_format_harmony(name,sizeof(name),detected);
    unsigned roles=hb_cp_detected_roles(detected.root_pc,hb_harmony_detected_mask(detected));
    int used=0;static const int order[]={0,2,4,6,1,3,5};
    for(int index=0;index<7;index++)if(roles&(1u<<order[index])){
        int role=order[index],degree=role+1;
        if(role==1&&(roles&(1u<<2)))degree=9;
        if(role==3&&(roles&(1u<<2)))degree=11;
        if(role==5&&(roles&(1u<<6)))degree=13;
        used+=snprintf(degrees+used,sizeof(degrees)-(size_t)used,"%d",degree);
    }
    degrees[used]=0;
    return snprintf(buffer,(size_t)length,"%s %s",name,degrees);
}
if(!strcmp(key,"final_harmony"))return hb_format_harmony(buffer,length,hb_render_harmony(instance));
if(!strcmp(key,"next_anti_buffer_ms")){
    int setting=instance->next_anti_buffer_ms;
    return setting<0?snprintf(buffer,(size_t)length,"%s",BUFFER_DIVISIONS[-setting-1]):snprintf(buffer,(size_t)length,"%d ms",setting);
}
if(!strcmp(key,"next_lookahead")){int index=instance->next_lookahead;return snprintf(buffer,(size_t)length,"%s",hb_next_lookahead_label(index));}
if(!strcmp(key,"next_model"))return snprintf(buffer,(size_t)length,"%s",g_movy_blocked==1?"No clips":g_movy_blocked==2?"Cycle too long":g_movy_blocked==3?"Non-repeating":g_movy_blocked==4?"Too many changes":g_bus.next_model_locked?"Locked":"Learning");
if(!strcmp(key,"next_shift")){
    double phase=hb_next_phase(hb_clip_playhead());
    int shifted=hb_render_shift_ready_for(instance)&&hb_next_model_event_for_phase_for(instance,phase,1)!=hb_next_model_event_for_phase_for(instance,phase,0);
    return snprintf(buffer,(size_t)length,"%s",shifted?(hb_next_immediate_for(instance)?"Immediate":hb_next_after_beats_for(instance)>=0.0?"After":hb_next_lookahead_beats_for(instance)<0.0?"Late":"Early"):"Live");
}
if(!strcmp(key,"next_loop_length")){double beats=hb_next_loop_length();if(beats<=0.0)return snprintf(buffer,(size_t)length,"--");if(((long)(beats+0.5))%4==0&&beats>=4.0)return snprintf(buffer,(size_t)length,"%.2f Bars",beats/4.0);return snprintf(buffer,(size_t)length,"%.2f Beats",beats);}
if(!strcmp(key,"next_position")){double beats=hb_next_loop_length();if(beats<=0.0||!g_bus.have_last_clip_playhead)return snprintf(buffer,(size_t)length,"--");double phase=hb_next_phase(g_bus.last_clip_playhead);if(((long)(beats+0.5))%4==0&&beats>=4.0)return snprintf(buffer,(size_t)length,"%.2f Bars",phase/4.0);return snprintf(buffer,(size_t)length,"%.2f Beats",phase);}
if(!strcmp(key,"next_harmony")){if(!g_bus.next_model_locked||g_bus.next_model_count<=0)return snprintf(buffer,(size_t)length,"--");double playhead=g_bus.have_last_clip_playhead?g_bus.last_clip_playhead:hb_clip_playhead();int index=hb_next_upcoming_event(hb_next_phase(playhead));if(index<0)return snprintf(buffer,(size_t)length,"--");return hb_format_harmony(buffer,length,g_bus.next_model[index].harmony);}
if(!strcmp(key,"next_reset"))return snprintf(buffer,(size_t)length,"Off");if(!strcmp(key,"version"))return snprintf(buffer,(size_t)length,"%s",HB_VERSION);if(!strcmp(key,"foll_mod_blank2"))return snprintf(buffer,(size_t)length,"");if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");if(!strcmp(key,"approach_status")){int active=hb_active_approach(instance);return snprintf(buffer,(size_t)length,"%s",APPROACH_OPTS[(active>=0&&active<3)?active:1]);}if(!strcmp(key,"touch_hint"))return snprintf(buffer,(size_t)length,"K6 OFF K7 UP K8 DN");if(!strcmp(key,"approach_reset"))return snprintf(buffer,(size_t)length,"%s",instance->motion.enclosure?hb_mo_pending_status(&instance->motion):instance->approach_pad_armed==HB_APPROACH_SCALE_ABOVE?"Armed Above":instance->approach_pad_armed==HB_APPROACH_CHROM_BELOW?"Armed Below":"Off");if(!strcmp(key,"approach_scale_next"))return snprintf(buffer,(size_t)length,"%s",(instance->motion.gesture_down&(1ULL<<HB_MOTION_LANES))?"Held":(instance->approach_pad_armed==HB_APPROACH_SCALE_ABOVE||(instance->motion.enclosure&&!instance->motion.tap_started&&(instance->motion.tap_mask&2)))?"Armed":"Off");if(!strcmp(key,"approach_chrom_next"))return snprintf(buffer,(size_t)length,"%s",(instance->motion.gesture_down&(1ULL<<(HB_MOTION_LANES+1)))?"Held":(instance->approach_pad_armed==HB_APPROACH_CHROM_BELOW||(instance->motion.enclosure&&!instance->motion.tap_started&&(instance->motion.tap_mask&1)))?"Armed":"Off");if(!strcmp(key,"mod_scale_above"))return snprintf(buffer,(size_t)length,"%s",instance->approach_control==2?"On":"Off");if(!strcmp(key,"mod_chrom_below"))return snprintf(buffer,(size_t)length,"%s",instance->approach_control==0?"On":"Off");if(!strcmp(key,"approach"))return snprintf(buffer,(size_t)length,"%s",APPROACH_OPTS[(instance->approach_control>=0&&instance->approach_control<3)?instance->approach_control:1]);if(!strcmp(key,"approach_mode")){int mode=instance->approach_mode;if(mode<0||mode>2)mode=0;return snprintf(buffer,(size_t)length,"%s",APPROACH_MODE_OPTS[mode]);}if(!strcmp(key,"approach_below_pad"))return snprintf(buffer,(size_t)length,"%d",instance->approach_below_held?1:0);if(!strcmp(key,"approach_above_pad"))return snprintf(buffer,(size_t)length,"%d",instance->approach_above_held?1:0);if(!strcmp(key,"retrigger_held"))return snprintf(buffer,(size_t)length,"%s",RETRIGGER_OPTS[instance->retrigger_held?1:0]);if(!strcmp(key,"follow_lookahead_ms"))return snprintf(buffer,(size_t)length,"%d",instance->follow_lookahead_ms);if(!strcmp(key,"conductor_this_1")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_local_conductor_note(instance,0),harmony,b,sizeof(b)));}
if(!strcmp(key,"conductor_this_2")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_local_conductor_note(instance,1),harmony,b,sizeof(b)));}
if(!strcmp(key,"conductor_this_3")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_local_conductor_note(instance,2),harmony,b,sizeof(b)));}
if(!strcmp(key,"conductor_this_4")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_local_conductor_note(instance,3),harmony,b,sizeof(b)));}
if(!strcmp(key,"conductor_this_count"))return snprintf(buffer,(size_t)length,"%d",hb_local_conductor_count(instance));
if(!strcmp(key,"conductor_all_1")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_shared_conductor_note(0),harmony,b,sizeof(b)));}
if(!strcmp(key,"conductor_all_2")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_shared_conductor_note(1),harmony,b,sizeof(b)));}
if(!strcmp(key,"conductor_all_3")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_shared_conductor_note(2),harmony,b,sizeof(b)));}
if(!strcmp(key,"conductor_all_4")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_shared_conductor_note(3),harmony,b,sizeof(b)));}
if(!strcmp(key,"conductor_all_count")){uint8_t n[64];return snprintf(buffer,(size_t)length,"%d",hb_observed_notes(0,n,64));}
if(!strcmp(key,"follower_this_1")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_local_follower_note(instance,0),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_this_2")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_local_follower_note(instance,1),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_this_3")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_local_follower_note(instance,2),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_this_4")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_local_follower_note(instance,3),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_this_count"))return snprintf(buffer,(size_t)length,"%d",hb_local_follower_count(instance));
if(!strcmp(key,"follower_all_1")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_aggregate_follower_observation(0,0),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_all_2")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_aggregate_follower_observation(1,0),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_all_3")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_aggregate_follower_observation(2,0),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_all_4")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_aggregate_follower_observation(3,0),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_all_count"))return snprintf(buffer,(size_t)length,"%d",hb_aggregate_follower_unique_count());
if(!strcmp(key,"follower_this_role_1")){int n=hb_nth_local_follower_note(instance,0);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note(instance,n));}
if(!strcmp(key,"follower_this_role_2")){int n=hb_nth_local_follower_note(instance,1);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note(instance,n));}
if(!strcmp(key,"follower_this_role_3")){int n=hb_nth_local_follower_note(instance,2);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note(instance,n));}
if(!strcmp(key,"follower_this_role_4")){int n=hb_nth_local_follower_note(instance,3);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note(instance,n));}
if(!strcmp(key,"follower_all_role_1")){const Inst *owner=0;int raw=hb_nth_aggregate_follower_observation(0,&owner);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note((Inst*)owner,raw));}
if(!strcmp(key,"follower_all_role_2")){const Inst *owner=0;int raw=hb_nth_aggregate_follower_observation(1,&owner);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note((Inst*)owner,raw));}
if(!strcmp(key,"follower_all_role_3")){const Inst *owner=0;int raw=hb_nth_aggregate_follower_observation(2,&owner);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note((Inst*)owner,raw));}
if(!strcmp(key,"follower_all_role_4")){const Inst *owner=0;int raw=hb_nth_aggregate_follower_observation(3,&owner);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note((Inst*)owner,raw));}
if(!strcmp(key,"follower_note_1_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,0),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_note_2_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,1),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_note_3_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,2),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_note_4_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,3),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_role_1"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,0));
if(!strcmp(key,"follower_role_2"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,1));
if(!strcmp(key,"follower_role_3"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,2));
if(!strcmp(key,"follower_role_4"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,3));
if(!strcmp(key,"follower_active_count"))return snprintf(buffer,(size_t)length,"%d",hb_follower_analysis_count(instance));if((!strcmp(key,"state")||!strcmp(key,"pad_state_base"))||!strcmp(key,"base_state")||!strcmp(key,"cp_state"))return snprintf(buffer,(size_t)length,"hb16,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",instance->role,instance->mode,instance->map_target,g_bus.inference_window_ms,hb_global_root_policy(),hb_global_explicit_root(),g_bus.global_input_root,g_bus.global_transpose,instance->follower_split_map,g_bus.stability,g_bus.accidentals,instance->render_channel,instance->source_channel,g_bus.chord_timing,g_bus.context,g_bus.clip_context,instance->follow_lookahead_ms,instance->retrigger_held,g_bus.anticipation,instance->boundary_buffer_ms,g_bus.analysis_release_ms,instance->content_map,instance->travel_map,hb_shared_follower_scale(),g_bus.quant_timing);if(!strcmp(key,"track_role")||!strcmp(key,"role"))return snprintf(buffer,(size_t)length,"%s",ROLE_OPTS[instance->role]);if(!strcmp(key,"clip_slot"))return snprintf(buffer,(size_t)length,"%s",CLIP_SLOT_OPTS[g_bus.clip_slot]);if(!strcmp(key,"sensor_sources"))return snprintf(buffer,(size_t)length,"%s","Realtime");if(!strcmp(key,"realtime_route"))return snprintf(buffer,(size_t)length,"%s","Set Track MIDI Out -> Schwung");if(!strcmp(key,"clip_context"))return snprintf(buffer,(size_t)length,"%s","Realtime Only");if(!strcmp(key,"mode"))return snprintf(buffer,(size_t)length,"%s",MODE_OPTS[instance->mode]);
if(!strcmp(key,"content_map")){
    static const char *opts[]={"Chord","Scale","Free","135","1357","12357","12356","Non-Avoid","123567"};
    return snprintf(buffer,(size_t)length,"%s",opts[instance->content_map]);
}
if(!strcmp(key,"chromatic_map"))return snprintf(buffer,(size_t)length,"%s",MO_SWITCH[hb_chromatic_travel(instance)]);
if(!strcmp(key,"travel_map")){static const char *opts[]={"Relative","Closest Scale Tone","Upward","Closest Split","Downward","Direct","Closest Split Chromatic","None","Nearest Octave","Closest Chord Tone"};int travel=instance->travel_map;if(travel<0||travel>9)travel=0;if(travel==6)travel=3;if(travel==5)travel=7;return snprintf(buffer,(size_t)length,"%s",opts[travel]);}if(!strcmp(key,"split_map")){static const char *opts[]={"Harm. / Out","135 / 2467","1357 / 246","Act. / Out"};int split=instance->follower_split_map;if(split<0||split>3)split=0;return snprintf(buffer,(size_t)length,"%s",opts[split]);}
if(!strcmp(key,"map_target"))return snprintf(buffer,(size_t)length,"%s",MAP_TARGET_OPTS[instance->map_target]);if(!strcmp(key,"source_channel"))return snprintf(buffer,(size_t)length,"%s",SOURCE_CH_OPTS[instance->source_channel+1]);if(!strcmp(key,"resolved_source_channel"))return snprintf(buffer,(size_t)length,"%d",instance->resolved_source_channel);if(!strcmp(key,"render_channel"))return snprintf(buffer,(size_t)length,"%s",RENDER_CH_OPTS[instance->render_channel+1]);if(!strcmp(key,"quant_timing"))return snprintf(buffer,(size_t)length,"%s",QUANT_GRID_OPTS[g_bus.quant_timing]);if(!strcmp(key,"chord_timing"))return snprintf(buffer,(size_t)length,"%s",TIMING_OPTS[g_bus.chord_timing]);if(!strcmp(key,"anticipation"))return snprintf(buffer,(size_t)length,"%s",ANTICIPATION_OPTS[g_bus.anticipation]);if(!strcmp(key,"boundary_buffer_ms")){int setting=hb_effective_follower_buffer_for(instance);if(setting<0)return snprintf(buffer,(size_t)length,"%s",BUFFER_DIVISIONS[-setting-1]);return snprintf(buffer,(size_t)length,"%d ms",setting);}if(!strcmp(key,"analysis_release_ms"))return snprintf(buffer,(size_t)length,"%d",g_bus.analysis_release_ms);if(!strcmp(key,"follower_scale"))return snprintf(buffer,(size_t)length,"%s",FOLLOWER_SCALE_OPTS[hb_shared_follower_scale()]);if(!strcmp(key,"context"))return snprintf(buffer,(size_t)length,"%s",CONTEXT_OPTS[g_bus.context]);if(!strcmp(key,"chord_timescale"))return snprintf(buffer,(size_t)length,"%s",TIMESCALE_OPTS[g_bus.chord_timescale]);if(!strcmp(key,"stability"))return snprintf(buffer,(size_t)length,"%s",STABILITY_OPTS[g_bus.stability]);if(!strcmp(key,"accidentals"))return snprintf(buffer,(size_t)length,"%s",ACCIDENTAL_OPTS[g_bus.accidentals]);if(!strcmp(key,"root_policy"))return snprintf(buffer,(size_t)length,"%s",POLICY_OPTS[g_bus.global_root_policy]);if(!strcmp(key,"explicit_root"))return snprintf(buffer,(size_t)length,"%s",PC_OPTS[g_bus.global_explicit_root]);if(!strcmp(key,"input_root"))return snprintf(buffer,(size_t)length,"%s",PC_OPTS[g_bus.global_input_root]);if(!strcmp(key,"transpose"))return snprintf(buffer,(size_t)length,"%d",g_bus.global_transpose);if(!strcmp(key,"window_ms"))return snprintf(buffer,(size_t)length,"%d",g_bus.inference_window_ms);if(!strcmp(key,"detected_root"))return snprintf(buffer,(size_t)length,"%s",harmony.valid?hb_pc_display(harmony.root_pc,harmony):"--");if(!strcmp(key,"detected_bass"))return snprintf(buffer,(size_t)length,"%s",harmony.valid?hb_pc_display(harmony.bass_pc,harmony):"--");if(!strcmp(key,"detected_quality"))return snprintf(buffer,(size_t)length,"%d",harmony.valid?harmony.chord_index+1:0);if(!strcmp(key,"confidence"))return snprintf(buffer,(size_t)length,"%d",harmony.valid?harmony.confidence:0);if(!strcmp(key,"resolved_root"))return snprintf(buffer,(size_t)length,"%d",reference_root(instance));if(!strcmp(key,"harmony"))return hb_format_harmony(buffer,length,harmony);if(!strcmp(key,"candidate_harmony"))return hb_format_harmony(buffer,length,instance->candidate_harmony);if(!strcmp(key,"pitch_mask"))return snprintf(buffer,(size_t)length,"%u",(unsigned)harmony.pitch_mask);if(!strcmp(key,"rx_count"))return snprintf(buffer,(size_t)length,"%u",instance->rx_count);if(!strcmp(key,"note_on_count"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"note_off_count"))return snprintf(buffer,(size_t)length,"%u",instance->note_off_count);if(!strcmp(key,"last_note"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"last_status"))return snprintf(buffer,(size_t)length,"%d",instance->last_status);if(!strcmp(key,"last_velocity"))return snprintf(buffer,(size_t)length,"%d",instance->last_velocity);if(!strcmp(key,"diag_midi_events"))return snprintf(buffer,(size_t)length,"%u",instance->rx_count);if(!strcmp(key,"diag_note_ons"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"diag_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"diag_recv_ch")){int recv=hb_monitor_channel(instance);return snprintf(buffer,(size_t)length,"%d",(recv>=0&&recv<16)?recv+1:-1);}if(!strcmp(key,"diag_event_ch")){int type=instance->last_status&0xF0;int event_ch=(type==0x80||type==0x90)?(instance->last_status&15):-1;return snprintf(buffer,(size_t)length,"%d",event_ch>=0?event_ch+1:-1);}if(!strcmp(key,"live_press_count"))return snprintf(buffer,(size_t)length,"%u",instance->live_press_count);if(!strcmp(key,"rt_note_count"))return snprintf(buffer,(size_t)length,"%d",hb_held_count(instance));if(!strcmp(key,"rt_note_ons"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"rt_note_offs"))return snprintf(buffer,(size_t)length,"%u",instance->note_off_count);if(!strcmp(key,"rt_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"rt_last_channel")){int t=instance->last_status&0xF0;return snprintf(buffer,(size_t)length,"%d",(t==0x80||t==0x90)?(instance->last_status&15):-1);}if(!strcmp(key,"clip_active_count")){uint8_t temp_notes[32];int temp_count=hb_clip_active_notes(temp_notes,32);return snprintf(buffer,(size_t)length,"%d",temp_count);}if(!strcmp(key,"active_count"))return snprintf(buffer,(size_t)length,"%d",hb_conductor_analysis_count(instance));if(!strcmp(key,"active_note_list"))return hb_format_active_notes(instance,buffer,length);if(!strcmp(key,"callback_active_notes"))return snprintf(buffer,(size_t)length,"%d",active_notes(instance,(uint8_t[128]){0}));if(!strcmp(key,"pending_releases")){int n=0;for(int i=0;i<128;i++)if(instance->pending_off_frames[i]>0)n++;return snprintf(buffer,(size_t)length,"%d",n);}if(!strcmp(key,"active_pc_mask"))return snprintf(buffer,(size_t)length,"%u",hb_active_pc_mask(instance));if(!strcmp(key,"active_note_1"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,0));if(!strcmp(key,"active_note_2"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,1));if(!strcmp(key,"active_note_3"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,2));if(!strcmp(key,"active_note_4"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,3));if(!strcmp(key,"active_note_4_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,3),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"active_note_3_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,2),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"active_note_2_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,1),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"active_note_1_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,0),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"clip_stage")){static const char *names[]={"idle","no active set","active set","no conductor","conductor found","no Song.abl","Song.abl found","no track","track found","no clip slots","no clip","clip found","no notes array","empty clip","OK"};int stage=g_bus.clip_stage;if(stage<0||stage>14)stage=0;return snprintf(buffer,(size_t)length,"%s",names[stage]);}if(!strcmp(key,"cache_rev"))return snprintf(buffer,(size_t)length,"%u",g_bus.cache_rev);if(!strcmp(key,"sense_rev"))return snprintf(buffer,(size_t)length,"%u",g_bus.sense_rev);if(!strcmp(key,"clip_track"))return snprintf(buffer,(size_t)length,"%d",g_bus.clip_context?g_bus.clip_track:-1);if(!strcmp(key,"clip_note_count"))return snprintf(buffer,(size_t)length,"%d",g_bus.clip_context?g_bus.clip_note_count:0);if(!strcmp(key,"clip_playhead"))return g_bus.clip_context?snprintf(buffer,(size_t)length,"%.3f",hb_clip_playhead()):snprintf(buffer,(size_t)length,"%s","realtime");if(!strcmp(key,"infer_note_count")){uint8_t notes[128];return snprintf(buffer,(size_t)length,"%d",active_notes(instance,notes));}if(!strcmp(key,"bus_seq"))return snprintf(buffer,(size_t)length,"%u",__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE));if(!strcmp(key,"raw_event_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_event_count);if(!strcmp(key,"raw_note_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_count);if(!strcmp(key,"raw_note_on_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_on_count);if(!strcmp(key,"raw_note_off_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_off_count);if(!strcmp(key,"raw_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_note);if(!strcmp(key,"raw_last_status"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_status);if(!strcmp(key,"raw_last_velocity"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_velocity);if(!strcmp(key,"raw_last_channel"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_channel);if(!strcmp(key,"raw_track_match"))return snprintf(buffer,(size_t)length,"%d",(g_bus.clip_track>=0&&instance->raw_last_channel==g_bus.clip_track)?1:0);if(!strcmp(key,"raw_last_cable"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_cable);if(!strcmp(key,"inject_available"))return snprintf(buffer,(size_t)length,"%d",(g_host&&g_host->midi_inject_to_move)?1:0);if(!strcmp(key,"render_count"))return snprintf(buffer,(size_t)length,"%u",instance->render_count);if(!strcmp(key,"render_fail_count"))return snprintf(buffer,(size_t)length,"%u",instance->render_fail_count);if(!strcmp(key,"render_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->render_last_note);if(!strcmp(key,"follower_last_in"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"follower_last_out"))return snprintf(buffer,(size_t)length,"%d",instance->render_channel>=0?instance->render_last_note:(instance->last_note>=0&&instance->last_note<128?instance->mapped[instance->last_note]:-1));if(!strcmp(key,"follower_root_policy")||!strcmp(key,"follower_source_policy"))return snprintf(buffer,(size_t)length,"%s",FOLLOWER_SOURCE_POLICY_OPTS[hb_global_root_policy()]);if(!strcmp(key,"follower_explicit_root")||!strcmp(key,"follower_source_root"))return snprintf(buffer,(size_t)length,"%s",PC_OPTS[hb_global_explicit_root()]);if(!strcmp(key,"inferred_root")){int root=0;if(!hb_infer_follower_root(instance,&root))return snprintf(buffer,(size_t)length,"%s","--");return snprintf(buffer,(size_t)length,"%s",PC_OPTS[root]);}if(!strcmp(key,"role_reference_root")||!strcmp(key,"used_root")){int root=0;if(!hb_resolve_follower_reference_root(instance,&root))return snprintf(buffer,(size_t)length,"%s","--");return snprintf(buffer,(size_t)length,"%s",PC_OPTS[root]);}if(!strcmp(key,"source_note_1")){char note_buf[8];int note=hb_nth_aggregate_conductor_note(0);return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(note,harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"source_of_1"))return hb_format_conductor_source(hb_nth_aggregate_conductor_note(0),buffer,length);if(!strcmp(key,"source_note_2")){char note_buf[8];int note=hb_nth_aggregate_conductor_note(1);return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(note,harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"source_of_2"))return hb_format_conductor_source(hb_nth_aggregate_conductor_note(1),buffer,length);if(!strcmp(key,"source_note_3")){char note_buf[8];int note=hb_nth_aggregate_conductor_note(2);return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(note,harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"source_of_3"))return hb_format_conductor_source(hb_nth_aggregate_conductor_note(2),buffer,length);if(!strcmp(key,"source_note_4")){char note_buf[8];int note=hb_nth_aggregate_conductor_note(3);return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(note,harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"source_of_4"))return hb_format_conductor_source(hb_nth_aggregate_conductor_note(3),buffer,length);if(!strcmp(key,"monitor_status"))return snprintf(buffer,(size_t)length,"%s",g_monitor?"ACTIVE":"ABSENT");
if(!strcmp(key,"monitor_generation"))return snprintf(buffer,(size_t)length,"%u",g_monitor?(unsigned)g_monitor->generation:0u);
if(!strcmp(key,"monitor_events"))return snprintf(buffer,(size_t)length,"%u",g_monitor?(unsigned)g_monitor->external_note_events:0u);
if(!strcmp(key,"v87_marker"))return snprintf(buffer,(size_t)length,"%s","HB264");
if(!strcmp(key,"v87_tick"))return snprintf(buffer,(size_t)length,"%u",g_bus.global_tick_count);
if(!strcmp(key,"v87_conductor_count")){uint8_t notes[64];return snprintf(buffer,(size_t)length,"%d",hb_observed_notes(0,notes,64));}
if(!strcmp(key,"v87_conductor_1")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_observed_note(0,0),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"v87_conductor_2")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_observed_note(0,1),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"v87_conductor_3")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_observed_note(0,2),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"v87_conductor_4")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_observed_note(0,3),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"v87_follower_count"))return snprintf(buffer,(size_t)length,"%d",hb_aggregate_follower_count());
if(!strcmp(key,"v87_process"))return snprintf(buffer,(size_t)length,"%u",g_bus.global_process_count);
if(!strcmp(key,"v87_raw_notes"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_count);
if(!strcmp(key,"diag_marker"))return snprintf(buffer,(size_t)length,"%s","HB187");
if(!strcmp(key,"diag_ui_instance"))return snprintf(buffer,(size_t)length,"%d",hb_instance_index(instance)+1);
if(!strcmp(key,"diag_global_process"))return snprintf(buffer,(size_t)length,"%u",g_bus.global_process_count);
if(!strcmp(key,"diag_global_note_events"))return snprintf(buffer,(size_t)length,"%u",g_bus.global_note_event_count);
if(!strcmp(key,"diag_global_accepted"))return snprintf(buffer,(size_t)length,"%u",g_bus.global_accepted_note_count);
if(!strcmp(key,"diag_global_last_note"))return snprintf(buffer,(size_t)length,"%d",g_bus.global_last_note);
if(!strcmp(key,"diag_global_last_ch"))return snprintf(buffer,(size_t)length,"%d",g_bus.global_last_channel>=0?g_bus.global_last_channel+1:-1);
if(!strcmp(key,"diag_global_last_inst"))return snprintf(buffer,(size_t)length,"%d",g_bus.global_last_instance>=0?g_bus.global_last_instance+1:-1);
if(!strcmp(key,"trace_1"))return hb_format_trace_event(instance,0,buffer,length);if(!strcmp(key,"trace_2"))return hb_format_trace_event(instance,1,buffer,length);if(!strcmp(key,"trace_3"))return hb_format_trace_event(instance,2,buffer,length);if(!strcmp(key,"trace_4"))return hb_format_trace_event(instance,3,buffer,length);if(!strcmp(key,"trace_5"))return hb_format_trace_event(instance,4,buffer,length);if(!strcmp(key,"trace_6"))return hb_format_trace_event(instance,5,buffer,length);if(!strcmp(key,"trace_7"))return hb_format_trace_event(instance,6,buffer,length);if(!strcmp(key,"trace_8"))return hb_format_trace_event(instance,7,buffer,length);if(!strcmp(key,"trace_note_ons"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"trace_note_offs"))return snprintf(buffer,(size_t)length,"%u",instance->note_off_count);if(!strcmp(key,"chain_params")){int size=(int)strlen(CHAIN_PARAMS);if(size>=length)return -1;memcpy(buffer,CHAIN_PARAMS,(size_t)size+1);return size;}return -1;}
__attribute__((visibility("default")))
int move_midi_fx_process_with_source(void *value,
                                     const uint8_t *input,int length,int source,
                                     uint8_t output[][3],int lengths[],int max_output){
    /* Low byte is Schwung source ID: 0=INTERNAL, 2=EXTERNAL MIDI_OUT,
       3=HOST-generated. The HarmonyBus Chain patch stores recv_channel+1
       in bits 8..12, giving every MIDI-FX instance its parent track identity
       without changing this optional extension's ABI. */
    if(!value||!input||length<1)return 0;
    Inst *instance=(Inst*)value;
    int base_source=source&0xFF;
    int tagged_channel=((source>>8)&0x1F)-1;
    if(tagged_channel>=0&&tagged_channel<16)
        instance->resolved_source_channel=tagged_channel;
    int status=input[0]&0xF0;
    int is_note=(length>=3&&(status==0x80||status==0x90));
    if(is_note&&base_source!=2){
        return pass(input,length,output,lengths,max_output);
    }
    return process(value,input,length,output,lengths,max_output);
}
/* Reset only this instance. Shared role/scale/motif settings remain owned by the set. */
static void hb_reset_track_defaults(Inst *instance,int track){
    if(track<0||track>=16||instance->movy_track!=track)return;
    int capabilities=instance->motion.host_capabilities,rhythm_host=instance->rhythm_host,layout=instance->approach_layout;
    uint8_t pending[128];memcpy(pending,instance->role_flush_pending,sizeof(pending));
    hb_stop_instance_note_state(instance);
    for(int pitch=0;pitch<128;pitch++)pending[pitch]|=instance->role_flush_pending[pitch];
    uint8_t output[128][3];int lengths[128];
    for(int pass=0;pass<128&&(instance->motif.cancel||instance->player.sounding_count||instance->motion_local.count);pass++){
        int count=hb_mt_tick(instance,output,lengths,128);
        count+=hb_player_tick(instance,output+count,lengths+count,128-count);
        count+=hb_motion_local_drain(instance,output+count,lengths+count,128-count);
        for(int event=0;event<count;event++)if(lengths[event]>=3&&(output[event][0]&0xf0)==0x80)pending[output[event][1]&127]=1;
    }
    int index=(int)(instance-g_pool);
    memset(&g_sc_key_sequence[index],0,sizeof(g_sc_key_sequence[0]));g_sc_key_audition[index].on=g_sc_key_request[index].on=0;
    memset(g_sc_live[index],0,sizeof(g_sc_live[index]));g_sc_manual[index]=g_sc_latch[index]=g_sc_lane[index]=0;
    memset(g_sc_replay[track],0,sizeof(g_sc_replay[track]));g_sc_dirty=1;
    hb_timeline_forget_track(instance);
    hb_init_instance(instance);
    instance->movy_track=track;instance->motion.host_capabilities=capabilities;instance->rhythm_host=rhythm_host;instance->approach_layout=layout;
    hb_mo_defaults(&instance->motion);instance->motion.host_capabilities=capabilities;
    instance->role=track>=12?3:track%4==0?0:1;
    instance->content_map=instance->role==1?1:0;
    instance->render_channel=track%4==0?2:track%4;instance->source_channel=-1;
    instance->player.config.mode=instance->role==0?1:0;instance->player.config.phase=2;
    instance->policy_initialized=0;instance->policy_overrides=0;hb_role_sync(instance);
    memcpy(instance->role_flush_pending,pending,sizeof(pending));instance->dirty=1;
    hb_sc_resolve(instance);hb_override_sync();
}
static void set_param(void *value,const char *key,const char *parameter){
    Inst *instance=(Inst*)value;if(!instance||!key||!parameter)return;
    if(!strcmp(key,"harm_play_release_control")){
        hb_chord_player *p=&instance->player;
        if(!strcmp(parameter,"Down")){p->release_held=1;p->release_used=p->release_turned=0;}
        else if(!strcmp(parameter,"LatchOn")||!strcmp(parameter,"LatchOff")){
            p->release_latched=!strcmp(parameter,"LatchOn");p->release_armed=p->release_held=0;p->release_turned=1;
        }else if(!strncmp(parameter,"Up,",3)){
            if(p->release_held&&!p->release_used&&!p->release_turned&&!p->release_latched&&parse_i(parameter+3,1000)<g_hb_hold_ms)p->release_armed=!p->release_armed;
            p->release_held=0;
        }else if(!strcmp(parameter,"Cancel"))p->release_held=0;
        else if(!strcmp(parameter,"Off"))p->release_held=p->release_armed=p->release_latched=0;
        return;
    }
    if(!strcmp(key,"harm_play_release_harmony")){instance->player.release_follow_harmony=!strcmp(parameter,"Follow Harmony");return;}
    if(!strcmp(key,"harm_play_release")){
        int setting=hb_cp_clamp(parse_i(parameter,0),0,1000);
        for(int index=0;index<9;index++)if(!strcmp(parameter,BUFFER_DIVISIONS[index]))setting=-index-1;
        for(int index=0;index<11;index++)if(!strcmp(parameter,HB_CP_RELEASE_ARP[index]))setting=-index-10;
        instance->player.release_ms=setting;return;
    }
    if(!strcmp(key,"track_defaults_reset")){hb_reset_track_defaults(instance,parse_i(parameter,-1));return;}
    if(!strcmp(parameter,"Secondary V"))parameter="Secondary V (Dom)";
    if(!strcmp(key,"harm_play_advance")){
        unsigned owner=instance->physical_target;
        if((!strcmp(parameter,"Next")||!strcmp(parameter,"1"))&&instance->role<2&&owner&&instance->physical_velocity[(owner-1)/128][(owner-1)%128]&&instance->motif.editor.recording<0&&(instance->approach_rows.performance||instance->motif.tap_active||instance->motif.playback_lane>=0)){
            if(!instance->advance_pending)instance->advance_owner=(unsigned short)owner;
            if(instance->advance_owner==owner&&instance->advance_pending<16)instance->advance_pending++;
        }
        return;
    }
    if(!strcmp(key,"dominant_color")){
        if(!strcmp(parameter,"Down")||!strcmp(parameter,"On")||!strcmp(parameter,"Hold")||!strcmp(parameter,"1"))instance->dominant_color_held=1;
        else if(!strcmp(parameter,"LatchOn")||!strcmp(parameter,"Latch")||!strcmp(parameter,"2"))instance->dominant_color_latched=1;
        else if(!strcmp(parameter,"LatchOff")||!strcmp(parameter,"Off")||!strcmp(parameter,"0"))instance->dominant_color_held=instance->dominant_color_latched=0;
        else instance->dominant_color_held=0;
        instance->opening_hash=0;instance->dirty=1;return;
    }
    if(!strcmp(key,"dominant_color_family")){
        instance->dominant_color_family=enum_index(parameter,DOMINANT_SCALE_OPTS,8,instance->dominant_color_family);
        instance->opening_hash=0;instance->dirty=1;return;
    }
    if(hb_sc_set(instance,key,parameter))return;
    if(!strcmp(key,"hb_pressure_full_velocity")){instance->pressure_full_velocity=parse_i(parameter,0)!=0;return;}
    if(!strcmp(key,"state")&&strstr(parameter,";freshrole1")){g_role_restored=0;g_pad_restored=0;}
    /* Transport telemetry is not a settings edit. Its base handlers own all
       required work; do not rescan every track's role/operation state for each
       per-chain heartbeat or clip position update. */
    if(!strcmp(key,"hb_movy_block")||!strcmp(key,"hb_movy_clip")||!strcmp(key,"hb_opening_preview")){
        set_param_base(value,key,parameter);return;
    }
    hb_role_sync(instance);
    if(!strcmp(key,"key_center")){
        instance->key_action=0;
        if(!strcmp(parameter,"Off")||!strcmp(parameter,"LatchOff")){for(int index=0;index<HB_MAX_INSTANCES;index++)g_pool[index].key_pending=0;g_key_armed=0;g_sc_arm_owner=-1;g_key_preview[0]=0;hb_sc_set_live(instance,HB_SC_KEY,0,0,0,0);}
        else if(!strncmp(parameter,"Down",4)||!strcmp(parameter,"On")||!strcmp(parameter,"LatchOn")){g_key_armed=1;g_sc_arm_owner=(int)(instance-g_pool);g_key_preview[0]=0;}
        return;
    }
    for(int field=0;field<4;field++)if(!strcmp(key,HB_TARGET_KEYS[field])){
        instance->target_scale_policy[field]=enum_index(parameter,HB_TARGET_OPTIONS[field],HB_TARGET_COUNTS[field],instance->target_scale_policy[field]);
        memset(instance->key_cache,0,sizeof(instance->key_cache));instance->opening_hash=0;instance->dirty=1;return;
    }
    if(!strcmp(key,"conductor_key_travel")||!strcmp(key,"follower_key_travel")||!strcmp(key,"follower_recorded_key_travel")||!strcmp(key,"follower_live_key_travel")){
        int *travel=!strcmp(key,"conductor_key_travel")?&g_key_conductor_travel:&g_key_follower_travel;
        int selected=travel!=&g_key_conductor_travel&&!strcmp(parameter,"Same as Conductor")?-1:enum_index(parameter,HB_KEY_TRAVEL,HB_KEY_TRAVEL_COUNT,*travel);
        if(selected!=*travel){*travel=selected;for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used){g_pool[index].play_revision++;g_pool[index].opening_hash=0;}}
        return;
    }
    if(!strcmp(key,"key_center_scale")){static const char *options[]={"Simplified Major/Minor","Mode from Parent","Use Parallel Scale"};g_key_scale_mode=enum_index(parameter,options,3,g_key_scale_mode);return;}
    if(!strcmp(key,"parallel_scale")){g_parallel_scale=enum_index(parameter,HB_PARALLEL_NAMES,HB_PARALLEL_COUNT,g_parallel_scale);if(g_parallel_scale<1)g_parallel_scale=1;if(g_sc_live[instance-g_pool][HB_SC_PARALLEL].on)hb_key_parallel(instance,1);return;}
    if(!strcmp(key,"parallel_mode")){
        if(!strcmp(parameter,"LatchOn")){g_sc_latch[instance-g_pool]=1;hb_key_parallel(instance,1);}
        else if(!strcmp(parameter,"Off")||!strcmp(parameter,"LatchOff")){g_sc_latch[instance-g_pool]=g_sc_manual[instance-g_pool]=0;hb_key_parallel(instance,0);}
        else if(!strncmp(parameter,"Down",4)||!strcmp(parameter,"On")){g_sc_manual[instance-g_pool]=1;hb_key_parallel(instance,1);}
        else {g_sc_manual[instance-g_pool]=0;if(!g_sc_latch[instance-g_pool]&&!g_sc_lane[instance-g_pool])hb_key_parallel(instance,0);}
        return;
    }
    instance->adopt_held=!strncmp(key,"motion_gesture_",15)||!strncmp(key,"motion_hold_",12);
    if(!hb_ml_set(instance,key,parameter)&&!hb_ar_set(instance,key,parameter)&&!hb_cs_set(instance,key,parameter)&&!hb_rr_set(instance,key,parameter)&&!hb_mt_set(instance,key,parameter)&&!hb_policy_set(instance,key,parameter))set_param_base(value,key,parameter);
    if(!strcmp(key,"state")){
        memset(instance->physical_velocity,0,sizeof(instance->physical_velocity));instance->physical_target=instance->target_attack_owner=0;instance->advance_pending=instance->advance_phase=0;
        if(!g_key_settings_restored){const char *suffix=strstr(parameter,";kc1,");int mode,travel,scale;
            if(suffix&&sscanf(suffix,";kc1,%d,%d,%d",&mode,&travel,&scale)==3&&mode>=0&&mode<=2&&travel>=0&&travel<HB_KEY_TRAVEL_COUNT&&scale>=1&&scale<HB_PARALLEL_COUNT){
                g_key_scale_mode=mode;g_key_conductor_travel=travel;g_parallel_scale=scale;
                const char *travel_suffix=strstr(parameter,";kt2,");int conductor,recorded,live;
                /* Legacy split settings migrate to the recorded policy so saved clips
                   retain their rendering. The new shared suffix takes precedence. */
                if(travel_suffix&&sscanf(travel_suffix,";kt2,%d,%d,%d",&conductor,&recorded,&live)==3&&conductor>=0&&conductor<HB_KEY_TRAVEL_COUNT&&recorded>=-1&&recorded<HB_KEY_TRAVEL_COUNT&&live>=-1&&live<HB_KEY_TRAVEL_COUNT){g_key_conductor_travel=conductor;g_key_follower_travel=recorded;}
                travel_suffix=strstr(parameter,";kt3,");int follower;
                if(travel_suffix&&sscanf(travel_suffix,";kt3,%d,%d",&conductor,&follower)==2&&conductor>=0&&conductor<HB_KEY_TRAVEL_COUNT&&follower>=-1&&follower<HB_KEY_TRAVEL_COUNT){g_key_conductor_travel=conductor;g_key_follower_travel=follower;}
                g_key_settings_restored=1;
            }}
        instance->dominant_color_held=instance->dominant_color_latched=0;instance->dominant_color_family=3;
        const char *color_suffix=strstr(parameter,";dc1,");int color_family=3;
        if(color_suffix&&sscanf(color_suffix,";dc1,%d",&color_family)==1&&color_family>=0&&color_family<8)instance->dominant_color_family=color_family;
        memset(instance->target_scale_policy,0,sizeof(instance->target_scale_policy));instance->target_scale_policy[2]=2;
        const char *target_suffix=strstr(parameter,";ts1,");int target_policy[4];
        if(target_suffix&&sscanf(target_suffix,";ts1,%d,%d,%d,%d",&target_policy[0],&target_policy[1],&target_policy[2],&target_policy[3])==4){
            int valid=1;for(int field=0;field<4;field++)if(target_policy[field]<0||target_policy[field]>=HB_TARGET_COUNTS[field])valid=0;
            if(valid)memcpy(instance->target_scale_policy,target_policy,sizeof(target_policy));
        }
        hb_role_restore(instance,parameter);hb_mt_restore(instance,parameter);hb_ml_restore(instance,parameter);hb_rr_restore(instance,parameter);hb_ar_restore(instance,parameter);hb_timeline_forget_track(instance);hb_timeline_restore(parameter);}
    for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used){g_pool[index].timeline_lane_cached=0;hb_role_sync(&g_pool[index]);}
    for(int index=0;index<HB_MAX_INSTANCES;index++)if(g_pool[index].used)hb_auto_chord_repeat_sync(&g_pool[index]);
    if(!strcmp(key,"role")||!strcmp(key,"track_role")){g_sc_dirty=1;hb_sc_resolve(instance);}
    if(!strcmp(key,"performance_reset")||!strcmp(key,"state"))hb_override_clear(instance);
    instance->adopt_held=0;
    hb_override_sync();
}
static midi_fx_api_v1_t API={MIDI_FX_API_VERSION,create_inst,destroy_inst,process,tick,set_param,get_param};
__attribute__((visibility("default")))
midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host){g_host=host;ensure_init();return &API;}
