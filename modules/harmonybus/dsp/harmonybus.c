/* Harmony Bus v0.2.73 — Schwung MIDI FX. */
#define HB_VERSION "0.2.73"
#ifdef HB_FREESTANDING
typedef __SIZE_TYPE__ size_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef struct _IO_FILE FILE;
extern int snprintf(char *, size_t, const char *, ...);
extern int sscanf(const char *, const char *, ...);
extern void *memset(void *, int, size_t);
extern void *memcpy(void *, const void *, size_t);
extern size_t strlen(const char *);
extern int strcmp(const char *, const char *);
extern int strncmp(const char *, const char *, size_t);
extern long strtol(const char *, char **, int);
extern double strtod(const char *, char **);
extern void *malloc(size_t);
extern void free(void *);
extern FILE *fopen(const char *, const char *);
extern int fclose(FILE *);
extern size_t fread(void *, size_t, size_t, FILE *);
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
#define HB_GLOBAL_VERSION 2u
#define HB_GLOBAL_SHM "/harmonybus-global-v1"
typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile uint32_t seq;
    volatile int follower_root_policy;
    volatile int follower_explicit_root;
    volatile int follower_content_map;
    volatile int follower_travel_map;
    volatile int follower_scale;
} hb_global_shared_t;
static hb_global_shared_t *g_global_shared=0;
/* These controls are truly global. A per-instance restore guard is not
   sufficient because Schwung can recreate instances while navigating UI. */
static int g_follower_globals_restored=0;
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
        shared->follower_content_map=0; /* In Chord */
        shared->follower_travel_map=0;  /* Relative */
        shared->follower_scale=0;       /* Infer */
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
static int hb_global_content_map(void){
    hb_global_open();
    if(!g_global_shared)return 0;
    int value=g_global_shared->follower_content_map;
    if(value<0||value>8){
        g_global_shared->seq++;
        g_global_shared->follower_content_map=0;
        g_global_shared->seq++;
        return 0;
    }
    return value;
}
static int hb_global_travel_map(void){
    hb_global_open();
    if(!g_global_shared)return 0;
    int value=g_global_shared->follower_travel_map;
    if(value<0||value>5){
        g_global_shared->seq++;
        g_global_shared->follower_travel_map=0;
        g_global_shared->seq++;
        return 0;
    }
    return value;
}
static int hb_global_scale(void){
    hb_global_open();
    if(!g_global_shared)return 0;
    int value=g_global_shared->follower_scale;
    if(value<0||value>9){
        g_global_shared->seq++;
        g_global_shared->follower_scale=0;
        g_global_shared->seq++;
        return 0;
    }
    return value;
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
static void hb_set_global_content_map(int value){
    hb_global_open();if(!g_global_shared)return;
    if(value<0||value>8)value=0;
    g_global_shared->seq++;g_global_shared->follower_content_map=value;g_global_shared->seq++;
}
static void hb_set_global_travel_map(int value){
    hb_global_open();if(!g_global_shared)return;
    if(value<0||value>5)value=0;
    g_global_shared->seq++;g_global_shared->follower_travel_map=value;g_global_shared->seq++;
}
static void hb_set_global_scale(int value){
    hb_global_open();if(!g_global_shared)return;
    if(value<0||value>9)value=0;
    g_global_shared->seq++;g_global_shared->follower_scale=value;g_global_shared->seq++;
}
typedef struct {
    int note;
    double start;
    double duration;
} hb_clip_note_t;
typedef struct { volatile unsigned seq; hb_harmony_t harmony; int global_transpose; int global_root_policy; int global_explicit_root; int global_input_root; int sensor_sources; int chord_timescale; int stability; int chord_timing; int quant_timing; int anticipation; int boundary_buffer_ms; int analysis_release_ms; int follower_content_map; int follower_travel_map; int follower_scale; int follower_split_map; int approach_control; int approach_mode; int inference_window_ms; int context; int accidentals; int auto_spell_sharps; int auto_spell_locked; int clip_track; int clip_slot; int clip_valid; int clip_note_count; int clip_stage; int clip_context; int last_clock_status; double clip_loop_start; double clip_loop_end; unsigned long clip_clock_ticks; unsigned clip_refresh_counter; double last_clip_playhead; int have_last_clip_playhead; unsigned cache_rev; unsigned sense_rev; int last_sense_count; uint8_t last_sense_notes[64]; unsigned global_process_count; unsigned global_note_event_count; unsigned global_accepted_note_count; unsigned global_tick_count; int global_last_status; int global_last_note; int global_last_channel; int global_last_instance; int follower_root_policy; int follower_explicit_root; hb_clip_note_t clip_notes[HB_MAX_CLIP_NOTES]; } SharedBus;
static SharedBus g_bus={0}; static int g_init=0;
typedef struct { int used,role,mode,content_map,travel_map,window_ms,dirty,frames_since_change; uint8_t active[128]; uint8_t held_now[128]; uint8_t held_count[128]; int pending_off_frames[128]; int mapped[128]; uint8_t follower_held[128]; uint8_t follower_sounding[128]; uint8_t follower_velocity[128]; unsigned follower_bus_seq; uint8_t source_seen[12]; int resolved_root,resolved_confidence; unsigned rx_count; unsigned note_on_count; unsigned note_off_count; int last_note; int last_status; int last_velocity; int active_count; int last_inferred_count; unsigned raw_event_count; unsigned raw_note_count; unsigned raw_note_on_count; unsigned raw_note_off_count; int raw_last_note; int raw_last_status; int raw_last_velocity; int raw_last_channel; int raw_last_cable; uint8_t raw_prev[HB_MIDI_OUT_BYTES]; int map_target; hb_harmony_t candidate_harmony; int candidate_frames; int committed_frames; int render_channel; int source_channel; int resolved_source_channel; unsigned live_press_count; int live_vouch_pending; int live_vouch_age; int recent_live_note[16]; int recent_live_age[16]; uint8_t recent_live_valid[16]; unsigned render_count; unsigned render_fail_count; int render_last_note; int retrigger_held; int follow_lookahead_ms; int approach_pad_armed; uint8_t approach_below_held; uint8_t approach_above_held; int follower_queue_count; uint8_t follower_queue_note[64]; uint8_t follower_queue_velocity[64]; uint8_t follower_queue_on[64]; uint8_t follower_queue_channel[64]; int follower_queue_age_frames[64]; double follower_queue_target_beat[64]; double follower_queue_arrival_beat[64]; double follower_note_delay_beats[128]; uint8_t follower_role_interval[128]; uint8_t published_conductor[128]; uint8_t published_follower[128]; int settle_frames_remaining; int clip_event_idle_frames; int last_transport_playing; uint8_t trace_note[8]; uint8_t trace_on[8]; uint8_t trace_channel[8]; unsigned trace_count; int local_sense_count; uint8_t local_sense_notes[64]; int global_timing_restored; } Inst;
static hb_harmony_t hb_mapping_target(hb_harmony_t harmony,int map_target);
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
static double hb_quant_grid_beats(void);
static double hb_anticipation_beats(void);
static double hb_ms_to_beats(int milliseconds);
static double hb_current_beat(void);
static void hb_publish_instance_notes(Inst *instance);
static int hb_read_active_set(char *uuid,int uuid_len,char *name,int name_len);
static int hb_find_conductor_track(const char *uuid);
static char *hb_read_text_file(const char *path,long *size_out);
static Inst g_pool[HB_MAX_INSTANCES];
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
    /* Fallback for stock/unpatched Chain hosts: Move's four track receive
       channels are track-index aligned (Track 1 -> Ch1 ... Track 4 -> Ch4).
       We can recover the conductor's owning track from the saved set state,
       which is much safer than accepting every cable-2 note broadcast. */
    if(instance->role==0){
        char uuid[128],name[128];
        if(hb_read_active_set(uuid,sizeof(uuid),name,sizeof(name))){
            int track=hb_find_conductor_track(uuid);
            if(track>=0&&track<4)return track;
        }
    }else if(instance->role==1){
        /* If the source-aware Chain tag has not arrived yet, recover a UNIQUE
           follower's owning track from saved slot state. This keeps Foll Trk
           populated from the realtime monitor after reload/navigation without
           accepting notes from unrelated tracks. */
        char uuid[128],name[128];
        if(hb_read_active_set(uuid,sizeof(uuid),name,sizeof(name))){
            int found=-1;
            char path[512];
            for(int track=0;track<4;track++){
                snprintf(path,sizeof(path),"/data/UserData/schwung/set_state/%s/slot_%d.json",uuid,track);
                long size=0;char *json=hb_read_text_file(path,&size);(void)size;
                if(!json)continue;
                int has_module=strstr(json,"harmonybus")!=0;
                int is_follower=(strstr(json,"hb15,1,")||strstr(json,"hb14,1,")||strstr(json,"hb13,1,")||strstr(json,"hb12,1,")||strstr(json,"hb11,1,")||strstr(json,"hb10,1,")||strstr(json,"hb9,1,")||strstr(json,"hb8,1,")||strstr(json,"hb7,1,")||strstr(json,"hb6,1,")||strstr(json,"hb5,1,")||strstr(json,"hb4,1,")||strstr(json,"hb3,1,")||strstr(json,"hb2,1,")||strstr(json,"hb1,1,"));
                free(json);
                if(has_module&&is_follower){
                    if(found>=0)return -1; /* ambiguous: wait for real track tag */
                    found=track;
                }
            }
            if(found>=0)return found;
        }
    }
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
    if(!instance||instance->role!=1)return 0;
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
    if(!instance||instance->role!=0)return 0;
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
static void ensure_init(void){if(g_init)return;memset(&g_bus,0,sizeof(g_bus));g_bus.global_root_policy=2;hb_global_open();g_bus.sensor_sources=0;g_bus.chord_timescale=0;g_bus.stability=0;g_bus.chord_timing=0;g_bus.quant_timing=0;g_bus.anticipation=0;g_bus.boundary_buffer_ms=20;g_bus.analysis_release_ms=60;g_bus.follower_content_map=0;g_bus.follower_travel_map=0;g_bus.follower_scale=0;g_bus.approach_control=1;g_bus.approach_mode=0;g_bus.inference_window_ms=25;g_bus.context=0;g_bus.accidentals=0;g_bus.auto_spell_sharps=1;g_bus.auto_spell_locked=0;g_bus.clip_track=-1;g_bus.clip_slot=0;g_bus.clip_stage=0;g_bus.clip_context=1;g_bus.last_clock_status=-1;g_bus.last_clip_playhead=0.0;g_bus.have_last_clip_playhead=0;g_bus.cache_rev=0;g_bus.sense_rev=0;g_bus.last_sense_count=0;g_bus.global_last_status=-1;g_bus.global_last_note=-1;g_bus.global_last_channel=-1;g_bus.global_last_instance=-1;g_bus.clip_loop_start=0.0;g_bus.clip_loop_end=4.0;for(int index=0;index<HB_MAX_INSTANCES;index++){memset(&g_pool[index],0,sizeof(g_pool[index]));g_pool[index].approach_pad_armed=1;for(int note=0;note<128;note++)g_pool[index].mapped[note]=-1;}g_init=1;}

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
static double hb_clip_playhead(void){
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

static void hb_render_follower_event(Inst *instance,int mapped_note,int velocity,int is_on,int is_off,int recv_channel){
    if(!instance||instance->role!=1||instance->render_channel<0||!g_host||!g_host->midi_inject_to_move)return;
    if(instance->render_channel==recv_channel)return; /* avoid self-echo loops */
    if(!(is_on||is_off)||mapped_note<0||mapped_note>127)return;
    /* The process path has already chosen and stored the pitch for this source
       note. Render must mirror that exact transformed event, not recompute it.
       In particular, note-off must use the same mapped pitch as note-on. */
    uint8_t packet[4];
    packet[0]=(uint8_t)(0x20 | (is_on?0x09:0x08)); /* cable 2 + CIN */
    packet[1]=(uint8_t)((is_on?0x90:0x80) | (instance->render_channel & 0x0F));
    packet[2]=(uint8_t)(mapped_note & 0x7F);
    packet[3]=(uint8_t)(is_on?velocity:0);
    int sent=g_host->midi_inject_to_move(packet,4);
    if(sent==4){instance->render_count++;instance->render_last_note=mapped_note;}
    else instance->render_fail_count++;
}

static int hb_inject_follower_note(Inst *instance,int mapped,int velocity,int is_on){
    if(!instance||instance->render_channel<0||!g_host||!g_host->midi_inject_to_move)return 0;
    uint8_t packet[4];
    packet[0]=(uint8_t)(0x20 | (is_on?0x09:0x08));
    packet[1]=(uint8_t)((is_on?0x90:0x80) | (instance->render_channel & 0x0F));
    packet[2]=(uint8_t)(mapped & 0x7F);
    packet[3]=(uint8_t)(is_on?velocity:0);
    int sent=g_host->midi_inject_to_move(packet,4);
    if(sent==4){instance->render_count++;instance->render_last_note=mapped;return 1;}
    instance->render_fail_count++;return 0;
}
static int hb_queue_follower_event(Inst *instance,int note,int velocity,int is_on,int channel){
    /* Chord Grid, Quant Grid, Anticipation and Buffer are follower-render
       timing only. Conductor events must never enter this subsystem. */
    if(!instance||instance->role!=1||instance->follower_queue_count>=64)return 0;
    int slot=instance->follower_queue_count++;
    double beat=hb_current_beat();
    instance->follower_queue_note[slot]=(uint8_t)(note&0x7F);
    instance->follower_queue_velocity[slot]=(uint8_t)(velocity&0x7F);
    instance->follower_queue_on[slot]=(uint8_t)(is_on?1:0);
    instance->follower_queue_channel[slot]=(uint8_t)(channel&0x0F);
    instance->follower_queue_target_beat[slot]=-1.0;
    instance->follower_queue_arrival_beat[slot]=beat;

    if(is_on){
        double capture=hb_ms_to_beats(g_bus.boundary_buffer_ms);
        double target=-1.0;

        /* Follower Buffer defines capture windows immediately BEFORE every
           enabled timing boundary. A note inside any window is delayed to the
           NEXT applicable boundary; notes outside all windows are untouched.

           Chord Grid boundaries are phase-shifted by Anticipation:
             grid=1 Bar, anticipation=1/8 => ..., 3.5, 7.5, 11.5, ...
           Quant Grid boundaries remain on their literal grid:
             quant=1/4 => ..., 3, 4, 5, ...

           If windows overlap, choose the EARLIEST upcoming boundary. Waiting
           for the later one adds an unintended extra rhythmic delay. */
        double chord_grid=hb_chord_grid_beats();
        if(chord_grid>0.0){
            double anticipation=hb_anticipation_beats();
            /* Solve for the first boundary > beat in the shifted sequence
               n*grid - anticipation. Avoid truncation-toward-zero phase bugs
               near beat 0 by advancing from a conservative integer bucket. */
            long cycle=(long)((beat+anticipation)/chord_grid);
            double boundary=(double)cycle*chord_grid-anticipation;
            while(boundary<=beat+1e-9)boundary+=chord_grid;
            double distance=boundary-beat;
            if(distance>=-1e-6&&distance<=capture+1e-6)
                target=boundary;
        }

        double quant_grid=hb_quant_grid_beats();
        if(quant_grid>0.0){
            long cycle=(long)(beat/quant_grid);
            double boundary=(double)cycle*quant_grid;
            while(boundary<=beat+1e-9)boundary+=quant_grid;
            double distance=boundary-beat;
            if(distance>=-1e-6&&distance<=capture+1e-6
               &&(target<0.0||boundary<target))
                target=boundary;
        }
        instance->follower_queue_target_beat[slot]=target;
    }else{
        /* Preserve articulation. A note-off inherits the exact delay applied
           to its matching note-on. If the ON is still queued, derive that delay
           from its arrival/target pair; otherwise use the delay remembered when
           the ON actually crossed the queue and became sounding. */
        double delay=instance->follower_note_delay_beats[note];
        for(int index=instance->follower_queue_count-2;index>=0;index--){
            if(instance->follower_queue_note[index]!=note||!instance->follower_queue_on[index])continue;
            if(instance->follower_queue_target_beat[index]>=0.0){
                double queued_delay=instance->follower_queue_target_beat[index]-instance->follower_queue_arrival_beat[index];
                if(queued_delay>delay)delay=queued_delay;
            }
            break;
        }
        if(delay>0.0)instance->follower_queue_target_beat[slot]=beat+delay;
    }

    /* -1 guarantees at least one full tick boundary before release. */
    instance->follower_queue_age_frames[slot]=-1;
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
static const char *hb_role_name_for_degree(int degree){
    static const char *roles[7]={"Root","2nd","3rd","4th","5th","6th","7th"};
    return (degree>=0&&degree<7)?roles[degree]:"--";
}
static const char *hb_follower_degree_role_for_note(Inst *instance,int note){
    if(!instance||note<0||note>127)return "--";
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))return "--";
    hb_harmony_t harmony=bus_read();
    int parent_scale_index=hb_parent_scale_index(instance,harmony);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);
    if(!parent_scale)return "--";
    int source_interval=mod12(note-source_root);
    int degree=hb_source_degree_from_parent_scale(source_interval,source_root,parent_scale);
    return hb_role_name_for_degree(degree);
}
static int hb_popcount12(uint16_t mask){
    int count=0;
    for(int bit=0;bit<12;bit++)if(mask&(1u<<bit))count++;
    return count;
}
static int hb_inferred_parent_scale_index(int source_root,hb_harmony_t harmony){
    /* Conservative parent-scale inference: choose the candidate tonic scale
       that contains the detected harmony most completely. Ties prefer Major,
       then Natural Minor. Explicit follower scale always overrides this. */
    uint16_t chord=hb_harmony_chord_mask(harmony);
    int best_scale=1;
    int best_score=-1;
    for(int scale_index=1;scale_index<=9;scale_index++){
        uint16_t candidate=hb_explicit_scale_mask(source_root,scale_index);
        int score=hb_popcount12((uint16_t)(chord&candidate));
        if(score>best_score){best_score=score;best_scale=scale_index;}
    }
    return best_scale;
}
static int hb_parent_scale_index(Inst *instance,hb_harmony_t harmony){
    int explicit_scale=hb_global_scale();
    if(explicit_scale>0)return explicit_scale;
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))source_root=harmony.root_pc;
    return hb_inferred_parent_scale_index(source_root,harmony);
}
static int hb_nth_scale_interval_from_root(uint16_t scale_mask,int target_root,int degree){
    /* degree is 0..6 meaning root, 2nd, 3rd, 4th, 5th, 6th, 7th.
       We rotate the parent scale onto the detected harmony root, yielding
       the chord-scale/mode for that diatonic chord root. */
    if(degree<=0)return 0;
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
    int source_interval=mod12(source_note-source_root);
    int degree=hb_source_degree_from_parent_scale(source_interval,source_root,
                                                   parent_scale);

    /* If the detected harmony root is in the parent scale, rotate that parent
       scale onto the harmony root. Example:
         F major + C root => C Mixolydian
         F natural minor + C root => C Phrygian
       Therefore follower G over root F (degree 2/9):
         over C in F major -> D
         over C in F minor -> Db. */
    uint16_t chord_scale=parent_scale;
    if(!(parent_scale&(1u<<mod12(harmony.root_pc)))){
        /* Chromatic detected root: fall back to a quality-derived local scale
           rather than Free/chromatic transport. */
        hb_harmony_t fallback=harmony;
        chord_scale=hb_scale_mask(fallback);
    }

    int target_interval=hb_nth_scale_interval_from_root(chord_scale,harmony.root_pc,degree);
    int target_pc=mod12(harmony.root_pc+target_interval);

    /* Keep octave/register close to a root-relative transposition of the source. */
    int root_delta=mod12(harmony.root_pc-source_root);
    if(root_delta>6)root_delta-=12;
    int nominal=source_note+root_delta;
    int adjustment=mod12(target_pc-mod12(nominal));
    if(adjustment>6)adjustment-=12;
    int mapped=nominal+adjustment;
    while(mapped<0)mapped+=12;
    while(mapped>127)mapped-=12;
    return mapped;
}
static hb_harmony_t hb_follower_content_target(Inst *instance,hb_harmony_t harmony,int content_map){
    /* Intermediate content modes are "In Chord + guaranteed scale degrees".
       Start with the normal inferred chord-tone set, then fill requested roles
       from the chord-scale implied by the follower parent scale.

       Internal content codes preserve legacy persistence:
         0 In Chord
         1 In Scale
         2 Free
         3 In 135
         4 In 1357
         5 In 12357
         6 In 12356
         7 In Non-Avoid
         8 In 123567
    */
    hb_harmony_t target=hb_mapping_target(harmony,0);
    if(content_map<3||content_map>8)return target;

    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))source_root=harmony.root_pc;
    int parent_scale_index=hb_parent_scale_index(instance,harmony);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);
    uint16_t chord_scale=parent_scale;
    if(!parent_scale||(parent_scale&(1u<<mod12(harmony.root_pc)))==0)
        chord_scale=hb_scale_mask(harmony);

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
        int interval=hb_nth_scale_interval_from_root(chord_scale,harmony.root_pc,degree);
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
static int hb_map_follower_note_relative(Inst *instance,int source_note,hb_harmony_t detected,
                                         hb_harmony_t content_target,int content_map){
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))return source_note;

    int nominal=source_note+hb_signed_root_delta(source_root,detected.root_pc);
    while(nominal<0)nominal+=12;
    while(nominal>127)nominal-=12;

    if(content_map==2)return nominal; /* Free + Relative is chromatic root-relative transport. */

    int parent_scale_index=hb_parent_scale_index(instance,detected);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);
    int source_degree=hb_source_degree_from_parent_scale(mod12(source_note-source_root),
                                                          source_root,parent_scale);
    uint16_t chord_scale=parent_scale;
    if(!parent_scale||(parent_scale&(1u<<mod12(detected.root_pc)))==0)
        chord_scale=hb_scale_mask(detected);

    int target_interval=hb_nth_scale_interval_from_root(chord_scale,detected.root_pc,source_degree);
    int target_pc=mod12(detected.root_pc+target_interval);

    if(content_target.pitch_mask&(1u<<target_pc))
        return hb_note_near_pc(nominal,target_pc);
    return hb_map_note(nominal,detected.root_pc,content_target,HB_MAP_NEAREST);
}
static uint16_t hb_scale_degree_mask(uint16_t chord_scale,int harmony_root,unsigned degree_bits){
    uint16_t mask=0;
    for(int degree=0;degree<7;degree++){
        if(!(degree_bits&(1u<<degree)))continue;
        int interval=hb_nth_scale_interval_from_root(chord_scale,harmony_root,degree);
        mask|=(uint16_t)(1u<<mod12(harmony_root+interval));
    }
    return mask;
}
static int hb_map_follower_note_closest_split(Inst *instance,int source_note,hb_harmony_t detected,
                                               hb_harmony_t content_target){
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))
        return hb_map_note(source_note,reference_root(instance),content_target,HB_MAP_NEAREST);

    int parent_scale_index=hb_parent_scale_index(instance,detected);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);
    int source_degree=hb_source_degree_from_parent_scale(mod12(source_note-source_root),
                                                          source_root,parent_scale);
    uint16_t chord_scale=parent_scale;
    if(!parent_scale||(parent_scale&(1u<<mod12(detected.root_pc)))==0)
        chord_scale=hb_scale_mask(detected);

    uint16_t inferred_chord_mask=hb_harmony_chord_mask(detected);
    uint16_t on_mask=0,off_mask=0;
    int source_is_on=0;
    int split=g_bus.follower_split_map;

    if(split==1){
        unsigned on_bits=(1u<<0)|(1u<<2)|(1u<<4);             /* 135 / 2467 */
        on_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,on_bits);
        off_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,0x7Fu&~on_bits);
        source_is_on=(on_bits&(1u<<source_degree))!=0;
    }else if(split==2){
        unsigned on_bits=(1u<<0)|(1u<<2)|(1u<<4)|(1u<<6);     /* 1357 / 246 */
        on_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,on_bits);
        off_mask=hb_scale_degree_mask(chord_scale,detected.root_pc,0x7Fu&~on_bits);
        source_is_on=(on_bits&(1u<<source_degree))!=0;
    }else if(split==3){
        /* Act. / Out: the ON side is the literal currently active conductor
           pitch classes, before diads are expanded or complex voicings are
           simplified by harmony inference. OUT is the remainder of the
           detected chord-scale. */
        uint8_t active_notes[64];
        int active_count=hb_observed_notes(0,active_notes,64);
        uint16_t active_mask=0;
        for(int index=0;index<active_count;index++)
            active_mask|=(uint16_t)(1u<<mod12(active_notes[index]));
        on_mask=active_mask;
        off_mask=(uint16_t)(chord_scale&(uint16_t)(~active_mask)&0x0FFFu);
        int degree_interval=hb_nth_scale_interval_from_root(chord_scale,detected.root_pc,source_degree);
        int degree_pc=mod12(detected.root_pc+degree_interval);
        source_is_on=(active_mask&(1u<<degree_pc))!=0;
    }else{
        /* Harm. / Out: use the inferred harmony mask. This intentionally uses
           the normalized harmony, so a diad may be expanded to its inferred
           triad and a complex voicing may be simplified before the split. */
        on_mask=inferred_chord_mask;
        off_mask=(uint16_t)(chord_scale&(uint16_t)(~inferred_chord_mask)&0x0FFFu);
        int degree_interval=hb_nth_scale_interval_from_root(chord_scale,detected.root_pc,source_degree);
        int degree_pc=mod12(detected.root_pc+degree_interval);
        source_is_on=(inferred_chord_mask&(1u<<degree_pc))!=0;
    }

    uint16_t legal=(uint16_t)(content_target.pitch_mask&0x0FFFu);
    uint16_t preferred=(uint16_t)(legal&(source_is_on?on_mask:off_mask));
    hb_harmony_t target=content_target;
    target.pitch_mask=preferred?preferred:legal;
    if(!target.pitch_mask)return source_note;
    return hb_map_note(source_note,reference_root(instance),target,HB_MAP_NEAREST);
}
static int hb_map_follower_note_now(Inst *instance,int source_note){
    hb_harmony_t detected=bus_read();
    int content_map=hb_global_content_map();
    if(!detected.valid)return source_note;

    hb_harmony_t target=detected;
    if(content_map==2){
        target.pitch_mask=0x0FFFu;
    }else if(content_map==1){
        target=hb_follower_scale_target(instance,detected);
    }else{
        target=hb_follower_content_target(instance,detected,content_map);
    }

    int travel=hb_global_travel_map();
    if(travel==5)return source_note;
    if(travel==0)
        return hb_map_follower_note_relative(instance,source_note,detected,target,content_map);
    if(travel==3)
        return hb_map_follower_note_closest_split(instance,source_note,detected,target);
    if(travel==1)
        return hb_map_note(source_note,reference_root(instance),target,HB_MAP_NEAREST);
    if(travel==2)
        return hb_map_note_upward(source_note,target);
    if(travel==4)
        return hb_map_note_downward(source_note,target);
    return source_note;
}
static int hb_active_approach(const Inst *instance){
    if(!instance)return 1;
    if(g_bus.approach_mode==2)return 1;
    if(g_bus.approach_mode==2)return 1;
    if(g_bus.approach_mode==2)return 1;
    if(g_bus.approach_mode==0){
        if(instance->approach_pad_armed!=1)return instance->approach_pad_armed;
        return g_bus.approach_control;
    }
    if(instance->approach_below_held)return 0;
    if(instance->approach_above_held)return 2;
    return g_bus.approach_control;
}
static int hb_apply_approach(Inst *instance,int mapped,int approach){
    if(approach==0){
        return mapped>0?mapped-1:0;
    }
    if(approach==2){
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
    return mapped;
}
static void hb_consume_next_approach(Inst *instance){
    if(!instance||g_bus.approach_mode!=0)return;
    instance->approach_pad_armed=1;
    /* The knob is another way to arm exactly the same one-shot state. */
    g_bus.approach_control=1;
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
static int hb_release_follower_queue(Inst *instance,int frames,int sample_rate,
                                     uint8_t output[][3],int lengths[],int max_output){
    if(!instance||instance->role!=1||max_output<=0)return 0;
    int target_frames=0; /* Follow Lookahead removed; Follower Buffer owns intentional pre-boundary delay. */
    int emitted=0;
    int next_approach=(g_bus.approach_mode==0)?hb_active_approach(instance):1;
    int next_approach_used=0;
    double next_arrival=-1.0;
    for(int index=0;index<instance->follower_queue_count;index++){
        int age=instance->follower_queue_age_frames[index];
        if(age<0){
            instance->follower_queue_age_frames[index]=0;
            continue;
        }
        age+=frames;
        instance->follower_queue_age_frames[index]=age;
        if(age<target_frames||emitted>=max_output){
            /* Capacity pressure must never consume/drop a queued MIDI event.
               Leave it intact for the next tick. */
            continue;
        }
        double target_beat=instance->follower_queue_target_beat[index];
        if(target_beat>=0.0&&hb_current_beat()+1e-6<target_beat){
            continue;
        }
        /* Do not map against stale harmony merely because this follower's
           tick happened before the conductor's tick in the same scheduler
           cycle.  This is the actual conductor-first barrier. */
        if(hb_conductor_pending_for_follow()){
            continue;
        }
        int source_note=instance->follower_queue_note[index];
        int velocity=instance->follower_queue_velocity[index];
        int is_on=instance->follower_queue_on[index]!=0;
        int mapped;
        if(is_on){
            mapped=hb_map_follower_note_now(instance,source_note);
            int approach=(g_bus.approach_mode==0)?next_approach:hb_active_approach(instance);
            if(approach!=1){
                int apply=1;
                if(g_bus.approach_mode==0){
                    double arrival=instance->follower_queue_arrival_beat[index];
                    if(next_arrival<0.0)next_arrival=arrival;
                    double difference=arrival-next_arrival;
                    if(difference<0.0)difference=-difference;
                    /* One physical chord/gesture may serialize into several
                       MIDI note-ons. Consume once, but decorate the whole
                       effectively-simultaneous group. */
                    if(difference>0.002)apply=0;
                }
                if(apply){
                    mapped=hb_apply_approach(instance,mapped,approach);
                    if(g_bus.approach_mode==0)next_approach_used=1;
                }
            }
            instance->mapped[source_note]=mapped;
            instance->follower_sounding[source_note]=1;
            double delay=instance->follower_queue_target_beat[index]>=0.0
                ?instance->follower_queue_target_beat[index]-instance->follower_queue_arrival_beat[index]:0.0;
            instance->follower_note_delay_beats[source_note]=delay>0.0?delay:0.0;
            instance->source_seen[source_note%12]=1;
        }else{
            mapped=instance->mapped[source_note];
            if(mapped<0)mapped=source_note;
            instance->follower_sounding[source_note]=0;
            instance->follower_note_delay_beats[source_note]=0.0;
            instance->mapped[source_note]=-1;
        }
        output[emitted][0]=(uint8_t)((is_on?0x90:0x80)|instance->follower_queue_channel[index]);
        output[emitted][1]=(uint8_t)(mapped&0x7F);
        output[emitted][2]=(uint8_t)(is_on?velocity:0);
        lengths[emitted]=3;
        emitted++;
        if(instance->render_channel>=0)
            hb_render_follower_event(instance,mapped,velocity,is_on,!is_on,instance->follower_queue_channel[index]);
        /* Mark released; compaction below drops it. */
        instance->follower_queue_age_frames[index]=-2147483647;
    }
    int write=0;
    for(int index=0;index<instance->follower_queue_count;index++){
        if(instance->follower_queue_age_frames[index]==-2147483647)continue;
        if(write!=index){
            instance->follower_queue_note[write]=instance->follower_queue_note[index];
            instance->follower_queue_velocity[write]=instance->follower_queue_velocity[index];
            instance->follower_queue_on[write]=instance->follower_queue_on[index];
            instance->follower_queue_channel[write]=instance->follower_queue_channel[index];
            instance->follower_queue_age_frames[write]=instance->follower_queue_age_frames[index];
            instance->follower_queue_target_beat[write]=instance->follower_queue_target_beat[index];
            instance->follower_queue_arrival_beat[write]=instance->follower_queue_arrival_beat[index];
        }
        write++;
    }
    instance->follower_queue_count=write;
    if(next_approach_used)hb_consume_next_approach(instance);
    return emitted;
}
static int hb_reharmonize_held_follower(Inst *instance,uint8_t output[][3],int lengths[],int max_output){
    if(!instance||instance->role!=1||!output||!lengths||max_output<=0)return 0;
    unsigned bus_seq=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);
    if(bus_seq==instance->follower_bus_seq)return 0;
    instance->follower_bus_seq=bus_seq;
    /* Off means event-preserving follower behavior: harmony changes affect the
       next source note-on, but never synthesize extra note-offs/note-ons for a
       note that is already held. This keeps follower rhythm exactly aligned
       with its source clip/performance. */
    if(!instance->retrigger_held)return 0;
    hb_harmony_t harmony=bus_read();
    if(!harmony.valid)return 0;

    uint8_t source_notes[128];
    int previous_outputs[128];
    int new_outputs[128];
    int voice_count=0;
    for(int source_note=0;source_note<128;source_note++){
        if(!instance->follower_sounding[source_note])continue;
        source_notes[voice_count]=(uint8_t)source_note;
        previous_outputs[voice_count]=instance->mapped[source_note];

        /* Retriggered held notes MUST use the exact same mapper as a fresh
           follower note-on. The old held-voice path used legacy mode/root
           mapping, which could choose a different octave (and even different
           Content/Travel semantics) from hb_map_follower_note_now(). */
        new_outputs[voice_count]=hb_map_follower_note_now(instance,source_note);

        int used_root=0;
        if(hb_resolve_follower_reference_root(instance,&used_root))
            instance->follower_role_interval[source_note]=(uint8_t)mod12(source_note-used_root);
        else
            instance->follower_role_interval[source_note]=255;
        voice_count++;
    }

    int emitted=0;
    /* Emit OFFs first, then ONs, through the normal MIDI-FX tick output.
       In Schw+Move mode the chain host injects these into THIS slot's native
       Move instrument. Render To Ch remains an optional secondary copy. */
    for(int voice=0;voice<voice_count&&emitted<max_output;voice++){
        if(previous_outputs[voice]<0||previous_outputs[voice]==new_outputs[voice])continue;
        output[emitted][0]=0x80;
        output[emitted][1]=(uint8_t)previous_outputs[voice];
        output[emitted][2]=0;
        lengths[emitted]=3;
        emitted++;
        if(instance->render_channel>=0)
            hb_inject_follower_note(instance,previous_outputs[voice],0,0);
    }
    for(int voice=0;voice<voice_count&&emitted<max_output;voice++){
        int source_note=source_notes[voice];
        if(previous_outputs[voice]==new_outputs[voice])continue;
        output[emitted][0]=0x90;
        output[emitted][1]=(uint8_t)new_outputs[voice];
        output[emitted][2]=instance->follower_velocity[source_note];
        lengths[emitted]=3;
        emitted++;
        if(instance->render_channel>=0)
            hb_inject_follower_note(instance,new_outputs[voice],instance->follower_velocity[source_note],1);
        instance->mapped[source_note]=new_outputs[voice];
    }
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
    static const int intervals[9][7]={
        {0,2,4,5,7,9,11},   /* Major / Ionian */
        {0,2,3,5,7,8,10},   /* Natural Minor / Aeolian */
        {0,2,3,5,7,9,10},   /* Dorian */
        {0,1,3,5,7,8,10},   /* Phrygian */
        {0,2,4,6,7,9,11},   /* Lydian */
        {0,2,4,5,7,9,10},   /* Mixolydian */
        {0,1,3,5,6,8,10},   /* Locrian */
        {0,2,3,5,7,8,11},   /* Harmonic Minor */
        {0,2,3,5,7,9,11}    /* Melodic Minor */
    };
    if(scale_index<1||scale_index>9)return 0;
    uint16_t mask=0;
    for(int i=0;i<7;i++)mask|=(uint16_t)(1u<<mod12(root_pc+intervals[scale_index-1][i]));
    return mask;
}
static hb_harmony_t hb_follower_scale_target(Inst *instance,hb_harmony_t harmony){
    if(!harmony.valid)return harmony;

    /* Follower Content = In Scale defines a LEGAL OUTPUT SET around the
       DETECTED harmony. The follower root/scale describes source-role
       semantics and the parent pitch collection; it must not re-root the
       output target back onto the follower root.

       Example: follower root F, parent F minor, detected Ab minor.
       The legal output set is the F-minor collection viewed from Ab
       (Ab Ionian-like rotation of that collection), while harmony.root_pc
       remains Ab. Closest therefore chooses nearest pitches from that legal
       set instead of collapsing toward an F-rooted scale target. */
    int source_root=0;
    if(!hb_resolve_follower_reference_root(instance,&source_root))
        source_root=harmony.root_pc;

    int parent_scale_index=hb_parent_scale_index(instance,harmony);
    uint16_t parent_scale=hb_explicit_scale_mask(source_root,parent_scale_index);

    if(parent_scale&&(parent_scale&(1u<<mod12(harmony.root_pc)))){
        /* Keep detected root/bass/quality; only replace the legal pitch set. */
        harmony.pitch_mask=parent_scale;
    }else{
        /* If the detected root is chromatic to the parent collection, use a
           quality-derived local chord-scale rather than re-rooting to source. */
        harmony.pitch_mask=hb_scale_mask(harmony);
    }
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
    static const char *suffix[]={"","m","5","s2","s4","dim","aug","6","m6","M7","7","m7","mM7","m7b5","dim7","a9","ma9","M9","9","m9","11","m11","13"};
    return (chord_index>=0&&chord_index<23)?suffix[chord_index]:"";
}
static int hb_format_harmony(char *buffer,int length,hb_harmony_t harmony){
    if(!harmony.valid)return snprintf(buffer,(size_t)length,"--");
    if(harmony.chord_index<0)return snprintf(buffer,(size_t)length,"%s?",hb_pc_display(harmony.root_pc,harmony));
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
    static const char *roles[12]={"Root","b2","2/9","m3","M3","4/11","b5","5","#5/b6","6/13","b7","M7"};
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
static int infer_reference_root(Inst *instance){static const int major[7]={0,2,4,5,7,9,11};static const int minor[7]={0,2,3,5,7,8,10};int pitch_classes=0,best=-999,best_root=instance->resolved_root;for(int index=0;index<12;index++)pitch_classes+=instance->source_seen[index]?1:0;if(!pitch_classes)return instance->resolved_root;for(int root=0;root<12;root++)for(int scale=0;scale<2;scale++){int score=0;for(int pitch_class=0;pitch_class<12;pitch_class++)if(instance->source_seen[pitch_class]){int relative=mod12(pitch_class-root),inside=0;for(int degree=0;degree<7;degree++)if(relative==(scale?minor[degree]:major[degree])){inside=1;break;}score+=inside?5:-4;}if(instance->source_seen[root])score+=3;if(score>best){best=score;best_root=root;}}instance->resolved_confidence=pitch_classes>=4?80:(pitch_classes>=3?65:45);return best_root;}
static int reference_root(Inst *instance){if(g_bus.global_root_policy==0)return mod12(g_bus.global_explicit_root);if(g_bus.global_root_policy==1)return mod12(g_bus.global_input_root);instance->resolved_root=infer_reference_root(instance);return mod12(instance->resolved_root);}
static void *create_inst(const char *module_dir,const char *config_json){(void)module_dir;(void)config_json;ensure_init();for(int index=0;index<HB_MAX_INSTANCES;index++)if(!g_pool[index].used){Inst *instance=&g_pool[index];memset(instance,0,sizeof(*instance));instance->used=1;instance->role=2;instance->mode=0;instance->content_map=0;instance->travel_map=0;instance->map_target=0;instance->window_ms=25;instance->last_note=-1;instance->last_status=-1;instance->last_velocity=-1;instance->raw_last_note=-1;instance->raw_last_status=-1;instance->raw_last_velocity=-1;instance->raw_last_channel=-1;instance->raw_last_cable=-1;instance->render_channel=-1;instance->source_channel=-1;instance->resolved_source_channel=-1;instance->render_last_note=-1;instance->retrigger_held=0;instance->follow_lookahead_ms=0;instance->follower_queue_count=0;for(int note=0;note<128;note++){instance->mapped[note]=-1;instance->follower_role_interval[note]=255;}return instance;}return 0;}
static void destroy_inst(void *value){Inst *instance=(Inst*)value;if(instance)instance->used=0;}
static int hb_source_channel_matches(Inst *instance,int midi_channel){
    if(!instance)return 0;
    if(instance->source_channel>=0)return midi_channel==instance->source_channel;
    /* Prefer the source-aware Chain tag; on stock/unpatched hosts the
       conductor fallback in hb_monitor_channel() resolves the owning Move
       track from set state. Never accept-all for a conductor, because cable-2
       is broadcast and that is exactly how follower-track notes leaked into
       the conductor local/global panels. */
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
static void hb_clear_instance_note_state(Inst *instance){
    if(!instance)return;
    memset(instance->held_now,0,sizeof(instance->held_now));memset(instance->held_count,0,sizeof(instance->held_count));
    memset(instance->active,0,sizeof(instance->active));
    memset(instance->pending_off_frames,0,sizeof(instance->pending_off_frames));
    memset(instance->follower_held,0,sizeof(instance->follower_held));
    memset(instance->follower_sounding,0,sizeof(instance->follower_sounding));
    memset(instance->follower_note_delay_beats,0,sizeof(instance->follower_note_delay_beats));
    memset(instance->follower_velocity,0,sizeof(instance->follower_velocity));instance->follower_queue_count=0;memset(instance->published_conductor,0,sizeof(instance->published_conductor));memset(instance->published_follower,0,sizeof(instance->published_follower));instance->settle_frames_remaining=0;instance->clip_event_idle_frames=0;
    for(int note=0;note<128;note++){
        instance->mapped[note]=-1;
        instance->follower_role_interval[note]=255;
    }
    instance->active_count=0;
    instance->last_inferred_count=0;
    instance->candidate_frames=0;
    instance->dirty=1;
    instance->frames_since_change=0;
}
static void hb_publish_instance_notes(Inst *instance){
    if(!instance)return;
    if(instance->role==0){
        for(int note=0;note<128;note++)instance->published_conductor[note]=instance->held_count[note]>0?1:0;
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
static int process(void *value,const uint8_t *input,int length,uint8_t output[][3],int lengths[],int max_output){Inst *instance=(Inst*)value;if(!instance||!input||length<1)return 0;g_bus.global_process_count++;g_bus.global_last_status=input[0];g_bus.global_last_instance=hb_instance_index(instance);
if(input[0]==0xFC){
    hb_clear_instance_note_state(instance);
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
    if(hb_source_channel_matches(instance,control_channel))hb_clear_instance_note_state(instance);
    return pass(input,length,output,lengths,max_output);
}
if(!(is_on||is_off))return pass(input,length,output,lengths,max_output);int note=input[1]&0x7F,mapped;int input_channel=input[0]&0x0F;if(instance->role==2)return pass(input,length,output,lengths,max_output);if(!hb_source_channel_matches(instance,input_channel))return pass(input,length,output,lengths,max_output);g_bus.global_accepted_note_count++;instance->last_status=input[0];instance->last_note=note;instance->last_velocity=length>=3?input[2]:0;hb_trace_note_event(instance,note,is_on,input_channel);if(is_on){instance->note_on_count++;instance->active_count++;}else if(is_off){instance->note_off_count++;if(instance->active_count>0)instance->active_count--;}if(instance->role==0){
    if(is_on){
        if(instance->held_count[note]<255)instance->held_count[note]++;
        instance->held_now[note]=instance->held_count[note]>0;
        instance->pending_off_frames[note]=0;
        mapped=note+g_bus.global_transpose;if(mapped<0)mapped=0;if(mapped>127)mapped=127;
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
        if(mapped<0)mapped=note+g_bus.global_transpose;
        if(mapped<0)mapped=0;if(mapped>127)mapped=127;
        if(instance->held_count[note]==0)instance->mapped[note]=-1;
    }
    hb_publish_instance_notes(instance);
    if(max_output<1)return 0;
    output[0][0]=input[0];output[0][1]=(uint8_t)mapped;output[0][2]=length>=3?input[2]:0;lengths[0]=3;
    return 1;
}
if(is_on){
    instance->follower_held[note]=1;
    instance->follower_velocity[note]=(uint8_t)(length>=3?input[2]:100);
    int root=0;
    instance->follower_role_interval[note]=hb_resolve_follower_reference_root(instance,&root)
        ?(uint8_t)mod12(note-root):255;
}else{
    instance->follower_held[note]=0;
    instance->follower_velocity[note]=0;
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
static int tick(void *value,int frames,int sample_rate,uint8_t output[][3],int lengths[],int max_output){
    Inst *instance=(Inst*)value;
    if(!instance)return 0;
    g_bus.global_tick_count++;
    if(instance->role==1)hb_sync_from_monitor(instance);
    else if(instance->role==0)hb_sync_conductor_from_monitor(instance);
    if(g_host&&g_host->get_clock_status){
        int clock_status=g_host->get_clock_status();
        int is_playing=(clock_status==1);
        if(instance->last_transport_playing&&!is_playing){
            /* Pause/stop is a safe one-way boundary: bias toward extra note-offs
               by clearing all held state rather than allowing stuck notes. */
            hb_clear_instance_note_state(instance);
        }
        instance->last_transport_playing=is_playing;
    }
    if(instance->role==1){
        int emitted=hb_release_follower_queue(instance,frames,sample_rate,output,lengths,max_output);
        if(emitted>0)return emitted;
        /* The follower queue is the serialization boundary between physical
           input state and sounding output state. Never synthesize retriggers
           while an ON/OFF is still buffered: doing so can create an OFF for a
           note that has not sounded yet, duplicate an ON, or orphan a later
           note-off. */
        if(instance->follower_queue_count>0)return 0;
        return hb_reharmonize_held_follower(instance,output,lengths,max_output);
    }
    if(instance->role!=0)return 0;

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

    if(g_bus.clip_context&&g_host&&g_host->get_clock_status){
        int clock_status=g_host->get_clock_status();
        if(clock_status==1){
            double playhead=hb_clip_playhead();
            g_bus.last_clip_playhead=playhead;
            g_bus.have_last_clip_playhead=1;
        }else{
            g_bus.have_last_clip_playhead=0;
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
            bus_write(instance->candidate_harmony);
            instance->committed_frames=0;
            instance->candidate_frames=0;
        }
        return 0;
    }

    if(!instance->dirty)return 0;

    instance->frames_since_change+=frames;
    int needed=(g_bus.inference_window_ms*sample_rate)/1000;
    uint8_t notes[128];
    int count=local_conductor_notes(instance,notes,128);
    /* A complete quantized/live chord should not pay the generic grouping
       window. With Free timing + Live context, three or more simultaneous
       conductor notes are authoritative in this block; the follower queue's
       one-tick barrier then guarantees they win over same-time follower notes. */
    int complete_live_free=(g_bus.context==0&&count>=3);
    if(!complete_live_free&&instance->frames_since_change<needed)return 0;

    instance->last_inferred_count=count;
    instance->dirty=0;

    if(count<=0){
        memset(&instance->candidate_harmony,0,sizeof(instance->candidate_harmony));
        instance->candidate_frames=0;
        return 0;
    }

    hb_harmony_t committed=bus_read();
    hb_harmony_t committed_sensor=hb_transpose_harmony(committed,-g_bus.global_transpose);
    hb_harmony_t candidate=hb_infer_harmony_contextual(notes,count,committed_sensor);
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
        bus_write(candidate);
        instance->committed_frames=0;
        instance->candidate_frames=0;
        return 0;
    }

    if(hb_same_harmony(candidate,committed)){
        instance->candidate_frames=0;
        return 0;
    }

    /* Be deliberately insensitive to nested subset/superset voicing changes,
       but promote a complete structurally incompatible harmony immediately.
       Example: Fmaj -> F5 holds Fmaj; Fmaj -> Fmin/Fdim changes now. */
    if(count>=3&&hb_character_change(candidate,committed)){
        bus_write(candidate);
        instance->committed_frames=0;
        instance->candidate_frames=0;
        return 0;
    }

    /* Free timing means a complete current voicing is authoritative now.
       Quantized/anticipated timing retains confirmation/dwell behavior. */
    if(g_bus.context==0 && count>=3){
        bus_write(candidate);
        instance->committed_frames=0;
        instance->candidate_frames=0;
        return 0;
    }

    if(candidate.valid){
        int min_dwell=hb_timescale_frames(sample_rate);
        int overwhelming=(candidate.confidence>=90&&count>=3);
        if(overwhelming && instance->committed_frames>=min_dwell/2){
            bus_write(candidate);
            instance->committed_frames=0;
            instance->candidate_frames=0;
        }else{
            instance->candidate_frames=frames;
        }
    }
    return 0;
}

static void hb_restore_state(Inst *instance,const char *state);
static int enum_index(const char *value,const char *const *options,int count,int fallback){
    /* Match enum labels BEFORE attempting numeric index parsing.
       Labels such as "1/16", "1/8", "1/4", "1/2", "1 Bar", and "2 Bars"
       all begin with a digit; strtol() previously consumed that leading digit
       and misread them as enum indices 1 or 2. "Free" worked only because it
       is non-numeric. */
    if(value){
        for(int index=0;index<count;index++)if(!strcmp(value,options[index]))return index;
        char *end=0;
        long parsed=strtol(value,&end,10);
        if(end&&end!=value&&*end=='\0'&&parsed>=0&&parsed<count)return (int)parsed;
    }
    return fallback;
}
static const char *ROLE_OPTS[]={"Conductor","Follower","Off"};static const char *RETRIGGER_OPTS[]={"Off","On"};static const char *APPROACH_OPTS[]={"Chrom Below","Off","Scale Above"};static const char *APPROACH_MODE_OPTS[]={"Next","Held","Off"};static const char *QUANT_GRID_OPTS[]={"Off","1/16","1/8","1/4","1/2","1 Bar","2 Bars"};static const char *FOLLOWER_SOURCE_POLICY_OPTS[]={"Infer Input","Infer Notes","Explicit"};static const char *CLIP_SLOT_OPTS[]={"Auto","1","2","3","4","5","6","7","8"};static const char *SENSOR_SOURCE_OPTS[]={"Realtime","Realtime + Clip","Clip"};static const char *CLIP_CONTEXT_OPTS[]={"Clip + Realtime","Realtime Only"};static const char *SOURCE_CH_OPTS[]={"Auto","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"};static const char *RENDER_CH_OPTS[]={"Off","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"};static const char *MODE_OPTS[]={"Relative","Smooth","Nearest"};static const char *POLICY_OPTS[]={"Explicit","Current Input Root","Auto-Infer"};static const char *MAP_TARGET_OPTS[]={"Chord","Scale"};static const char *TIMING_OPTS[]={"Free","1/16","1/8","1/4","1/2","1 Bar","2 Bars"};static const char *FOLLOWER_SCALE_OPTS[]={"Infer","Major","Natural Minor","Dorian","Phrygian","Lydian","Mixolydian","Locrian","Harmonic Minor","Melodic Minor"};static const char *ANTICIPATION_OPTS[]={"On Grid","1/64 Early","1/32 Early","1/16 Early","1/8 Early","1/4 Early"};static const char *CONTEXT_OPTS[]={"Live","1/32","1/16","1/8","1/4","1/2","1 Bar"};static const char *TIMESCALE_OPTS[]={"Free","1/16","1/8","1/4","1/2","1 Bar"};static const char *STABILITY_OPTS[]={"Responsive","Balanced","Stable"};static const char *ACCIDENTAL_OPTS[]={"Auto","Sharps","C#D#F#G#Bb","C#EbF#G#Bb","C#EbF#AbBb","DbEbF#AbBb","Flats"};static const char *PC_OPTS[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
static int hb_timing_to_legacy_timescale(int timing){
    /* Chord Grid is a musical-boundary hint, not a mandatory dwell time. */
    (void)timing;
    return 0;
}
static double hb_chord_grid_beats(void){
    static const double beats[7]={0.0,0.25,0.5,1.0,2.0,4.0,8.0};
    int index=g_bus.chord_timing;
    return (index>=0&&index<7)?beats[index]:0.0;
}
static double hb_quant_grid_beats(void){
    static const double beats[7]={0.0,0.25,0.5,1.0,2.0,4.0,8.0};
    int index=g_bus.quant_timing;
    return (index>=0&&index<7)?beats[index]:0.0;
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
static const char CHAIN_PARAMS[]="["
"{\\\"key\\\":\\\"role\\\",\\\"name\\\":\\\"Role\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Conductor\\\",\\\"Follower\\\",\\\"Off\\\"],\\\"options_as_string\\\":true},"
"{\\\"key\\\":\\\"clip_context\\\",\\\"name\\\":\\\"Clip Context\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Clip + Realtime\\\",\\\"Realtime Only\\\"],\\\"options_as_string\\\":true},"
"{\\\"key\\\":\\\"mode\\\",\\\"name\\\":\\\"Follow Mode\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Transpose\\\",\\\"Chord\\\",\\\"Nearest\\\"],\\\"options_as_string\\\":true},"
"{\\\"key\\\":\\\"root_policy\\\",\\\"name\\\":\\\"Root Policy\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Explicit\\\",\\\"Current Input Root\\\",\\\"Auto-Infer\\\"],\\\"options_as_string\\\":true},"
"{\\\"key\\\":\\\"explicit_root\\\",\\\"name\\\":\\\"Explicit Root\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"C\\\",\\\"C#\\\",\\\"D\\\",\\\"Eb\\\",\\\"E\\\",\\\"F\\\",\\\"F#\\\",\\\"G\\\",\\\"Ab\\\",\\\"A\\\",\\\"Bb\\\",\\\"B\\\"],\\\"options_as_string\\\":true},"
"{\\\"key\\\":\\\"content_map\\\",\\\"name\\\":\\\"Follower Content\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"In Chord\\\",\\\"In Scale\\\",\\\"Free\\\"],\\\"options_as_string\\\":true,\\\"default\\\":\\\"In Chord\\\"},"
"{\\\"key\\\":\\\"travel_map\\\",\\\"name\\\":\\\"Follower Travel\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Relative\\\",\\\"Closest\\\",\\\"Up\\\"],\\\"options_as_string\\\":true,\\\"default\\\":\\\"Relative\\\"},"
"{\\\"key\\\":\\\"follower_scale\\\",\\\"name\\\":\\\"Follower Scale\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Infer\\\",\\\"Major\\\",\\\"Natural Minor\\\",\\\"Dorian\\\",\\\"Phrygian\\\",\\\"Lydian\\\",\\\"Mixolydian\\\",\\\"Locrian\\\",\\\"Harmonic Minor\\\",\\\"Melodic Minor\\\"],\\\"options_as_string\\\":true,\\\"default\\\":\\\"Infer\\\"},"
"{\\\"key\\\":\\\"quant_timing\\\",\\\"name\\\":\\\"Quant Grid\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Off\\\",\\\"1/16\\\",\\\"1/8\\\",\\\"1/4\\\",\\\"1/2\\\",\\\"1 Bar\\\",\\\"2 Bars\\\"],\\\"options_as_string\\\":true,\\\"default\\\":\\\"Off\\\"},"
"{\\\"key\\\":\\\"transpose\\\",\\\"name\\\":\\\"Transpose\\\",\\\"type\\\":\\\"int\\\",\\\"min\\\":-24,\\\"max\\\":24,\\\"step\\\":1},"
"{\\\"key\\\":\\\"window_ms\\\",\\\"name\\\":\\\"Inference Window\\\",\\\"type\\\":\\\"int\\\",\\\"min\\\":10,\\\"max\\\":500,\\\"step\\\":5},"
"{\\\"key\\\":\\\"harmony\\\",\\\"name\\\":\\\"Harmony\\\",\\\"type\\\":\\\"string\\\",\\\"access\\\":\\\"read\\\"},"
"{\\\"key\\\":\\\"detected_root\\\",\\\"name\\\":\\\"Root\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"C\\\",\\\"C#\\\",\\\"D\\\",\\\"Eb\\\",\\\"E\\\",\\\"F\\\",\\\"F#\\\",\\\"G\\\",\\\"Ab\\\",\\\"A\\\",\\\"Bb\\\",\\\"B\\\"],\\\"options_as_string\\\":true,\\\"access\\\":\\\"read\\\"},"
"{\\\"key\\\":\\\"detected_bass\\\",\\\"name\\\":\\\"Bass\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"C\\\",\\\"C#\\\",\\\"D\\\",\\\"Eb\\\",\\\"E\\\",\\\"F\\\",\\\"F#\\\",\\\"G\\\",\\\"Ab\\\",\\\"A\\\",\\\"Bb\\\",\\\"B\\\"],\\\"options_as_string\\\":true,\\\"access\\\":\\\"read\\\"},"
"{\\\"key\\\":\\\"confidence\\\",\\\"name\\\":\\\"Confidence\\\",\\\"type\\\":\\\"int\\\",\\\"min\\\":0,\\\"max\\\":100,\\\"access\\\":\\\"read\\\"}"
"]";
static void set_param(void *value,const char *key,const char *parameter){Inst *instance=(Inst*)value;if(!instance||!key||!parameter)return;if(!strcmp(key,"live_press")){if(parameter[0]=='1')hb_receive_live_vouch(instance);return;}if(!strcmp(key,"approach_below_pad")){int down=parameter[0]=='1';if(g_bus.approach_mode==2)return;if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;}else instance->approach_below_held=(uint8_t)down;return;}if(!strcmp(key,"approach_above_pad")){int down=parameter[0]=='1';if(g_bus.approach_mode==2)return;if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;}else instance->approach_above_held=(uint8_t)down;return;}if(!strcmp(key,"approach_reset")){if(parameter[0]=='1'||!strcmp(parameter,"Reset")){instance->approach_pad_armed=1;instance->approach_below_held=0;instance->approach_above_held=0;g_bus.approach_control=1;}return;}if(!strcmp(key,"mod_scale_above")){int down=parameter[0]=='1'||!strcmp(parameter,"On");if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;else if(instance->approach_pad_armed==2)instance->approach_pad_armed=1;}else instance->approach_above_held=(uint8_t)down;return;}if(!strcmp(key,"mod_chrom_below")){int down=parameter[0]=='1'||!strcmp(parameter,"On");if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;else if(instance->approach_pad_armed==0)instance->approach_pad_armed=1;}else instance->approach_below_held=(uint8_t)down;return;}if(!strcmp(key,"track_role")||!strcmp(key,"role")){
    int new_role=enum_index(parameter,ROLE_OPTS,3,instance->role);
    if(new_role!=instance->role){
        hb_clear_instance_note_state(instance);
        memset(instance->source_seen,0,sizeof(instance->source_seen));
        instance->role=new_role;
        instance->resolved_source_channel=-1;
    }
    if(instance->role==0){if(!hb_load_clip_cache())hb_clear_clip_cache();}
}else if(!strcmp(key,"clip_slot")){g_bus.clip_slot=enum_index(parameter,CLIP_SLOT_OPTS,9,g_bus.clip_slot);if(instance->role==0){if(!hb_load_clip_cache())hb_clear_clip_cache();}}else if(!strcmp(key,"sensor_sources")){g_bus.sensor_sources=0;instance->dirty=1;instance->frames_since_change=0;}else if(!strcmp(key,"clip_context")){g_bus.clip_context=0;g_bus.sensor_sources=0;hb_clear_clip_cache();instance->dirty=1;instance->frames_since_change=0;}else if(!strcmp(key,"retrigger_held")){instance->retrigger_held=enum_index(parameter,RETRIGGER_OPTS,2,instance->retrigger_held);}else if(!strcmp(key,"follow_lookahead_ms")){int parsed=parse_i(parameter,instance->follow_lookahead_ms);if(parsed<0)parsed=0;if(parsed>100)parsed=100;instance->follow_lookahead_ms=parsed;}else if(!strcmp(key,"follower_root_policy")||!strcmp(key,"follower_source_policy")){hb_set_global_root_policy(enum_index(parameter,FOLLOWER_SOURCE_POLICY_OPTS,3,hb_global_root_policy()));}else if(!strcmp(key,"follower_explicit_root")||!strcmp(key,"follower_source_root")){hb_set_global_explicit_root(enum_index(parameter,PC_OPTS,12,hb_global_explicit_root()));}else if(!strcmp(key,"mode")){instance->mode=enum_index(parameter,MODE_OPTS,3,instance->mode);instance->follower_bus_seq=0;}else if(!strcmp(key,"content_map")){
    int value=hb_global_content_map();
    if(!strcmp(parameter,"In Chord"))value=0;
    else if(!strcmp(parameter,"In Scale"))value=1;
    else if(!strcmp(parameter,"Free"))value=2;
    else if(!strcmp(parameter,"In 135"))value=3;
    else if(!strcmp(parameter,"In 1357"))value=4;
    else if(!strcmp(parameter,"In 12357"))value=5;
    else if(!strcmp(parameter,"In 12356"))value=6;
    else if(!strcmp(parameter,"In Non-Avoid"))value=7;
    else if(!strcmp(parameter,"In 123567"))value=8;
    hb_set_global_content_map(value);
}else if(!strcmp(key,"travel_map")){static const char *opts[]={"Relative","Closest","Upward","Closest Split","Downward","Direct"};hb_set_global_travel_map(enum_index(parameter,opts,6,hb_global_travel_map()));}else if(!strcmp(key,"split_map")){static const char *opts[]={"Harm. / Out","135 / 2467","1357 / 246","Act. / Out"};g_bus.follower_split_map=enum_index(parameter,opts,4,g_bus.follower_split_map);}else if(!strcmp(key,"approach")){g_bus.approach_control=enum_index(parameter,APPROACH_OPTS,3,g_bus.approach_control);}else if(!strcmp(key,"approach_mode")){g_bus.approach_mode=enum_index(parameter,APPROACH_MODE_OPTS,3,g_bus.approach_mode);instance->approach_pad_armed=1;instance->approach_below_held=0;instance->approach_above_held=0;}else if(!strcmp(key,"map_target"))instance->map_target=enum_index(parameter,MAP_TARGET_OPTS,2,instance->map_target);else if(!strcmp(key,"source_channel")){int idx=enum_index(parameter,SOURCE_CH_OPTS,17,instance->source_channel+1);instance->source_channel=idx-1;instance->resolved_source_channel=-1;}else if(!strcmp(key,"render_channel")){int idx=enum_index(parameter,RENDER_CH_OPTS,17,instance->render_channel+1);instance->render_channel=idx-1;}else if(!strcmp(key,"quant_timing")){/* follower-render timing only */g_bus.quant_timing=enum_index(parameter,QUANT_GRID_OPTS,7,g_bus.quant_timing);}else if(!strcmp(key,"chord_timing")){/* follower-render timing only */g_bus.chord_timing=enum_index(parameter,TIMING_OPTS,7,g_bus.chord_timing);g_bus.chord_timescale=0;}else if(!strcmp(key,"anticipation")){/* follower-render timing only */g_bus.anticipation=enum_index(parameter,ANTICIPATION_OPTS,6,g_bus.anticipation);}else if(!strcmp(key,"boundary_buffer_ms")){/* follower-render capture window only */int parsed=parse_i(parameter,g_bus.boundary_buffer_ms);if(parsed<0)parsed=0;if(parsed>1000)parsed=1000;g_bus.boundary_buffer_ms=parsed;}else if(!strcmp(key,"analysis_release_ms")){int parsed=parse_i(parameter,g_bus.analysis_release_ms);if(parsed<0)parsed=0;if(parsed>500)parsed=500;g_bus.analysis_release_ms=parsed;}else if(!strcmp(key,"follower_scale")){hb_set_global_scale(enum_index(parameter,FOLLOWER_SCALE_OPTS,10,hb_global_scale()));}else if(!strcmp(key,"context")){g_bus.context=enum_index(parameter,CONTEXT_OPTS,7,g_bus.context);g_bus.stability=hb_context_to_legacy_stability(g_bus.context);}else if(!strcmp(key,"chord_timescale"))g_bus.chord_timescale=enum_index(parameter,TIMESCALE_OPTS,6,g_bus.chord_timescale);else if(!strcmp(key,"stability"))g_bus.stability=enum_index(parameter,STABILITY_OPTS,3,g_bus.stability);else if(!strcmp(key,"accidentals")){int previous=g_bus.accidentals;g_bus.accidentals=enum_index(parameter,ACCIDENTAL_OPTS,7,g_bus.accidentals);if(g_bus.accidentals==0&&previous!=0)g_bus.auto_spell_locked=0;}else if(!strcmp(key,"root_policy"))g_bus.global_root_policy=enum_index(parameter,POLICY_OPTS,3,g_bus.global_root_policy);else if(!strcmp(key,"explicit_root"))g_bus.global_explicit_root=enum_index(parameter,PC_OPTS,12,g_bus.global_explicit_root);else if(!strcmp(key,"input_root"))g_bus.global_input_root=enum_index(parameter,PC_OPTS,12,g_bus.global_input_root);else if(!strcmp(key,"transpose")){int parsed=parse_i(parameter,g_bus.global_transpose);if(parsed<-24)parsed=-24;if(parsed>24)parsed=24;g_bus.global_transpose=parsed;}else if(!strcmp(key,"window_ms")){int parsed=parse_i(parameter,g_bus.inference_window_ms);if(parsed<0)parsed=0;if(parsed>500)parsed=500;g_bus.inference_window_ms=parsed;}else if(!strcmp(key,"state"))hb_restore_state(instance,parameter);}
static void hb_restore_state(Inst *instance,const char *state){
    if(!instance||!state)return;
    int values[25];for(int i=0;i<25;i++)values[i]=-999;
    int parsed=sscanf(state,"hb15,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20],&values[21],&values[22],&values[23],&values[24]);
    int is_hb15=(parsed==25);
    if(!is_hb15)parsed=sscanf(state,"hb14,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20],&values[21],&values[22],&values[23]);
    int is_hb14=(!is_hb15&&parsed==24);
    if(!is_hb15&&!is_hb14)parsed=sscanf(state,"hb13,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20],&values[21],&values[22],&values[23]);
    int is_hb13=(!is_hb15&&!is_hb14&&parsed==24);
    if(!is_hb15&&!is_hb14&&!is_hb13)parsed=sscanf(state,"hb12,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20],&values[21],&values[22]);
    int is_hb12=(!is_hb13&&parsed==23);
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12)parsed=sscanf(state,"hb11,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],
        &values[18],&values[19],&values[20]);
    int is_hb11=(!is_hb13&&!is_hb12&&parsed==21);
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11)parsed=sscanf(state,"hb10,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],&values[18],&values[19]);
    int is_hb10=(!is_hb13&&!is_hb12&&!is_hb11&&parsed==20);
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10)parsed=sscanf(state,"hb9,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17],&values[18]);
    int is_hb9=(!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&parsed==19);
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9)parsed=sscanf(state,"hb8,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15],&values[16],&values[17]);
    if(parsed==18){
        /* Follower root policy/root are global live state. Per-track restore
           must not overwrite them; otherwise changing tracks makes "last
           visited track wins". */
    }
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=18){
        parsed=sscanf(state,"hb7,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14],&values[15]);
    }
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=16&&parsed!=18){
        parsed=sscanf(state,"hb6,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14]);
    }
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=15&&parsed!=16&&parsed!=18){
        parsed=sscanf(state,"hb5,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14],&values[15]);
    }
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=16&&parsed!=15&&parsed!=18){
        parsed=sscanf(state,"hb4,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14]);
    }
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=15&&parsed!=16&&parsed!=18){
        parsed=sscanf(state,"hb3,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],&values[12]);
    }
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=13&&parsed!=15&&parsed!=16&&parsed!=18){
        parsed=sscanf(state,"hb2,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11]);
    }
    if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed!=12&&parsed!=13&&parsed!=15&&parsed!=16&&parsed!=18){
        parsed=sscanf(state,"hb1,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10]);
        if(parsed!=11)return;
    }
    if(values[0]>=0&&values[0]<=2)instance->role=values[0];
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
    if((is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10||is_hb9||parsed>=12)&&values[11]>=-1&&values[11]<16)instance->render_channel=values[11];
    if((is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10||is_hb9||parsed==13||parsed>=15)&&values[12]>=-1&&values[12]<16){
        instance->source_channel=values[12];
        /* Restoring opaque state must never leave a cached owning-track
           channel from a previous UI/instance lifetime. Re-resolve direct
           MIDI routing from the current track tag/fallback. */
        instance->resolved_source_channel=-1;
    }
    if((is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10||is_hb9||parsed>=18)&&values[16]>=0&&values[16]<=100)instance->follow_lookahead_ms=values[16];
    if((is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10||is_hb9||parsed>=18)&&values[17]>=0&&values[17]<=1)instance->retrigger_held=values[17];
    /* Follower mapping controls are global from hb14 onward. Older per-track
       copies are ignored during migration to avoid last-visited-track wins. */
    instance->follow_lookahead_ms=0;
    if((is_hb15||is_hb14)&&values[0]==0&&!g_follower_globals_restored){
        /* The conductor snapshot is the canonical persisted copy of GLOBAL
           follower settings. Restore it exactly once after process/Move restart;
           later per-track/UI restores must never overwrite live globals. */
        if(values[8]>=0&&values[8]<4)g_bus.follower_split_map=values[8];
        if(values[4]>=0&&values[4]<3)hb_set_global_root_policy(values[4]);
        if(values[5]>=0&&values[5]<12)hb_set_global_explicit_root(values[5]);
        if(values[21]>=0&&values[21]<9)hb_set_global_content_map(values[21]);
        if(values[22]>=0&&values[22]<5)hb_set_global_travel_map(values[22]);
        if(values[23]>=0&&values[23]<10)hb_set_global_scale(values[23]);
        g_follower_globals_restored=1;
    }
    if((is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10||is_hb9)&&values[0]==0&&!instance->global_timing_restored){
        /* Restore shared timing only once for the conductor instance. Schwung
           may reapply opaque module state while navigating the UI; allowing
           that stale copy to overwrite live globals made Chord Grid snap back. */
        g_bus.inference_window_ms=25;
        if(values[13]>=0&&values[13]<7)g_bus.chord_timing=values[13];
        if(is_hb15&&values[24]>=0&&values[24]<7)g_bus.quant_timing=values[24];
        g_bus.context=0;
        if(values[18]>=0&&values[18]<6)g_bus.anticipation=values[18];
        if((is_hb15||is_hb14||is_hb13||is_hb12||is_hb11||is_hb10)&&values[19]>=0&&values[19]<=1000)g_bus.boundary_buffer_ms=values[19];
        if((is_hb15||is_hb14||is_hb13||is_hb12||is_hb11)&&values[20]>=0&&values[20]<=500)g_bus.analysis_release_ms=values[20];
        instance->global_timing_restored=1;
    }else if(!is_hb15&&!is_hb14&&!is_hb13&&!is_hb12&&!is_hb11&&!is_hb10&&!is_hb9&&parsed>=15&&values[0]==0&&!instance->global_timing_restored){
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
    if(instance->role==0)hb_clear_clip_cache();
}
static int get_param(void *value,const char *key,char *buffer,int length){Inst *instance=(Inst*)value;if(!instance||!key||!buffer||length<2)return -1;hb_harmony_t harmony=bus_read();if(!strcmp(key,"version"))return snprintf(buffer,(size_t)length,"%s",HB_VERSION);if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");if(!strcmp(key,"approach_status")){int active=hb_active_approach(instance);return snprintf(buffer,(size_t)length,"%s",APPROACH_OPTS[(active>=0&&active<3)?active:1]);}if(!strcmp(key,"touch_hint"))return snprintf(buffer,(size_t)length,"K6 OFF K7 UP K8 DN");if(!strcmp(key,"approach_reset"))return snprintf(buffer,(size_t)length,"Off");if(!strcmp(key,"mod_scale_above")){int on=g_bus.approach_mode==0?(instance->approach_pad_armed==2):(instance->approach_above_held!=0);return snprintf(buffer,(size_t)length,"%s",on?"On":"Off");}if(!strcmp(key,"mod_chrom_below")){int on=g_bus.approach_mode==0?(instance->approach_pad_armed==0):(instance->approach_below_held!=0);return snprintf(buffer,(size_t)length,"%s",on?"On":"Off");}if(!strcmp(key,"approach"))return snprintf(buffer,(size_t)length,"%s",APPROACH_OPTS[(g_bus.approach_control>=0&&g_bus.approach_control<3)?g_bus.approach_control:1]);if(!strcmp(key,"approach_mode")){int mode=g_bus.approach_mode;if(mode<0||mode>2)mode=0;return snprintf(buffer,(size_t)length,"%s",APPROACH_MODE_OPTS[mode]);}if(!strcmp(key,"approach_below_pad"))return snprintf(buffer,(size_t)length,"%d",instance->approach_below_held?1:0);if(!strcmp(key,"approach_above_pad"))return snprintf(buffer,(size_t)length,"%d",instance->approach_above_held?1:0);if(!strcmp(key,"retrigger_held"))return snprintf(buffer,(size_t)length,"%s",RETRIGGER_OPTS[instance->retrigger_held?1:0]);if(!strcmp(key,"follow_lookahead_ms"))return snprintf(buffer,(size_t)length,"%d",instance->follow_lookahead_ms);if(!strcmp(key,"conductor_this_1")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_local_conductor_note(instance,0),harmony,b,sizeof(b)));}
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
if(!strcmp(key,"follower_all_1")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_aggregate_follower_unique_note(0),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_all_2")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_aggregate_follower_unique_note(1),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_all_3")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_aggregate_follower_unique_note(2),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_all_4")){char b[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_aggregate_follower_unique_note(3),harmony,b,sizeof(b)));}
if(!strcmp(key,"follower_all_count"))return snprintf(buffer,(size_t)length,"%d",hb_aggregate_follower_unique_count());
if(!strcmp(key,"follower_this_role_1")){int n=hb_nth_local_follower_note(instance,0);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note(instance,n));}
if(!strcmp(key,"follower_this_role_2")){int n=hb_nth_local_follower_note(instance,1);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note(instance,n));}
if(!strcmp(key,"follower_this_role_3")){int n=hb_nth_local_follower_note(instance,2);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note(instance,n));}
if(!strcmp(key,"follower_this_role_4")){int n=hb_nth_local_follower_note(instance,3);return snprintf(buffer,(size_t)length,"%s",hb_follower_degree_role_for_note(instance,n));}
if(!strcmp(key,"follower_all_role_1"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,0));
if(!strcmp(key,"follower_all_role_2"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,1));
if(!strcmp(key,"follower_all_role_3"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,2));
if(!strcmp(key,"follower_all_role_4"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,3));
if(!strcmp(key,"follower_note_1_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,0),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_note_2_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,1),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_note_3_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,2),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_note_4_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,3),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_role_1"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,0));
if(!strcmp(key,"follower_role_2"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,1));
if(!strcmp(key,"follower_role_3"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,2));
if(!strcmp(key,"follower_role_4"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,3));
if(!strcmp(key,"follower_active_count"))return snprintf(buffer,(size_t)length,"%d",hb_follower_analysis_count(instance));if(!strcmp(key,"state"))return snprintf(buffer,(size_t)length,"hb15,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",instance->role,instance->mode,instance->map_target,g_bus.inference_window_ms,hb_global_root_policy(),hb_global_explicit_root(),g_bus.global_input_root,g_bus.global_transpose,g_bus.follower_split_map,g_bus.stability,g_bus.accidentals,instance->render_channel,instance->source_channel,g_bus.chord_timing,g_bus.context,g_bus.clip_context,instance->follow_lookahead_ms,instance->retrigger_held,g_bus.anticipation,g_bus.boundary_buffer_ms,g_bus.analysis_release_ms,hb_global_content_map(),hb_global_travel_map(),hb_global_scale(),g_bus.quant_timing);if(!strcmp(key,"track_role")||!strcmp(key,"role"))return snprintf(buffer,(size_t)length,"%s",ROLE_OPTS[instance->role]);if(!strcmp(key,"clip_slot"))return snprintf(buffer,(size_t)length,"%s",CLIP_SLOT_OPTS[g_bus.clip_slot]);if(!strcmp(key,"sensor_sources"))return snprintf(buffer,(size_t)length,"%s","Realtime");if(!strcmp(key,"realtime_route"))return snprintf(buffer,(size_t)length,"%s","Set Track MIDI Out -> Schwung");if(!strcmp(key,"clip_context"))return snprintf(buffer,(size_t)length,"%s","Realtime Only");if(!strcmp(key,"mode"))return snprintf(buffer,(size_t)length,"%s",MODE_OPTS[instance->mode]);
if(!strcmp(key,"content_map")){
    static const char *opts[]={"In Chord","In Scale","Free","In 135","In 1357","In 12357","In 12356","In Non-Avoid","In 123567"};
    return snprintf(buffer,(size_t)length,"%s",opts[hb_global_content_map()]);
}
if(!strcmp(key,"travel_map")){static const char *opts[]={"Relative","Closest","Upward","Closest Split","Downward","Direct"};int travel=hb_global_travel_map();if(travel<0||travel>5)travel=0;return snprintf(buffer,(size_t)length,"%s",opts[travel]);}if(!strcmp(key,"split_map")){static const char *opts[]={"Harm. / Out","135 / 2467","1357 / 246","Act. / Out"};int split=g_bus.follower_split_map;if(split<0||split>3)split=0;return snprintf(buffer,(size_t)length,"%s",opts[split]);}
if(!strcmp(key,"map_target"))return snprintf(buffer,(size_t)length,"%s",MAP_TARGET_OPTS[instance->map_target]);if(!strcmp(key,"source_channel"))return snprintf(buffer,(size_t)length,"%s",SOURCE_CH_OPTS[instance->source_channel+1]);if(!strcmp(key,"resolved_source_channel"))return snprintf(buffer,(size_t)length,"%d",instance->resolved_source_channel);if(!strcmp(key,"render_channel"))return snprintf(buffer,(size_t)length,"%s",RENDER_CH_OPTS[instance->render_channel+1]);if(!strcmp(key,"quant_timing"))return snprintf(buffer,(size_t)length,"%s",QUANT_GRID_OPTS[g_bus.quant_timing]);if(!strcmp(key,"chord_timing"))return snprintf(buffer,(size_t)length,"%s",TIMING_OPTS[g_bus.chord_timing]);if(!strcmp(key,"anticipation"))return snprintf(buffer,(size_t)length,"%s",ANTICIPATION_OPTS[g_bus.anticipation]);if(!strcmp(key,"boundary_buffer_ms"))return snprintf(buffer,(size_t)length,"%d",g_bus.boundary_buffer_ms);if(!strcmp(key,"analysis_release_ms"))return snprintf(buffer,(size_t)length,"%d",g_bus.analysis_release_ms);if(!strcmp(key,"follower_scale"))return snprintf(buffer,(size_t)length,"%s",FOLLOWER_SCALE_OPTS[hb_global_scale()]);if(!strcmp(key,"context"))return snprintf(buffer,(size_t)length,"%s",CONTEXT_OPTS[g_bus.context]);if(!strcmp(key,"chord_timescale"))return snprintf(buffer,(size_t)length,"%s",TIMESCALE_OPTS[g_bus.chord_timescale]);if(!strcmp(key,"stability"))return snprintf(buffer,(size_t)length,"%s",STABILITY_OPTS[g_bus.stability]);if(!strcmp(key,"accidentals"))return snprintf(buffer,(size_t)length,"%s",ACCIDENTAL_OPTS[g_bus.accidentals]);if(!strcmp(key,"root_policy"))return snprintf(buffer,(size_t)length,"%s",POLICY_OPTS[g_bus.global_root_policy]);if(!strcmp(key,"explicit_root"))return snprintf(buffer,(size_t)length,"%s",PC_OPTS[g_bus.global_explicit_root]);if(!strcmp(key,"input_root"))return snprintf(buffer,(size_t)length,"%s",PC_OPTS[g_bus.global_input_root]);if(!strcmp(key,"transpose"))return snprintf(buffer,(size_t)length,"%d",g_bus.global_transpose);if(!strcmp(key,"window_ms"))return snprintf(buffer,(size_t)length,"%d",g_bus.inference_window_ms);if(!strcmp(key,"detected_root"))return snprintf(buffer,(size_t)length,"%s",harmony.valid?hb_pc_display(harmony.root_pc,harmony):"--");if(!strcmp(key,"detected_bass"))return snprintf(buffer,(size_t)length,"%s",harmony.valid?hb_pc_display(harmony.bass_pc,harmony):"--");if(!strcmp(key,"detected_quality"))return snprintf(buffer,(size_t)length,"%d",harmony.valid?harmony.chord_index+1:0);if(!strcmp(key,"confidence"))return snprintf(buffer,(size_t)length,"%d",harmony.valid?harmony.confidence:0);if(!strcmp(key,"resolved_root"))return snprintf(buffer,(size_t)length,"%d",reference_root(instance));if(!strcmp(key,"harmony"))return hb_format_harmony(buffer,length,harmony);if(!strcmp(key,"candidate_harmony"))return hb_format_harmony(buffer,length,instance->candidate_harmony);if(!strcmp(key,"pitch_mask"))return snprintf(buffer,(size_t)length,"%u",(unsigned)harmony.pitch_mask);if(!strcmp(key,"rx_count"))return snprintf(buffer,(size_t)length,"%u",instance->rx_count);if(!strcmp(key,"note_on_count"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"note_off_count"))return snprintf(buffer,(size_t)length,"%u",instance->note_off_count);if(!strcmp(key,"last_note"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"last_status"))return snprintf(buffer,(size_t)length,"%d",instance->last_status);if(!strcmp(key,"last_velocity"))return snprintf(buffer,(size_t)length,"%d",instance->last_velocity);if(!strcmp(key,"diag_midi_events"))return snprintf(buffer,(size_t)length,"%u",instance->rx_count);if(!strcmp(key,"diag_note_ons"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"diag_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"diag_recv_ch")){int recv=hb_monitor_channel(instance);return snprintf(buffer,(size_t)length,"%d",(recv>=0&&recv<16)?recv+1:-1);}if(!strcmp(key,"diag_event_ch")){int type=instance->last_status&0xF0;int event_ch=(type==0x80||type==0x90)?(instance->last_status&15):-1;return snprintf(buffer,(size_t)length,"%d",event_ch>=0?event_ch+1:-1);}if(!strcmp(key,"live_press_count"))return snprintf(buffer,(size_t)length,"%u",instance->live_press_count);if(!strcmp(key,"rt_note_count"))return snprintf(buffer,(size_t)length,"%d",hb_held_count(instance));if(!strcmp(key,"rt_note_ons"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"rt_note_offs"))return snprintf(buffer,(size_t)length,"%u",instance->note_off_count);if(!strcmp(key,"rt_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"rt_last_channel")){int t=instance->last_status&0xF0;return snprintf(buffer,(size_t)length,"%d",(t==0x80||t==0x90)?(instance->last_status&15):-1);}if(!strcmp(key,"clip_active_count")){uint8_t temp_notes[32];int temp_count=hb_clip_active_notes(temp_notes,32);return snprintf(buffer,(size_t)length,"%d",temp_count);}if(!strcmp(key,"active_count"))return snprintf(buffer,(size_t)length,"%d",hb_conductor_analysis_count(instance));if(!strcmp(key,"active_note_list"))return hb_format_active_notes(instance,buffer,length);if(!strcmp(key,"callback_active_notes"))return snprintf(buffer,(size_t)length,"%d",active_notes(instance,(uint8_t[128]){0}));if(!strcmp(key,"pending_releases")){int n=0;for(int i=0;i<128;i++)if(instance->pending_off_frames[i]>0)n++;return snprintf(buffer,(size_t)length,"%d",n);}if(!strcmp(key,"active_pc_mask"))return snprintf(buffer,(size_t)length,"%u",hb_active_pc_mask(instance));if(!strcmp(key,"active_note_1"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,0));if(!strcmp(key,"active_note_2"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,1));if(!strcmp(key,"active_note_3"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,2));if(!strcmp(key,"active_note_4"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,3));if(!strcmp(key,"active_note_4_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,3),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"active_note_3_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,2),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"active_note_2_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,1),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"active_note_1_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,0),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"clip_stage")){static const char *names[]={"idle","no active set","active set","no conductor","conductor found","no Song.abl","Song.abl found","no track","track found","no clip slots","no clip","clip found","no notes array","empty clip","OK"};int stage=g_bus.clip_stage;if(stage<0||stage>14)stage=0;return snprintf(buffer,(size_t)length,"%s",names[stage]);}if(!strcmp(key,"cache_rev"))return snprintf(buffer,(size_t)length,"%u",g_bus.cache_rev);if(!strcmp(key,"sense_rev"))return snprintf(buffer,(size_t)length,"%u",g_bus.sense_rev);if(!strcmp(key,"clip_track"))return snprintf(buffer,(size_t)length,"%d",g_bus.clip_context?g_bus.clip_track:-1);if(!strcmp(key,"clip_note_count"))return snprintf(buffer,(size_t)length,"%d",g_bus.clip_context?g_bus.clip_note_count:0);if(!strcmp(key,"clip_playhead"))return g_bus.clip_context?snprintf(buffer,(size_t)length,"%.3f",hb_clip_playhead()):snprintf(buffer,(size_t)length,"%s","realtime");if(!strcmp(key,"infer_note_count")){uint8_t notes[128];return snprintf(buffer,(size_t)length,"%d",active_notes(instance,notes));}if(!strcmp(key,"bus_seq"))return snprintf(buffer,(size_t)length,"%u",__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE));if(!strcmp(key,"raw_event_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_event_count);if(!strcmp(key,"raw_note_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_count);if(!strcmp(key,"raw_note_on_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_on_count);if(!strcmp(key,"raw_note_off_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_off_count);if(!strcmp(key,"raw_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_note);if(!strcmp(key,"raw_last_status"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_status);if(!strcmp(key,"raw_last_velocity"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_velocity);if(!strcmp(key,"raw_last_channel"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_channel);if(!strcmp(key,"raw_track_match"))return snprintf(buffer,(size_t)length,"%d",(g_bus.clip_track>=0&&instance->raw_last_channel==g_bus.clip_track)?1:0);if(!strcmp(key,"raw_last_cable"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_cable);if(!strcmp(key,"inject_available"))return snprintf(buffer,(size_t)length,"%d",(g_host&&g_host->midi_inject_to_move)?1:0);if(!strcmp(key,"render_count"))return snprintf(buffer,(size_t)length,"%u",instance->render_count);if(!strcmp(key,"render_fail_count"))return snprintf(buffer,(size_t)length,"%u",instance->render_fail_count);if(!strcmp(key,"render_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->render_last_note);if(!strcmp(key,"follower_last_in"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"follower_last_out"))return snprintf(buffer,(size_t)length,"%d",instance->render_channel>=0?instance->render_last_note:(instance->last_note>=0&&instance->last_note<128?instance->mapped[instance->last_note]:-1));if(!strcmp(key,"follower_root_policy")||!strcmp(key,"follower_source_policy"))return snprintf(buffer,(size_t)length,"%s",FOLLOWER_SOURCE_POLICY_OPTS[hb_global_root_policy()]);if(!strcmp(key,"follower_explicit_root")||!strcmp(key,"follower_source_root"))return snprintf(buffer,(size_t)length,"%s",PC_OPTS[hb_global_explicit_root()]);if(!strcmp(key,"inferred_root")){int root=0;if(!hb_infer_follower_root(instance,&root))return snprintf(buffer,(size_t)length,"%s","--");return snprintf(buffer,(size_t)length,"%s",PC_OPTS[root]);}if(!strcmp(key,"role_reference_root")||!strcmp(key,"used_root")){int root=0;if(!hb_resolve_follower_reference_root(instance,&root))return snprintf(buffer,(size_t)length,"%s","--");return snprintf(buffer,(size_t)length,"%s",PC_OPTS[root]);}if(!strcmp(key,"source_note_1")){char note_buf[8];int note=hb_nth_aggregate_conductor_note(0);return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(note,harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"source_of_1"))return hb_format_conductor_source(hb_nth_aggregate_conductor_note(0),buffer,length);if(!strcmp(key,"source_note_2")){char note_buf[8];int note=hb_nth_aggregate_conductor_note(1);return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(note,harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"source_of_2"))return hb_format_conductor_source(hb_nth_aggregate_conductor_note(1),buffer,length);if(!strcmp(key,"source_note_3")){char note_buf[8];int note=hb_nth_aggregate_conductor_note(2);return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(note,harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"source_of_3"))return hb_format_conductor_source(hb_nth_aggregate_conductor_note(2),buffer,length);if(!strcmp(key,"source_note_4")){char note_buf[8];int note=hb_nth_aggregate_conductor_note(3);return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(note,harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"source_of_4"))return hb_format_conductor_source(hb_nth_aggregate_conductor_note(3),buffer,length);if(!strcmp(key,"monitor_status"))return snprintf(buffer,(size_t)length,"%s",g_monitor?"ACTIVE":"ABSENT");
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
static midi_fx_api_v1_t API={MIDI_FX_API_VERSION,create_inst,destroy_inst,process,tick,set_param,get_param};
__attribute__((visibility("default")))
midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host){g_host=host;ensure_init();return &API;}
