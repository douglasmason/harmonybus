/* Harmony Bus v0.1.77 — Schwung MIDI FX. */
#define HB_VERSION "0.1.77"
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
typedef struct {
    int note;
    double start;
    double duration;
} hb_clip_note_t;
typedef struct { volatile unsigned seq; hb_harmony_t harmony; int global_transpose; int global_root_policy; int global_explicit_root; int global_input_root; int sensor_sources; int chord_timescale; int stability; int chord_timing; int context; int accidentals; int auto_spell_sharps; int auto_spell_locked; int clip_track; int clip_slot; int clip_valid; int clip_note_count; int clip_stage; int clip_context; int last_clock_status; double clip_loop_start; double clip_loop_end; unsigned long clip_clock_ticks; unsigned clip_refresh_counter; double last_clip_playhead; int have_last_clip_playhead; unsigned cache_rev; unsigned sense_rev; int last_sense_count; uint8_t last_sense_notes[64]; hb_clip_note_t clip_notes[HB_MAX_CLIP_NOTES]; } SharedBus;
static SharedBus g_bus={0}; static int g_init=0;
typedef struct { int used,role,mode,window_ms,dirty,frames_since_change; uint8_t active[128]; uint8_t held_now[128]; int pending_off_frames[128]; int mapped[128]; uint8_t follower_held[128]; uint8_t follower_velocity[128]; unsigned follower_bus_seq; hb_harmony_t follower_source_harmony; uint8_t source_seen[12]; int resolved_root,resolved_confidence; unsigned rx_count; unsigned note_on_count; unsigned note_off_count; int last_note; int last_status; int last_velocity; int active_count; int last_inferred_count; unsigned raw_event_count; unsigned raw_note_count; unsigned raw_note_on_count; unsigned raw_note_off_count; int raw_last_note; int raw_last_status; int raw_last_velocity; int raw_last_channel; int raw_last_cable; uint8_t raw_prev[HB_MIDI_OUT_BYTES]; int map_target; hb_harmony_t candidate_harmony; int candidate_frames; int committed_frames; int render_channel; int source_channel; int resolved_source_channel; unsigned live_press_count; int live_vouch_pending; int live_vouch_age; int recent_live_note[16]; int recent_live_age[16]; uint8_t recent_live_valid[16]; unsigned render_count; unsigned render_fail_count; int render_last_note; int retrigger_held; uint8_t follower_role_interval[128]; uint8_t published_conductor[128]; uint8_t published_follower[128]; int settle_frames_remaining; int clip_event_idle_frames; } Inst;
static hb_harmony_t hb_mapping_target(hb_harmony_t harmony,int map_target);
static int reference_root(Inst *instance);
static int hb_nth_held_note(const Inst *instance,int ordinal);
static int hb_held_count(const Inst *instance);
static int hb_nth_follower_note(const Inst *instance,int ordinal);
static int hb_follower_held_count(const Inst *instance);
static const char *hb_role_name_for_interval(int interval);
static const char *hb_follower_role_name(const Inst *instance,int ordinal);
static Inst g_pool[HB_MAX_INSTANCES];
static int mod12(int value){value%=12;return value<0?value+12:value;}
static int parse_i(const char *value,int fallback){char *end;long parsed;if(!value||!*value)return fallback;end=0;parsed=strtol(value,&end,10);return end==value?fallback:(int)parsed;}
static void ensure_init(void){if(g_init)return;memset(&g_bus,0,sizeof(g_bus));g_bus.global_root_policy=2;g_bus.sensor_sources=0;g_bus.chord_timescale=3;g_bus.stability=1;g_bus.chord_timing=5;g_bus.context=3;g_bus.accidentals=0;g_bus.auto_spell_sharps=1;g_bus.auto_spell_locked=0;g_bus.clip_track=-1;g_bus.clip_slot=0;g_bus.clip_stage=0;g_bus.clip_context=1;g_bus.last_clock_status=-1;g_bus.last_clip_playhead=0.0;g_bus.have_last_clip_playhead=0;g_bus.cache_rev=0;g_bus.sense_rev=0;g_bus.last_sense_count=0;g_bus.clip_loop_start=0.0;g_bus.clip_loop_end=4.0;for(int index=0;index<HB_MAX_INSTANCES;index++){memset(&g_pool[index],0,sizeof(g_pool[index]));for(int note=0;note<128;note++)g_pool[index].mapped[note]=-1;}g_init=1;}

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
        int is_conductor=(strstr(json,"hb7,0,")||strstr(json,"hb6,0,")||strstr(json,"hb5,0,")||strstr(json,"hb4,0,")||strstr(json,"hb3,0,")||strstr(json,"hb2,0,")||strstr(json,"hb1,0,"));
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
            if(conductor->published_conductor[note])seen[note]=1;
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
    if(ordinal<0||ordinal>=g_bus.last_sense_count||ordinal>=64)return -1;
    return g_bus.last_sense_notes[ordinal];
}
static int hb_nth_conductor_analysis_note(const Inst *instance,int ordinal){
    if(!instance||ordinal<0)return -1;
    if(instance->role==0){int seen=0;for(int note=0;note<128;note++)if(instance->published_conductor[note]){if(seen==ordinal)return note;seen++;}return -1;}
    uint8_t notes[64];int count=hb_observed_notes(instance,notes,64);
    return ordinal<count?notes[ordinal]:-1;
}
static int hb_conductor_analysis_count(const Inst *instance){
    if(!instance)return 0;
    if(instance->role==0){int count=0;for(int note=0;note<128;note++)if(instance->published_conductor[note])count++;return count;}
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
static const char *hb_current_role_name_for_note(const Inst *instance,int note){
    if(!instance||note<0||note>127||!instance->published_follower[note])return "--";
    int interval=instance->follower_role_interval[note];
    if(interval<0||interval>11)return "--";
    return hb_role_name_for_interval(interval);
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
static void bus_write(hb_harmony_t harmony){unsigned sequence=__atomic_load_n(&g_bus.seq,__ATOMIC_RELAXED);__atomic_store_n(&g_bus.seq,sequence+1,__ATOMIC_RELEASE);g_bus.harmony=harmony;__atomic_store_n(&g_bus.seq,sequence+2,__ATOMIC_RELEASE);}
static hb_harmony_t bus_read(void){hb_harmony_t harmony;memset(&harmony,0,sizeof(harmony));for(int tries=0;tries<3;tries++){unsigned before=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE),after;if(before&1u)continue;harmony=g_bus.harmony;after=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);if(before==after&&!(after&1u))return harmony;}return harmony;}

static void hb_render_follower_event(Inst *instance,int source_note,int velocity,int is_on,int is_off,int recv_channel){
    if(!instance||instance->role!=1||instance->render_channel<0||!g_host||!g_host->midi_inject_to_move)return;
    if(instance->render_channel==recv_channel)return; /* avoid self-echo loops */
    int mapped=source_note;
    if(is_on){
        hb_harmony_t harmony=hb_mapping_target(bus_read(),instance->map_target);
        int render_root=reference_root(instance);
        mapped=hb_map_note(source_note,render_root,harmony,(hb_map_mode_t)instance->mode);
        instance->mapped[source_note]=mapped;
    }else if(is_off){
        mapped=instance->mapped[source_note];
        if(mapped<0)mapped=source_note;
        instance->mapped[source_note]=-1;
    }else return;
    uint8_t packet[4];
    packet[0]=(uint8_t)(0x20 | (is_on?0x09:0x08)); /* cable 2 + CIN */
    packet[1]=(uint8_t)((is_on?0x90:0x80) | (instance->render_channel & 0x0F));
    packet[2]=(uint8_t)(mapped & 0x7F);
    packet[3]=(uint8_t)(is_on?velocity:0);
    int sent=g_host->midi_inject_to_move(packet,4);
    if(sent==4){instance->render_count++;instance->render_last_note=mapped;}
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
static int hb_reharmonize_held_follower(Inst *instance,uint8_t output[][3],int lengths[],int max_output){
    if(!instance||instance->role!=1||!output||!lengths||max_output<=0)return 0;
    unsigned bus_seq=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);
    if(bus_seq==instance->follower_bus_seq)return 0;
    instance->follower_bus_seq=bus_seq;
    hb_harmony_t harmony=hb_mapping_target(bus_read(),instance->map_target);
    if(!harmony.valid)return 0;

    int render_root=reference_root(instance);
    uint8_t source_notes[128];
    int previous_outputs[128];
    int new_outputs[128];
    int role_changed[128];
    int voice_count=0;
    for(int source_note=0;source_note<128;source_note++){
        if(!instance->follower_held[source_note])continue;
        source_notes[voice_count]=(uint8_t)source_note;
        previous_outputs[voice_count]=instance->mapped[source_note];
        new_outputs[voice_count]=previous_outputs[voice_count];
        int next_role=mod12(source_note-bus_read().root_pc);
        role_changed[voice_count]=(instance->follower_role_interval[source_note]!=255&&instance->follower_role_interval[source_note]!=next_role);
        instance->follower_role_interval[source_note]=(uint8_t)next_role;
        voice_count++;
    }

    if((instance->mode==0||instance->mode==1)&&instance->follower_source_harmony.valid)
        hb_map_held_voices_by_role(source_notes,voice_count,
                                   instance->follower_source_harmony,bus_read(),
                                   instance->mode==1,previous_outputs,new_outputs);
    else
        hb_map_held_voices(source_notes,voice_count,render_root,harmony,
                           HB_MAP_NEAREST,previous_outputs,new_outputs);

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
    int recv_channel = -1;
    if (g_host->slot_recv_channel) recv_channel = g_host->slot_recv_channel(instance);

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
    if(note<0)return "--";
    hb_harmony_t source=instance->follower_source_harmony;
    if(!source.valid)return "--";
    return hb_role_name_for_interval(note-source.root_pc);
}
static int hb_nth_active_note(const Inst *instance,int ordinal){
    if(!instance||ordinal<0)return -1;
    int seen=0;
    for(int note=0;note<128;note++){
        if(instance->active[note]){
            if(seen==ordinal)return note;
            seen++;
        }
    }
    return -1;
}
static int hb_format_active_notes(const Inst *instance,char *buffer,int length){
    if(!instance||!buffer||length<2)return -1;
    int used=0;
    for(int note=0;note<128;note++){
        if(!instance->active[note])continue;
        int written=snprintf(buffer+used,(size_t)(length-used),used? ",%d":"%d",note);
        if(written<0||used+written>=length)return used;
        used+=written;
    }
    if(used==0)return snprintf(buffer,(size_t)length,"--");
    return used;
}
static unsigned hb_active_pc_mask(const Inst *instance){
    unsigned mask=0;
    if(!instance)return 0;
    for(int note=0;note<128;note++)if(instance->active[note])mask|=(1u<<(note%12));
    return mask;
}
static int active_notes(const Inst *instance,uint8_t *output){int count=0;for(int note=0;note<128;note++)if(instance->active[note])output[count++]=(uint8_t)note;return count;}
static int infer_reference_root(Inst *instance){static const int major[7]={0,2,4,5,7,9,11};static const int minor[7]={0,2,3,5,7,8,10};int pitch_classes=0,best=-999,best_root=instance->resolved_root;for(int index=0;index<12;index++)pitch_classes+=instance->source_seen[index]?1:0;if(!pitch_classes)return instance->resolved_root;for(int root=0;root<12;root++)for(int scale=0;scale<2;scale++){int score=0;for(int pitch_class=0;pitch_class<12;pitch_class++)if(instance->source_seen[pitch_class]){int relative=mod12(pitch_class-root),inside=0;for(int degree=0;degree<7;degree++)if(relative==(scale?minor[degree]:major[degree])){inside=1;break;}score+=inside?5:-4;}if(instance->source_seen[root])score+=3;if(score>best){best=score;best_root=root;}}instance->resolved_confidence=pitch_classes>=4?80:(pitch_classes>=3?65:45);return best_root;}
static int reference_root(Inst *instance){if(g_bus.global_root_policy==0)return mod12(g_bus.global_explicit_root);if(g_bus.global_root_policy==1)return mod12(g_bus.global_input_root);instance->resolved_root=infer_reference_root(instance);return mod12(instance->resolved_root);}
static void *create_inst(const char *module_dir,const char *config_json){(void)module_dir;(void)config_json;ensure_init();for(int index=0;index<HB_MAX_INSTANCES;index++)if(!g_pool[index].used){Inst *instance=&g_pool[index];memset(instance,0,sizeof(*instance));instance->used=1;instance->role=0;instance->mode=0;instance->map_target=0;instance->window_ms=70;instance->last_note=-1;instance->last_status=-1;instance->last_velocity=-1;instance->raw_last_note=-1;instance->raw_last_status=-1;instance->raw_last_velocity=-1;instance->raw_last_channel=-1;instance->raw_last_cable=-1;instance->render_channel=-1;instance->source_channel=-1;instance->resolved_source_channel=-1;instance->render_last_note=-1;instance->retrigger_held=1;for(int note=0;note<128;note++){instance->mapped[note]=-1;instance->follower_role_interval[note]=255;}return instance;}return 0;}
static void destroy_inst(void *value){Inst *instance=(Inst*)value;if(instance)instance->used=0;}
static int hb_source_channel_matches(Inst *instance,int midi_channel){
    if(!instance)return 0;
    if(instance->source_channel>=0)return midi_channel==instance->source_channel;
    if(instance->resolved_source_channel>=0)return midi_channel==instance->resolved_source_channel;
    int slot_channel=-1;
    if(g_host&&g_host->slot_recv_channel)slot_channel=g_host->slot_recv_channel(instance);
    if(slot_channel>=0&&slot_channel<16){
        instance->resolved_source_channel=slot_channel;
        return midi_channel==slot_channel;
    }
    instance->resolved_source_channel=midi_channel;
    return 1;
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
    memset(instance->held_now,0,sizeof(instance->held_now));
    memset(instance->active,0,sizeof(instance->active));
    memset(instance->pending_off_frames,0,sizeof(instance->pending_off_frames));
    memset(instance->follower_held,0,sizeof(instance->follower_held));
    memset(instance->follower_velocity,0,sizeof(instance->follower_velocity));memset(instance->published_conductor,0,sizeof(instance->published_conductor));memset(instance->published_follower,0,sizeof(instance->published_follower));instance->settle_frames_remaining=0;instance->clip_event_idle_frames=0;
    for(int note=0;note<128;note++){
        instance->mapped[note]=-1;
        instance->follower_role_interval[note]=255;
    }
    memset(&instance->follower_source_harmony,0,sizeof(instance->follower_source_harmony));
    instance->active_count=0;
    instance->last_inferred_count=0;
    instance->candidate_frames=0;
    instance->dirty=1;
    instance->frames_since_change=0;
}
static int hb_transport_playing(void){
    return g_host&&g_host->get_clock_status&&g_host->get_clock_status()==1;
}
static void hb_publish_instance_notes(Inst *instance){
    if(!instance)return;
    memcpy(instance->published_conductor,instance->held_now,sizeof(instance->published_conductor));
    memcpy(instance->published_follower,instance->follower_held,sizeof(instance->published_follower));
}
static void hb_schedule_or_publish_notes(Inst *instance){
    if(!instance)return;
    if(hb_transport_playing()){
        int sample_rate=(g_host&&g_host->sample_rate>0)?g_host->sample_rate:44100;
        int settle_ms=instance->window_ms>0?instance->window_ms:70;
        instance->settle_frames_remaining=(sample_rate*settle_ms)/1000;
    }else{
        instance->settle_frames_remaining=0;
        hb_publish_instance_notes(instance);
    }
}
static void hb_maybe_start_new_clip_phrase(Inst *instance,int is_on){
    if(!instance||!is_on||!hb_transport_playing())return;
    int sample_rate=(g_host&&g_host->sample_rate>0)?g_host->sample_rate:44100;
    int threshold_ms=instance->window_ms>0?instance->window_ms:70;
    int threshold_frames=(sample_rate*threshold_ms)/1000;
    if(instance->clip_event_idle_frames>=threshold_frames){
        /* A fresh note-on burst after a quiet gap starts a new clip phrase.
           Any still-held state here is orphaned from the prior phrase. */
        memset(instance->held_now,0,sizeof(instance->held_now));
        memset(instance->follower_held,0,sizeof(instance->follower_held));
        memset(instance->follower_velocity,0,sizeof(instance->follower_velocity));
        memset(instance->published_conductor,0,sizeof(instance->published_conductor));
        memset(instance->published_follower,0,sizeof(instance->published_follower));
        for(int note=0;note<128;note++){
            instance->mapped[note]=-1;
            instance->follower_role_interval[note]=255;
        }
        memset(&instance->follower_source_harmony,0,sizeof(instance->follower_source_harmony));
        instance->active_count=0;
        instance->candidate_frames=0;
        instance->dirty=1;
        instance->frames_since_change=0;
    }
}
static int pass(const uint8_t *input,int length,uint8_t output[][3],int lengths[],int max_output){if(!input||length<1||length>3||max_output<1)return 0;memcpy(output[0],input,(size_t)length);lengths[0]=length;return 1;}
static int process(void *value,const uint8_t *input,int length,uint8_t output[][3],int lengths[],int max_output){Inst *instance=(Inst*)value;if(!instance||!input||length<1)return 0;
if(input[0]==0xFA||input[0]==0xFB||input[0]==0xFC){
    if(input[0]==0xFA)g_bus.clip_clock_ticks=0;
    hb_clear_instance_note_state(instance);
    if(input[0]==0xFC)instance->resolved_source_channel=-1;
}
else if(instance->role==0&&input[0]==0xF8)g_bus.clip_clock_ticks++;
instance->rx_count++;instance->last_status=input[0];if(length>=2)instance->last_note=input[1]&0x7F;if(length>=3)instance->last_velocity=input[2];int status=input[0]&0xF0,is_on=(status==0x90&&length>=3&&input[2]>0),is_off=(status==0x80&&length>=3)||(status==0x90&&length>=3&&input[2]==0);
if(status==0xB0&&length>=3&&(input[1]==120||input[1]==123)){
    int control_channel=input[0]&0x0F;
    if(hb_source_channel_matches(instance,control_channel))hb_clear_instance_note_state(instance);
    return pass(input,length,output,lengths,max_output);
}
if(!(is_on||is_off))return pass(input,length,output,lengths,max_output);int note=input[1]&0x7F,mapped;int input_channel=input[0]&0x0F;if(instance->role==2)return pass(input,length,output,lengths,max_output);if(!hb_source_channel_matches(instance,input_channel))return pass(input,length,output,lengths,max_output);hb_maybe_start_new_clip_phrase(instance,is_on);instance->clip_event_idle_frames=0;if(is_on){instance->note_on_count++;instance->active_count++;}else if(is_off){instance->note_off_count++;if(instance->active_count>0)instance->active_count--;}if(instance->role==0){if(is_on){instance->held_now[note]=1;instance->pending_off_frames[note]=0;mapped=note+g_bus.global_transpose;if(mapped<0)mapped=0;if(mapped>127)mapped=127;instance->mapped[note]=mapped;}else{instance->held_now[note]=0;instance->pending_off_frames[note]=0;mapped=instance->mapped[note];if(mapped<0)mapped=note+g_bus.global_transpose;if(mapped<0)mapped=0;if(mapped>127)mapped=127;instance->mapped[note]=-1;}instance->candidate_frames=0;instance->dirty=1;instance->frames_since_change=0;hb_schedule_or_publish_notes(instance);if(max_output<1)return 0;output[0][0]=input[0];output[0][1]=(uint8_t)mapped;output[0][2]=length>=3?input[2]:0;lengths[0]=3;return 1;}if(is_on){instance->follower_held[note]=1;instance->follower_velocity[note]=(uint8_t)(length>=3?input[2]:100);instance->source_seen[note%12]=1;hb_harmony_t harmony=bus_read();instance->follower_role_interval[note]=harmony.valid?(uint8_t)mod12(note-harmony.root_pc):255;if(!instance->follower_source_harmony.valid&&harmony.valid)instance->follower_source_harmony=harmony;if((instance->mode==0||instance->mode==1)&&instance->follower_source_harmony.valid)mapped=hb_map_note_by_role(note,instance->follower_source_harmony,harmony,instance->mode==1);else{harmony=hb_mapping_target(harmony,instance->map_target);mapped=hb_map_note(note,reference_root(instance),harmony,HB_MAP_NEAREST);}instance->mapped[note]=mapped;}else{instance->follower_held[note]=0;instance->follower_velocity[note]=0;mapped=instance->mapped[note];if(mapped<0)mapped=note;instance->mapped[note]=-1;instance->follower_role_interval[note]=255;if(hb_follower_held_count(instance)==0)memset(&instance->follower_source_harmony,0,sizeof(instance->follower_source_harmony));}hb_schedule_or_publish_notes(instance);
if(instance->render_channel>=0){
    int recv_channel=(g_host&&g_host->slot_recv_channel)?g_host->slot_recv_channel(instance):-1;
    if(input_channel!=instance->render_channel){
        hb_render_follower_event(instance,note,length>=3?input[2]:0,is_on,is_off,recv_channel);
    }
}
if(max_output<1)return 0;output[0][0]=input[0];output[0][1]=(uint8_t)mapped;output[0][2]=length>=3?input[2]:0;lengths[0]=3;return 1;}
static int tick(void *value,int frames,int sample_rate,uint8_t output[][3],int lengths[],int max_output){
    Inst *instance=(Inst*)value;
    if(!instance)return 0;
    if(hb_transport_playing())instance->clip_event_idle_frames+=frames;else instance->clip_event_idle_frames=0;
    if(instance->settle_frames_remaining>0){
        instance->settle_frames_remaining-=frames;
        if(instance->settle_frames_remaining<=0){
            instance->settle_frames_remaining=0;
            hb_publish_instance_notes(instance);
            if(instance->role==0){instance->candidate_frames=0;instance->dirty=1;instance->frames_since_change=0;}
        }
    }
    if(instance->role==1){
        return hb_reharmonize_held_follower(instance,output,lengths,max_output);
    }
    if(instance->role!=0)return 0;

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
        int observed_count=hb_observed_notes(instance,observed_notes,64);
        int sense_changed=(observed_count!=g_bus.last_sense_count);
        if(!sense_changed){
            for(int i=0;i<observed_count;i++){
                if(g_bus.last_sense_notes[i]!=observed_notes[i]){sense_changed=1;break;}
            }
        }
        if(sense_changed){
            g_bus.last_sense_count=observed_count;
            for(int i=0;i<observed_count;i++)g_bus.last_sense_notes[i]=observed_notes[i];
            g_bus.sense_rev++;
            memset(instance->active,0,sizeof(instance->active));
            for(int i=0;i<observed_count;i++)instance->active[observed_notes[i]]=1;
            instance->candidate_frames=0;
            instance->dirty=1;
            instance->frames_since_change=0;
        }
    }

    int expired_release=0;
    for(int note=0;note<128;note++){
        if(instance->pending_off_frames[note]<=0)continue;
        instance->pending_off_frames[note]-=frames;
        if(instance->pending_off_frames[note]<=0){
            instance->pending_off_frames[note]=0;
            if(instance->active[note]){
                instance->active[note]=0;
                expired_release=1;
            }
        }
    }
    if(expired_release){
        instance->candidate_frames=0;
        instance->dirty=1;
        instance->frames_since_change=0;
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
    int needed=(instance->window_ms*sample_rate)/1000;
    if(instance->frames_since_change<needed)return 0;

    uint8_t notes[128];
    int count=active_notes(instance,notes);
    instance->last_inferred_count=count;
    instance->dirty=0;

    if(count<=0)return 0;

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
    if(g_bus.chord_timing==0 && count>=3){
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
static int enum_index(const char *value,const char *const *options,int count,int fallback){int parsed=parse_i(value,-999);if(parsed>=0&&parsed<count)return parsed;if(value)for(int index=0;index<count;index++)if(!strcmp(value,options[index]))return index;return fallback;}
static const char *ROLE_OPTS[]={"Conductor","Follower","Off"};static const char *RETRIGGER_OPTS[]={"Off","On"};static const char *CLIP_SLOT_OPTS[]={"Auto","1","2","3","4","5","6","7","8"};static const char *SENSOR_SOURCE_OPTS[]={"Realtime","Realtime + Clip","Clip"};static const char *CLIP_CONTEXT_OPTS[]={"Clip + Realtime","Realtime Only"};static const char *SOURCE_CH_OPTS[]={"Auto","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"};static const char *RENDER_CH_OPTS[]={"Off","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"};static const char *MODE_OPTS[]={"Relative","Smooth","Nearest"};static const char *POLICY_OPTS[]={"Explicit","Current Input Root","Auto-Infer"};static const char *MAP_TARGET_OPTS[]={"Chord","Scale"};static const char *TIMING_OPTS[]={"Free","1/16","1/16 Ant","1/8","1/8 Ant","1/4","1/4 Ant","1/2","1/2 Ant","1 Bar","1 Bar Ant","2 Bars","2 Bars Ant"};static const char *CONTEXT_OPTS[]={"Live","1/32","1/16","1/8","1/4","1/2","1 Bar"};static const char *TIMESCALE_OPTS[]={"Free","1/16","1/8","1/4","1/2","1 Bar"};static const char *STABILITY_OPTS[]={"Responsive","Balanced","Stable"};static const char *ACCIDENTAL_OPTS[]={"Auto","Sharps","C#D#F#G#Bb","C#EbF#G#Bb","C#EbF#AbBb","DbEbF#AbBb","Flats"};static const char *PC_OPTS[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
static int hb_timing_to_legacy_timescale(int timing){
    if(timing<=0)return 0;
    if(timing<=2)return 1;  /* 1/16 */
    if(timing<=4)return 2;  /* 1/8 */
    if(timing<=6)return 3;  /* 1/4 */
    if(timing<=8)return 4;  /* 1/2 */
    return 5;               /* 1 bar or longer in realtime fallback */
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
"{\\\"key\\\":\\\"transpose\\\",\\\"name\\\":\\\"Transpose\\\",\\\"type\\\":\\\"int\\\",\\\"min\\\":-24,\\\"max\\\":24,\\\"step\\\":1},"
"{\\\"key\\\":\\\"window_ms\\\",\\\"name\\\":\\\"Inference Window\\\",\\\"type\\\":\\\"int\\\",\\\"min\\\":10,\\\"max\\\":500,\\\"step\\\":5},"
"{\\\"key\\\":\\\"harmony\\\",\\\"name\\\":\\\"Harmony\\\",\\\"type\\\":\\\"string\\\",\\\"access\\\":\\\"read\\\"},"
"{\\\"key\\\":\\\"detected_root\\\",\\\"name\\\":\\\"Root\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"C\\\",\\\"C#\\\",\\\"D\\\",\\\"Eb\\\",\\\"E\\\",\\\"F\\\",\\\"F#\\\",\\\"G\\\",\\\"Ab\\\",\\\"A\\\",\\\"Bb\\\",\\\"B\\\"],\\\"options_as_string\\\":true,\\\"access\\\":\\\"read\\\"},"
"{\\\"key\\\":\\\"detected_bass\\\",\\\"name\\\":\\\"Bass\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"C\\\",\\\"C#\\\",\\\"D\\\",\\\"Eb\\\",\\\"E\\\",\\\"F\\\",\\\"F#\\\",\\\"G\\\",\\\"Ab\\\",\\\"A\\\",\\\"Bb\\\",\\\"B\\\"],\\\"options_as_string\\\":true,\\\"access\\\":\\\"read\\\"},"
"{\\\"key\\\":\\\"confidence\\\",\\\"name\\\":\\\"Confidence\\\",\\\"type\\\":\\\"int\\\",\\\"min\\\":0,\\\"max\\\":100,\\\"access\\\":\\\"read\\\"}"
"]";
static void set_param(void *value,const char *key,const char *parameter){Inst *instance=(Inst*)value;if(!instance||!key||!parameter)return;if(!strcmp(key,"live_press")){if(parameter[0]=='1')hb_receive_live_vouch(instance);return;}if(!strcmp(key,"role")){instance->role=enum_index(parameter,ROLE_OPTS,3,instance->role);instance->resolved_source_channel=-1;if(instance->role==0){if(!hb_load_clip_cache())hb_clear_clip_cache();}}else if(!strcmp(key,"clip_slot")){g_bus.clip_slot=enum_index(parameter,CLIP_SLOT_OPTS,9,g_bus.clip_slot);if(instance->role==0){if(!hb_load_clip_cache())hb_clear_clip_cache();}}else if(!strcmp(key,"sensor_sources")){g_bus.sensor_sources=0;instance->dirty=1;instance->frames_since_change=0;}else if(!strcmp(key,"clip_context")){g_bus.clip_context=0;g_bus.sensor_sources=0;hb_clear_clip_cache();instance->dirty=1;instance->frames_since_change=0;}else if(!strcmp(key,"retrigger_held")){instance->retrigger_held=enum_index(parameter,RETRIGGER_OPTS,2,instance->retrigger_held);}else if(!strcmp(key,"mode")){instance->mode=enum_index(parameter,MODE_OPTS,3,instance->mode);instance->follower_bus_seq=0;}else if(!strcmp(key,"map_target"))instance->map_target=enum_index(parameter,MAP_TARGET_OPTS,2,instance->map_target);else if(!strcmp(key,"source_channel")){int idx=enum_index(parameter,SOURCE_CH_OPTS,17,instance->source_channel+1);instance->source_channel=idx-1;instance->resolved_source_channel=-1;}else if(!strcmp(key,"render_channel")){int idx=enum_index(parameter,RENDER_CH_OPTS,17,instance->render_channel+1);instance->render_channel=idx-1;}else if(!strcmp(key,"chord_timing")){g_bus.chord_timing=enum_index(parameter,TIMING_OPTS,13,g_bus.chord_timing);g_bus.chord_timescale=hb_timing_to_legacy_timescale(g_bus.chord_timing);}else if(!strcmp(key,"context")){g_bus.context=enum_index(parameter,CONTEXT_OPTS,7,g_bus.context);g_bus.stability=hb_context_to_legacy_stability(g_bus.context);}else if(!strcmp(key,"chord_timescale"))g_bus.chord_timescale=enum_index(parameter,TIMESCALE_OPTS,6,g_bus.chord_timescale);else if(!strcmp(key,"stability"))g_bus.stability=enum_index(parameter,STABILITY_OPTS,3,g_bus.stability);else if(!strcmp(key,"accidentals")){int previous=g_bus.accidentals;g_bus.accidentals=enum_index(parameter,ACCIDENTAL_OPTS,7,g_bus.accidentals);if(g_bus.accidentals==0&&previous!=0)g_bus.auto_spell_locked=0;}else if(!strcmp(key,"root_policy"))g_bus.global_root_policy=enum_index(parameter,POLICY_OPTS,3,g_bus.global_root_policy);else if(!strcmp(key,"explicit_root"))g_bus.global_explicit_root=enum_index(parameter,PC_OPTS,12,g_bus.global_explicit_root);else if(!strcmp(key,"input_root"))g_bus.global_input_root=enum_index(parameter,PC_OPTS,12,g_bus.global_input_root);else if(!strcmp(key,"transpose")){int parsed=parse_i(parameter,g_bus.global_transpose);if(parsed<-24)parsed=-24;if(parsed>24)parsed=24;g_bus.global_transpose=parsed;}else if(!strcmp(key,"window_ms")){int parsed=parse_i(parameter,instance->window_ms);if(parsed<0)parsed=0;if(parsed>500)parsed=500;instance->window_ms=parsed;}else if(!strcmp(key,"state"))hb_restore_state(instance,parameter);}
static void hb_restore_state(Inst *instance,const char *state){
    if(!instance||!state)return;
    int values[16];for(int i=0;i<16;i++)values[i]=-999;
    int parsed=sscanf(state,"hb7,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
        &values[12],&values[13],&values[14],&values[15]);
    if(parsed!=16){
        parsed=sscanf(state,"hb6,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14]);
    }
    if(parsed!=15&&parsed!=16){
        parsed=sscanf(state,"hb5,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14],&values[15]);
    }
    if(parsed!=16&&parsed!=15){
        parsed=sscanf(state,"hb4,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],
            &values[12],&values[13],&values[14]);
    }
    if(parsed!=15&&parsed!=16){
        parsed=sscanf(state,"hb3,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11],&values[12]);
    }
    if(parsed!=13&&parsed!=15&&parsed!=16){
        parsed=sscanf(state,"hb2,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10],&values[11]);
    }
    if(parsed!=12&&parsed!=13&&parsed!=15&&parsed!=16){
        parsed=sscanf(state,"hb1,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
            &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
            &values[6],&values[7],&values[8],&values[9],&values[10]);
        if(parsed!=11)return;
    }
    if(values[0]>=0&&values[0]<=2)instance->role=values[0];
    if(values[1]>=0&&values[1]<=2)instance->mode=values[1];
    instance->map_target=0;
    if(values[3]>=0&&values[3]<=500)instance->window_ms=values[3];
    if(values[0]==0){
        if(values[4]>=0&&values[4]<=2)g_bus.global_root_policy=values[4];
        if(values[5]>=0&&values[5]<12)g_bus.global_explicit_root=values[5];
        if(values[6]>=0&&values[6]<12)g_bus.global_input_root=values[6];
        if(values[7]>=-24&&values[7]<=24)g_bus.global_transpose=values[7];
        if(values[8]>=0&&values[8]<=5)g_bus.chord_timescale=values[8];
        if(values[9]>=0&&values[9]<=2)g_bus.stability=values[9];
        if(values[10]>=0&&values[10]<=6)g_bus.accidentals=values[10];
        if(parsed>=15){
            if(values[13]>=0&&values[13]<13)g_bus.chord_timing=values[13];
            if(values[14]>=0&&values[14]<7)g_bus.context=values[14];
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
    if(parsed>=12&&values[11]>=-1&&values[11]<16)instance->render_channel=values[11];
    if((parsed==13||parsed>=15)&&values[12]>=-1&&values[12]<16)instance->source_channel=values[12];
    if(instance->role==0)hb_clear_clip_cache();
}
static int get_param(void *value,const char *key,char *buffer,int length){Inst *instance=(Inst*)value;if(!instance||!key||!buffer||length<2)return -1;hb_harmony_t harmony=bus_read();if(!strcmp(key,"version"))return snprintf(buffer,(size_t)length,"%s",HB_VERSION);if(!strcmp(key,"retrigger_held"))return snprintf(buffer,(size_t)length,"%s",RETRIGGER_OPTS[instance->retrigger_held?1:0]);if(!strcmp(key,"follower_note_1_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,0),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_note_2_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,1),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_note_3_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,2),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_note_4_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_follower_analysis_note(instance,3),bus_read(),note_buf,sizeof(note_buf)));}
if(!strcmp(key,"follower_role_1"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,0));
if(!strcmp(key,"follower_role_2"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,1));
if(!strcmp(key,"follower_role_3"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,2));
if(!strcmp(key,"follower_role_4"))return snprintf(buffer,(size_t)length,"%s",hb_follower_analysis_role_name(instance,3));
if(!strcmp(key,"follower_active_count"))return snprintf(buffer,(size_t)length,"%d",hb_follower_analysis_count(instance));if(!strcmp(key,"state"))return snprintf(buffer,(size_t)length,"hb7,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",instance->role,instance->mode,instance->map_target,instance->window_ms,g_bus.global_root_policy,g_bus.global_explicit_root,g_bus.global_input_root,g_bus.global_transpose,g_bus.chord_timescale,g_bus.stability,g_bus.accidentals,instance->render_channel,instance->source_channel,g_bus.chord_timing,g_bus.context,g_bus.clip_context);if(!strcmp(key,"role"))return snprintf(buffer,(size_t)length,"%s",ROLE_OPTS[instance->role]);if(!strcmp(key,"clip_slot"))return snprintf(buffer,(size_t)length,"%s",CLIP_SLOT_OPTS[g_bus.clip_slot]);if(!strcmp(key,"sensor_sources"))return snprintf(buffer,(size_t)length,"%s","Realtime");if(!strcmp(key,"realtime_route"))return snprintf(buffer,(size_t)length,"%s","Set Track MIDI Out -> Schwung");if(!strcmp(key,"clip_context"))return snprintf(buffer,(size_t)length,"%s","Realtime Only");if(!strcmp(key,"mode"))return snprintf(buffer,(size_t)length,"%s",MODE_OPTS[instance->mode]);if(!strcmp(key,"map_target"))return snprintf(buffer,(size_t)length,"%s",MAP_TARGET_OPTS[instance->map_target]);if(!strcmp(key,"source_channel"))return snprintf(buffer,(size_t)length,"%s",SOURCE_CH_OPTS[instance->source_channel+1]);if(!strcmp(key,"resolved_source_channel"))return snprintf(buffer,(size_t)length,"%d",instance->resolved_source_channel);if(!strcmp(key,"render_channel"))return snprintf(buffer,(size_t)length,"%s",RENDER_CH_OPTS[instance->render_channel+1]);if(!strcmp(key,"chord_timing"))return snprintf(buffer,(size_t)length,"%s",TIMING_OPTS[g_bus.chord_timing]);if(!strcmp(key,"context"))return snprintf(buffer,(size_t)length,"%s",CONTEXT_OPTS[g_bus.context]);if(!strcmp(key,"chord_timescale"))return snprintf(buffer,(size_t)length,"%s",TIMESCALE_OPTS[g_bus.chord_timescale]);if(!strcmp(key,"stability"))return snprintf(buffer,(size_t)length,"%s",STABILITY_OPTS[g_bus.stability]);if(!strcmp(key,"accidentals"))return snprintf(buffer,(size_t)length,"%s",ACCIDENTAL_OPTS[g_bus.accidentals]);if(!strcmp(key,"root_policy"))return snprintf(buffer,(size_t)length,"%s",POLICY_OPTS[g_bus.global_root_policy]);if(!strcmp(key,"explicit_root"))return snprintf(buffer,(size_t)length,"%s",PC_OPTS[g_bus.global_explicit_root]);if(!strcmp(key,"input_root"))return snprintf(buffer,(size_t)length,"%s",PC_OPTS[g_bus.global_input_root]);if(!strcmp(key,"transpose"))return snprintf(buffer,(size_t)length,"%d",g_bus.global_transpose);if(!strcmp(key,"window_ms"))return snprintf(buffer,(size_t)length,"%d",instance->window_ms);if(!strcmp(key,"detected_root"))return snprintf(buffer,(size_t)length,"%s",harmony.valid?hb_pc_display(harmony.root_pc,harmony):"--");if(!strcmp(key,"detected_bass"))return snprintf(buffer,(size_t)length,"%s",harmony.valid?hb_pc_display(harmony.bass_pc,harmony):"--");if(!strcmp(key,"detected_quality"))return snprintf(buffer,(size_t)length,"%d",harmony.valid?harmony.chord_index+1:0);if(!strcmp(key,"confidence"))return snprintf(buffer,(size_t)length,"%d",harmony.valid?harmony.confidence:0);if(!strcmp(key,"resolved_root"))return snprintf(buffer,(size_t)length,"%d",reference_root(instance));if(!strcmp(key,"harmony"))return hb_format_harmony(buffer,length,harmony);if(!strcmp(key,"candidate_harmony"))return hb_format_harmony(buffer,length,instance->candidate_harmony);if(!strcmp(key,"pitch_mask"))return snprintf(buffer,(size_t)length,"%u",(unsigned)harmony.pitch_mask);if(!strcmp(key,"rx_count"))return snprintf(buffer,(size_t)length,"%u",instance->rx_count);if(!strcmp(key,"note_on_count"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"note_off_count"))return snprintf(buffer,(size_t)length,"%u",instance->note_off_count);if(!strcmp(key,"last_note"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"last_status"))return snprintf(buffer,(size_t)length,"%d",instance->last_status);if(!strcmp(key,"last_velocity"))return snprintf(buffer,(size_t)length,"%d",instance->last_velocity);if(!strcmp(key,"diag_midi_events"))return snprintf(buffer,(size_t)length,"%u",instance->rx_count);if(!strcmp(key,"diag_note_ons"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"diag_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"diag_recv_ch")){int recv=(g_host&&g_host->slot_recv_channel)?g_host->slot_recv_channel(instance):-1;return snprintf(buffer,(size_t)length,"%d",(recv>=0&&recv<16)?recv+1:-1);}if(!strcmp(key,"diag_event_ch")){int type=instance->last_status&0xF0;int event_ch=(type==0x80||type==0x90)?(instance->last_status&15):-1;return snprintf(buffer,(size_t)length,"%d",event_ch>=0?event_ch+1:-1);}if(!strcmp(key,"live_press_count"))return snprintf(buffer,(size_t)length,"%u",instance->live_press_count);if(!strcmp(key,"rt_note_count"))return snprintf(buffer,(size_t)length,"%d",hb_held_count(instance));if(!strcmp(key,"rt_note_ons"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"rt_note_offs"))return snprintf(buffer,(size_t)length,"%u",instance->note_off_count);if(!strcmp(key,"rt_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"rt_last_channel")){int t=instance->last_status&0xF0;return snprintf(buffer,(size_t)length,"%d",(t==0x80||t==0x90)?(instance->last_status&15):-1);}if(!strcmp(key,"clip_active_count")){uint8_t temp_notes[32];int temp_count=hb_clip_active_notes(temp_notes,32);return snprintf(buffer,(size_t)length,"%d",temp_count);}if(!strcmp(key,"active_count"))return snprintf(buffer,(size_t)length,"%d",hb_conductor_analysis_count(instance));if(!strcmp(key,"active_note_list"))return hb_format_active_notes(instance,buffer,length);if(!strcmp(key,"callback_active_notes"))return snprintf(buffer,(size_t)length,"%d",active_notes(instance,(uint8_t[128]){0}));if(!strcmp(key,"pending_releases")){int n=0;for(int i=0;i<128;i++)if(instance->pending_off_frames[i]>0)n++;return snprintf(buffer,(size_t)length,"%d",n);}if(!strcmp(key,"active_pc_mask"))return snprintf(buffer,(size_t)length,"%u",hb_active_pc_mask(instance));if(!strcmp(key,"active_note_1"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,0));if(!strcmp(key,"active_note_2"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,1));if(!strcmp(key,"active_note_3"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,2));if(!strcmp(key,"active_note_4"))return snprintf(buffer,(size_t)length,"%d",hb_nth_active_note(instance,3));if(!strcmp(key,"active_note_4_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,3),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"active_note_3_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,2),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"active_note_2_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,1),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"active_note_1_name")){char note_buf[8];return snprintf(buffer,(size_t)length,"%s",hb_note_name_with_octave(hb_nth_conductor_analysis_note(instance,0),harmony,note_buf,sizeof(note_buf)));}if(!strcmp(key,"clip_stage")){static const char *names[]={"idle","no active set","active set","no conductor","conductor found","no Song.abl","Song.abl found","no track","track found","no clip slots","no clip","clip found","no notes array","empty clip","OK"};int stage=g_bus.clip_stage;if(stage<0||stage>14)stage=0;return snprintf(buffer,(size_t)length,"%s",names[stage]);}if(!strcmp(key,"cache_rev"))return snprintf(buffer,(size_t)length,"%u",g_bus.cache_rev);if(!strcmp(key,"sense_rev"))return snprintf(buffer,(size_t)length,"%u",g_bus.sense_rev);if(!strcmp(key,"clip_track"))return snprintf(buffer,(size_t)length,"%d",g_bus.clip_context?g_bus.clip_track:-1);if(!strcmp(key,"clip_note_count"))return snprintf(buffer,(size_t)length,"%d",g_bus.clip_context?g_bus.clip_note_count:0);if(!strcmp(key,"clip_playhead"))return g_bus.clip_context?snprintf(buffer,(size_t)length,"%.3f",hb_clip_playhead()):snprintf(buffer,(size_t)length,"%s","realtime");if(!strcmp(key,"infer_note_count"))return snprintf(buffer,(size_t)length,"%d",instance->last_inferred_count);if(!strcmp(key,"bus_seq"))return snprintf(buffer,(size_t)length,"%u",__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE));if(!strcmp(key,"raw_event_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_event_count);if(!strcmp(key,"raw_note_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_count);if(!strcmp(key,"raw_note_on_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_on_count);if(!strcmp(key,"raw_note_off_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_off_count);if(!strcmp(key,"raw_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_note);if(!strcmp(key,"raw_last_status"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_status);if(!strcmp(key,"raw_last_velocity"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_velocity);if(!strcmp(key,"raw_last_channel"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_channel);if(!strcmp(key,"raw_track_match"))return snprintf(buffer,(size_t)length,"%d",(g_bus.clip_track>=0&&instance->raw_last_channel==g_bus.clip_track)?1:0);if(!strcmp(key,"raw_last_cable"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_cable);if(!strcmp(key,"inject_available"))return snprintf(buffer,(size_t)length,"%d",(g_host&&g_host->midi_inject_to_move)?1:0);if(!strcmp(key,"render_count"))return snprintf(buffer,(size_t)length,"%u",instance->render_count);if(!strcmp(key,"render_fail_count"))return snprintf(buffer,(size_t)length,"%u",instance->render_fail_count);if(!strcmp(key,"render_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->render_last_note);if(!strcmp(key,"follower_last_in"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"follower_last_out"))return snprintf(buffer,(size_t)length,"%d",instance->render_channel>=0?instance->render_last_note:(instance->last_note>=0&&instance->last_note<128?instance->mapped[instance->last_note]:-1));if(!strcmp(key,"chain_params")){int size=(int)strlen(CHAIN_PARAMS);if(size>=length)return -1;memcpy(buffer,CHAIN_PARAMS,(size_t)size+1);return size;}return -1;}
static midi_fx_api_v1_t API={MIDI_FX_API_VERSION,create_inst,destroy_inst,process,tick,set_param,get_param};
midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host){g_host=host;ensure_init();return &API;}
