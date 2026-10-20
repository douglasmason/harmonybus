#ifndef HB_FOLLOWER_OWNERSHIP_H
#define HB_FOLLOWER_OWNERSHIP_H
/* Input identity survives mapping: live/clip, MIDI channel, source pitch.
   Display arrays remain pitch summaries; they never own a note's lifetime. */
#define HB_FOLLOWER_VOICES 256
typedef struct { int degree,target,shift; unsigned token; } hb_input_intent;
typedef struct {
    int used,source,channel,origin,pitch,velocity;
    hb_harmony_t harmony;
    hb_input_intent input;
    unsigned long long events[HB_MOTION_LANES+1];
} hb_follower_voice;
static int hb_fv_identity(int source,int channel,int origin){return 1+source+128*channel+2048*origin;}
static hb_follower_voice *hb_fv_find(hb_follower_voice *voices,int source,int channel,int origin,int create){
    hb_follower_voice *free_slot=0;
    for(int i=0;i<HB_FOLLOWER_VOICES;i++){
        hb_follower_voice *voice=&voices[i];
        if(voice->used&&voice->source==source&&voice->channel==channel&&voice->origin==origin)return voice;
        if(!voice->used&&!free_slot)free_slot=voice;
    }
    return create?free_slot:0;
}
#endif
