/* Harmony Bus v0.1 — Schwung MIDI FX. */
#ifdef HB_FREESTANDING
typedef __SIZE_TYPE__ size_t;
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
extern int snprintf(char *, size_t, const char *, ...);
extern int sscanf(const char *, const char *, ...);
extern void *memset(void *, int, size_t);
extern void *memcpy(void *, const void *, size_t);
extern size_t strlen(const char *);
extern int strcmp(const char *, const char *);
extern long strtol(const char *, char **, int);
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
};
static const host_api_v1_t *g_host = 0;
#define HB_MIDI_OUT_BYTES 80

#define HB_MAX_INSTANCES 16
typedef struct { volatile unsigned seq; hb_harmony_t harmony; int global_transpose; int global_root_policy; int global_explicit_root; int global_input_root; int chord_timescale; int stability; int accidentals; int auto_spell_sharps; int auto_spell_locked; } SharedBus;
static SharedBus g_bus={0}; static int g_init=0;
typedef struct { int used,role,mode,window_ms,dirty,frames_since_change; uint8_t active[128]; int mapped[128]; uint8_t source_seen[12]; int resolved_root,resolved_confidence; unsigned rx_count; unsigned note_on_count; unsigned note_off_count; int last_note; int last_status; int last_velocity; int active_count; int last_inferred_count; unsigned raw_event_count; unsigned raw_note_count; unsigned raw_note_on_count; unsigned raw_note_off_count; int raw_last_note; int raw_last_status; int raw_last_velocity; int raw_last_channel; int raw_last_cable; uint8_t raw_prev[HB_MIDI_OUT_BYTES]; int map_target; hb_harmony_t candidate_harmony; int candidate_frames; int committed_frames; int render_channel; unsigned render_count; unsigned render_fail_count; int render_last_note; } Inst;
static hb_harmony_t hb_mapping_target(hb_harmony_t harmony,int map_target);
static Inst g_pool[HB_MAX_INSTANCES];
static int mod12(int value){value%=12;return value<0?value+12:value;}
static int parse_i(const char *value,int fallback){char *end;long parsed;if(!value||!*value)return fallback;end=0;parsed=strtol(value,&end,10);return end==value?fallback:(int)parsed;}
static void ensure_init(void){if(g_init)return;memset(&g_bus,0,sizeof(g_bus));g_bus.global_root_policy=2;g_bus.chord_timescale=3;g_bus.stability=1;g_bus.accidentals=0;g_bus.auto_spell_sharps=1;g_bus.auto_spell_locked=0;for(int index=0;index<HB_MAX_INSTANCES;index++){memset(&g_pool[index],0,sizeof(g_pool[index]));for(int note=0;note<128;note++)g_pool[index].mapped[note]=-1;}g_init=1;}
static void bus_write(hb_harmony_t harmony){unsigned sequence=__atomic_load_n(&g_bus.seq,__ATOMIC_RELAXED);__atomic_store_n(&g_bus.seq,sequence+1,__ATOMIC_RELEASE);g_bus.harmony=harmony;__atomic_store_n(&g_bus.seq,sequence+2,__ATOMIC_RELEASE);}
static hb_harmony_t bus_read(void){hb_harmony_t harmony;memset(&harmony,0,sizeof(harmony));for(int tries=0;tries<3;tries++){unsigned before=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE),after;if(before&1u)continue;harmony=g_bus.harmony;after=__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE);if(before==after&&!(after&1u))return harmony;}return harmony;}

static void hb_render_follower_event(Inst *instance,int source_note,int velocity,int is_on,int is_off,int recv_channel){
    if(!instance||instance->role!=1||instance->render_channel<0||!g_host||!g_host->midi_inject_to_move)return;
    if(instance->render_channel==recv_channel)return; /* avoid self-echo loops */
    int mapped=source_note;
    if(is_on){
        hb_harmony_t harmony=hb_mapping_target(bus_read(),instance->map_target);
        int render_root=g_bus.global_root_policy==0?g_bus.global_explicit_root:(g_bus.global_root_policy==1?g_bus.global_input_root:source_note%12);
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

        if (instance->role == 0) {
            if (is_on) instance->active[data1] = 1;
            else instance->active[data1] = 0;
            instance->candidate_frames = 0;
            instance->dirty = 1;
            instance->frames_since_change = 0;
        }
    }
}


static int hb_same_harmony(hb_harmony_t left,hb_harmony_t right){
    return left.valid&&right.valid&&left.root_pc==right.root_pc&&left.chord_index==right.chord_index;
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
    if(!harmony.valid||map_target==0)return harmony;
    harmony.pitch_mask=hb_scale_mask(harmony);return harmony;
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
    static const char *suffix[]={"","min","5","sus2","sus4","dim","aug","6","min6","maj7","7","min7","minMaj7","min7b5","dim7","add9","minAdd9","maj9","9","min9","11","min11","13"};
    return (chord_index>=0&&chord_index<23)?suffix[chord_index]:"";
}
static int hb_format_harmony(char *buffer,int length,hb_harmony_t harmony){
    if(!harmony.valid)return snprintf(buffer,(size_t)length,"--");
    if(harmony.bass_pc!=harmony.root_pc)return snprintf(buffer,(size_t)length,"%s%s/%s",hb_pc_display(harmony.root_pc,harmony),hb_quality_suffix(harmony.chord_index),hb_pc_display(harmony.bass_pc,harmony));
    return snprintf(buffer,(size_t)length,"%s%s",hb_pc_display(harmony.root_pc,harmony),hb_quality_suffix(harmony.chord_index));
}

static int active_notes(const Inst *instance,uint8_t *output){int count=0;for(int note=0;note<128;note++)if(instance->active[note])output[count++]=(uint8_t)note;return count;}
static int infer_reference_root(Inst *instance){static const int major[7]={0,2,4,5,7,9,11};static const int minor[7]={0,2,3,5,7,8,10};int pitch_classes=0,best=-999,best_root=instance->resolved_root;for(int index=0;index<12;index++)pitch_classes+=instance->source_seen[index]?1:0;if(!pitch_classes)return instance->resolved_root;for(int root=0;root<12;root++)for(int scale=0;scale<2;scale++){int score=0;for(int pitch_class=0;pitch_class<12;pitch_class++)if(instance->source_seen[pitch_class]){int relative=mod12(pitch_class-root),inside=0;for(int degree=0;degree<7;degree++)if(relative==(scale?minor[degree]:major[degree])){inside=1;break;}score+=inside?5:-4;}if(instance->source_seen[root])score+=3;if(score>best){best=score;best_root=root;}}instance->resolved_confidence=pitch_classes>=4?80:(pitch_classes>=3?65:45);return best_root;}
static int reference_root(Inst *instance){if(g_bus.global_root_policy==0)return mod12(g_bus.global_explicit_root);if(g_bus.global_root_policy==1)return mod12(g_bus.global_input_root);instance->resolved_root=infer_reference_root(instance);return mod12(instance->resolved_root);}
static void *create_inst(const char *module_dir,const char *config_json){(void)module_dir;(void)config_json;ensure_init();for(int index=0;index<HB_MAX_INSTANCES;index++)if(!g_pool[index].used){Inst *instance=&g_pool[index];memset(instance,0,sizeof(*instance));instance->used=1;instance->role=1;instance->mode=HB_MAP_CHORD;instance->map_target=0;instance->window_ms=70;instance->last_note=-1;instance->last_status=-1;instance->last_velocity=-1;instance->raw_last_note=-1;instance->raw_last_status=-1;instance->raw_last_velocity=-1;instance->raw_last_channel=-1;instance->raw_last_cable=-1;instance->render_channel=-1;instance->render_last_note=-1;for(int note=0;note<128;note++)instance->mapped[note]=-1;return instance;}return 0;}
static void destroy_inst(void *value){Inst *instance=(Inst*)value;if(instance)instance->used=0;}
static int pass(const uint8_t *input,int length,uint8_t output[][3],int lengths[],int max_output){if(!input||length<1||length>3||max_output<1)return 0;memcpy(output[0],input,(size_t)length);lengths[0]=length;return 1;}
static int process(void *value,const uint8_t *input,int length,uint8_t output[][3],int lengths[],int max_output){Inst *instance=(Inst*)value;if(!instance||!input||length<1)return 0;instance->rx_count++;instance->last_status=input[0];if(length>=2)instance->last_note=input[1]&0x7F;if(length>=3)instance->last_velocity=input[2];int status=input[0]&0xF0,is_on=(status==0x90&&length>=3&&input[2]>0),is_off=(status==0x80&&length>=3)||(status==0x90&&length>=3&&input[2]==0);if(!(is_on||is_off))return pass(input,length,output,lengths,max_output);int note=input[1]&0x7F,mapped;if(is_on){instance->note_on_count++;instance->active_count++;}else if(is_off){instance->note_off_count++;if(instance->active_count>0)instance->active_count--;}if(instance->role==2)return pass(input,length,output,lengths,max_output);if(instance->role==0){if(is_on){instance->active[note]=1;mapped=note+g_bus.global_transpose;if(mapped<0)mapped=0;if(mapped>127)mapped=127;instance->mapped[note]=mapped;}else{instance->active[note]=0;mapped=instance->mapped[note];if(mapped<0)mapped=note+g_bus.global_transpose;if(mapped<0)mapped=0;if(mapped>127)mapped=127;instance->mapped[note]=-1;}instance->candidate_frames=0;instance->dirty=1;instance->frames_since_change=0;if(max_output<1)return 0;output[0][0]=input[0];output[0][1]=(uint8_t)mapped;output[0][2]=length>=3?input[2]:0;lengths[0]=3;return 1;}if(is_on){instance->source_seen[note%12]=1;hb_harmony_t harmony=hb_mapping_target(bus_read(),instance->map_target);mapped=hb_map_note(note,reference_root(instance),harmony,(hb_map_mode_t)instance->mode);instance->mapped[note]=mapped;}else{mapped=instance->mapped[note];if(mapped<0)mapped=note;instance->mapped[note]=-1;}if(max_output<1)return 0;output[0][0]=input[0];output[0][1]=(uint8_t)mapped;output[0][2]=length>=3?input[2]:0;lengths[0]=3;return 1;}
static int tick(void *value,int frames,int sample_rate,uint8_t output[][3],int lengths[],int max_output){
    (void)output;(void)lengths;(void)max_output;
    Inst *instance=(Inst*)value;
    if(!instance)return 0;
    hb_scan_raw_midi_out(instance);
    if(instance->role!=0)return 0;

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

    hb_harmony_t candidate=hb_infer_harmony(notes,count);
    candidate=hb_transpose_harmony(candidate,g_bus.global_transpose);
    hb_harmony_t committed=bus_read();
    uint16_t current_mask=hb_active_mask(instance);
    int compatible_subset=committed.valid&&((current_mask&~committed.pitch_mask)==0);

    if(!committed.valid){
        if(candidate.valid){
            bus_write(candidate);
            instance->committed_frames=0;
            instance->candidate_frames=0;
            instance->candidate_harmony=candidate;
        }
        return 0;
    }

    if(hb_same_harmony(candidate,committed)){
        instance->candidate_frames=0;
        instance->candidate_harmony=candidate;
        return 0;
    }

    /* Note releases are weak evidence. If what remains still fits inside the
     * committed chord, hold it rather than reclassifying a partial voicing. */
    if(compatible_subset&&count<3){
        instance->candidate_frames=0;
        return 0;
    }

    if(candidate.valid){
        int min_dwell=hb_timescale_frames(sample_rate);
        int overwhelming=(candidate.confidence>=90&&count>=3);
        instance->candidate_harmony=candidate;
        if(overwhelming && instance->committed_frames>=min_dwell/2){
            bus_write(candidate);
            instance->committed_frames=0;
            instance->candidate_frames=0;
        }else{
            /* Confirmation continues on subsequent audio ticks while the
             * observed note-set remains unchanged. */
            instance->candidate_frames=frames;
        }
    }
    return 0;
}

static void hb_restore_state(Inst *instance,const char *state);
static int enum_index(const char *value,const char *const *options,int count,int fallback){int parsed=parse_i(value,-999);if(parsed>=0&&parsed<count)return parsed;if(value)for(int index=0;index<count;index++)if(!strcmp(value,options[index]))return index;return fallback;}
static const char *ROLE_OPTS[]={"Conductor","Follower","Off"};static const char *RENDER_CH_OPTS[]={"Off","1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"};static const char *MODE_OPTS[]={"Transpose","Chord","Nearest"};static const char *POLICY_OPTS[]={"Explicit","Current Input Root","Auto-Infer"};static const char *MAP_TARGET_OPTS[]={"Chord","Scale"};static const char *TIMESCALE_OPTS[]={"Free","1/16","1/8","1/4","1/2","1 Bar"};static const char *STABILITY_OPTS[]={"Responsive","Balanced","Stable"};static const char *ACCIDENTAL_OPTS[]={"Auto","Sharps","C#D#F#G#Bb","C#EbF#G#Bb","C#EbF#AbBb","DbEbF#AbBb","Flats"};static const char *PC_OPTS[]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
static const char CHAIN_PARAMS[]="["
"{\\\"key\\\":\\\"role\\\",\\\"name\\\":\\\"Role\\\",\\\"type\\\":\\\"enum\\\",\\\"options\\\":[\\\"Conductor\\\",\\\"Follower\\\",\\\"Off\\\"],\\\"options_as_string\\\":true},"
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
static void set_param(void *value,const char *key,const char *parameter){Inst *instance=(Inst*)value;if(!instance||!key||!parameter)return;if(!strcmp(key,"role"))instance->role=enum_index(parameter,ROLE_OPTS,3,instance->role);else if(!strcmp(key,"mode"))instance->mode=enum_index(parameter,MODE_OPTS,3,instance->mode);else if(!strcmp(key,"map_target"))instance->map_target=enum_index(parameter,MAP_TARGET_OPTS,2,instance->map_target);else if(!strcmp(key,"render_channel")){int idx=enum_index(parameter,RENDER_CH_OPTS,17,instance->render_channel+1);instance->render_channel=idx-1;}else if(!strcmp(key,"chord_timescale"))g_bus.chord_timescale=enum_index(parameter,TIMESCALE_OPTS,6,g_bus.chord_timescale);else if(!strcmp(key,"stability"))g_bus.stability=enum_index(parameter,STABILITY_OPTS,3,g_bus.stability);else if(!strcmp(key,"accidentals")){int previous=g_bus.accidentals;g_bus.accidentals=enum_index(parameter,ACCIDENTAL_OPTS,7,g_bus.accidentals);if(g_bus.accidentals==0&&previous!=0)g_bus.auto_spell_locked=0;}else if(!strcmp(key,"root_policy"))g_bus.global_root_policy=enum_index(parameter,POLICY_OPTS,3,g_bus.global_root_policy);else if(!strcmp(key,"explicit_root"))g_bus.global_explicit_root=enum_index(parameter,PC_OPTS,12,g_bus.global_explicit_root);else if(!strcmp(key,"input_root"))g_bus.global_input_root=enum_index(parameter,PC_OPTS,12,g_bus.global_input_root);else if(!strcmp(key,"transpose")){int parsed=parse_i(parameter,g_bus.global_transpose);if(parsed<-24)parsed=-24;if(parsed>24)parsed=24;g_bus.global_transpose=parsed;}else if(!strcmp(key,"window_ms")){int parsed=parse_i(parameter,instance->window_ms);if(parsed<10)parsed=10;if(parsed>500)parsed=500;instance->window_ms=parsed;}else if(!strcmp(key,"state"))hb_restore_state(instance,parameter);}
static void hb_restore_state(Inst *instance,const char *state){
    if(!instance||!state)return;
    int values[11];for(int i=0;i<11;i++)values[i]=-999;
    if(sscanf(state,"hb1,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
        &values[0],&values[1],&values[2],&values[3],&values[4],&values[5],
        &values[6],&values[7],&values[8],&values[9],&values[10])!=11)return;
    if(values[0]>=0&&values[0]<=2)instance->role=values[0];
    if(values[1]>=0&&values[1]<=2)instance->mode=values[1];
    if(values[2]>=0&&values[2]<=1)instance->map_target=values[2];
    if(values[3]>=10&&values[3]<=500)instance->window_ms=values[3];
    if(values[4]>=0&&values[4]<=2)g_bus.global_root_policy=values[4];
    if(values[5]>=0&&values[5]<12)g_bus.global_explicit_root=values[5];
    if(values[6]>=0&&values[6]<12)g_bus.global_input_root=values[6];
    if(values[7]>=-24&&values[7]<=24)g_bus.global_transpose=values[7];
    if(values[8]>=0&&values[8]<=5)g_bus.chord_timescale=values[8];
    if(values[9]>=0&&values[9]<=2)g_bus.stability=values[9];
    if(values[10]>=0&&values[10]<=6)g_bus.accidentals=values[10];if(parsed>=12&&values[11]>=-1&&values[11]<16)instance->render_channel=values[11];
}
static int get_param(void *value,const char *key,char *buffer,int length){Inst *instance=(Inst*)value;if(!instance||!key||!buffer||length<2)return -1;hb_harmony_t harmony=bus_read();if(!strcmp(key,"state"))return snprintf(buffer,(size_t)length,"hb2,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",instance->role,instance->mode,instance->map_target,instance->window_ms,g_bus.global_root_policy,g_bus.global_explicit_root,g_bus.global_input_root,g_bus.global_transpose,g_bus.chord_timescale,g_bus.stability,g_bus.accidentals,instance->render_channel);if(!strcmp(key,"role"))return snprintf(buffer,(size_t)length,"%s",ROLE_OPTS[instance->role]);if(!strcmp(key,"mode"))return snprintf(buffer,(size_t)length,"%s",MODE_OPTS[instance->mode]);if(!strcmp(key,"map_target"))return snprintf(buffer,(size_t)length,"%s",MAP_TARGET_OPTS[instance->map_target]);if(!strcmp(key,"render_channel"))return snprintf(buffer,(size_t)length,"%s",RENDER_CH_OPTS[instance->render_channel+1]);if(!strcmp(key,"chord_timescale"))return snprintf(buffer,(size_t)length,"%s",TIMESCALE_OPTS[g_bus.chord_timescale]);if(!strcmp(key,"stability"))return snprintf(buffer,(size_t)length,"%s",STABILITY_OPTS[g_bus.stability]);if(!strcmp(key,"accidentals"))return snprintf(buffer,(size_t)length,"%s",ACCIDENTAL_OPTS[g_bus.accidentals]);if(!strcmp(key,"root_policy"))return snprintf(buffer,(size_t)length,"%s",POLICY_OPTS[g_bus.global_root_policy]);if(!strcmp(key,"explicit_root"))return snprintf(buffer,(size_t)length,"%s",PC_OPTS[g_bus.global_explicit_root]);if(!strcmp(key,"input_root"))return snprintf(buffer,(size_t)length,"%s",PC_OPTS[g_bus.global_input_root]);if(!strcmp(key,"transpose"))return snprintf(buffer,(size_t)length,"%d",g_bus.global_transpose);if(!strcmp(key,"window_ms"))return snprintf(buffer,(size_t)length,"%d",instance->window_ms);if(!strcmp(key,"detected_root"))return snprintf(buffer,(size_t)length,"%s",harmony.valid?hb_pc_display(harmony.root_pc,harmony):"--");if(!strcmp(key,"detected_bass"))return snprintf(buffer,(size_t)length,"%s",harmony.valid?hb_pc_display(harmony.bass_pc,harmony):"--");if(!strcmp(key,"detected_quality"))return snprintf(buffer,(size_t)length,"%d",harmony.valid?harmony.chord_index+1:0);if(!strcmp(key,"confidence"))return snprintf(buffer,(size_t)length,"%d",harmony.valid?harmony.confidence:0);if(!strcmp(key,"resolved_root"))return snprintf(buffer,(size_t)length,"%d",reference_root(instance));if(!strcmp(key,"harmony"))return hb_format_harmony(buffer,length,harmony);if(!strcmp(key,"pitch_mask"))return snprintf(buffer,(size_t)length,"%u",(unsigned)harmony.pitch_mask);if(!strcmp(key,"rx_count"))return snprintf(buffer,(size_t)length,"%u",instance->rx_count);if(!strcmp(key,"note_on_count"))return snprintf(buffer,(size_t)length,"%u",instance->note_on_count);if(!strcmp(key,"note_off_count"))return snprintf(buffer,(size_t)length,"%u",instance->note_off_count);if(!strcmp(key,"last_note"))return snprintf(buffer,(size_t)length,"%d",instance->last_note);if(!strcmp(key,"last_status"))return snprintf(buffer,(size_t)length,"%d",instance->last_status);if(!strcmp(key,"last_velocity"))return snprintf(buffer,(size_t)length,"%d",instance->last_velocity);if(!strcmp(key,"active_count"))return snprintf(buffer,(size_t)length,"%d",instance->active_count);if(!strcmp(key,"infer_note_count"))return snprintf(buffer,(size_t)length,"%d",instance->last_inferred_count);if(!strcmp(key,"bus_seq"))return snprintf(buffer,(size_t)length,"%u",__atomic_load_n(&g_bus.seq,__ATOMIC_ACQUIRE));if(!strcmp(key,"raw_event_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_event_count);if(!strcmp(key,"raw_note_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_count);if(!strcmp(key,"raw_note_on_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_on_count);if(!strcmp(key,"raw_note_off_count"))return snprintf(buffer,(size_t)length,"%u",instance->raw_note_off_count);if(!strcmp(key,"raw_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_note);if(!strcmp(key,"raw_last_status"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_status);if(!strcmp(key,"raw_last_velocity"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_velocity);if(!strcmp(key,"raw_last_channel"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_channel);if(!strcmp(key,"raw_last_cable"))return snprintf(buffer,(size_t)length,"%d",instance->raw_last_cable);if(!strcmp(key,"render_count"))return snprintf(buffer,(size_t)length,"%u",instance->render_count);if(!strcmp(key,"render_fail_count"))return snprintf(buffer,(size_t)length,"%u",instance->render_fail_count);if(!strcmp(key,"render_last_note"))return snprintf(buffer,(size_t)length,"%d",instance->render_last_note);if(!strcmp(key,"chain_params")){int size=(int)strlen(CHAIN_PARAMS);if(size>=length)return -1;memcpy(buffer,CHAIN_PARAMS,(size_t)size+1);return size;}return -1;}
static midi_fx_api_v1_t API={MIDI_FX_API_VERSION,create_inst,destroy_inst,process,tick,set_param,get_param};
midi_fx_api_v1_t *move_midi_fx_init(const host_api_v1_t *host){g_host=host;ensure_init();return &API;}
