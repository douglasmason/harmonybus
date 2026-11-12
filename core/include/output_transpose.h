#ifndef HB_OUTPUT_TRANSPOSE_H
#define HB_OUTPUT_TRANSPOSE_H
/* Final MIDI output, after the existing rhythm/voice owner. The map is
   injective: out-of-range pitches are silent, never clamped onto a peer.
   Reconcile all OFFs before ONs; common tones stay sounding. Each pop emits
   at most one message, so small host buffers cannot lose a release. */
typedef struct {
    unsigned char logical[16][128],heard[16][128];
    int current,target,stage,cursor;
} hb_output_transpose;
static void hb_ot_target(hb_output_transpose *state,int target){
    if(target==state->target)return;
    state->target=target;state->stage=1;state->cursor=0;
}
static int hb_ot_pending(hb_output_transpose *state,unsigned char message[3]){
    while(state->stage){
        while(state->cursor<16*128){
            int index=state->cursor++,channel=index/128,pitch=index%128;
            int source=pitch-state->target;
            int wanted=source>=0&&source<128?state->logical[channel][source]:0;
            int heard=state->heard[channel][pitch];
            if((state->stage==1&&heard&&!wanted)||(state->stage==2&&!heard&&wanted)){
                int velocity=state->stage==2?wanted:0;
                message[0]=(unsigned char)((velocity?0x90:0x80)|channel);
                message[1]=(unsigned char)pitch;message[2]=(unsigned char)velocity;
                state->heard[channel][pitch]=(unsigned char)velocity;return 1;
            }
        }
        if(state->stage==1){state->stage=2;state->cursor=0;}
        else {state->stage=0;state->current=state->target;}
    }
    return 0;
}
static int hb_ot_message(hb_output_transpose *state,unsigned char message[3]){
    int type=message[0]&240,channel=message[0]&15,source=message[1]&127;
    if(type==0xB0&&(source==120||source==123)){
        memset(state->logical[channel],0,128);memset(state->heard[channel],0,128);
    }
    if(type!=0x80&&type!=0x90&&type!=0xA0)return 1;
    int pitch=source+state->current;
    /* Late pressure cannot address a released, skipped or out-of-range voice. */
    if(type==0xA0&&!state->logical[channel][source])return 0;
    if(type!=0xA0)state->logical[channel][source]=type==0x90?message[2]:0;
    if(pitch<0||pitch>127)return 0;
    message[1]=(unsigned char)pitch;
    if(type!=0xA0)state->heard[channel][pitch]=type==0x90?message[2]:0;
    return 1;
}
static void hb_ot_panic(hb_output_transpose *state){
    /* Stop during a partially drained revoice must never resume its ON pass. */
    memset(state->logical,0,sizeof(state->logical));
    if(state->stage){state->stage=1;state->cursor=0;}
}
#endif
