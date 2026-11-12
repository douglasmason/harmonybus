#ifndef HB_NOTE_BLEND_H
#define HB_NOTE_BLEND_H
#ifdef HB_FREESTANDING
extern double exp(double),fabs(double);
#else
#include <math.h>
#endif
/* A final-output monophonic pressure centroid. Input gates remain independent
   from the one heard voice. Reconcile OFF, bend, then ON, one message per pop;
   mode edits and tiny host buffers cannot discard a release. */
typedef struct {
    unsigned char held[16][128],weight[16][128],heard[16][128];
    unsigned short dirty,active;
    int mode,anchor[16],wanted[16],bend[16],wanted_bend[16],velocity[16];
    double current[16],target[16];
} hb_note_blend;
static int hb_nb_range(int mode){static const int ranges[]={0,0,2,12,24,48};return ranges[mode];}
static void hb_nb_init(hb_note_blend *state){
    memset(state,0,sizeof(*state));
    for(int channel=0;channel<16;channel++)state->bend[channel]=state->wanted_bend[channel]=8192;
}
static void hb_nb_goal(hb_note_blend *state,int channel){
    unsigned bit=1u<<channel;
    int first=-1,velocity=0,total=0;double weighted=0;
    for(int pitch=0;pitch<128;pitch++)if(state->held[channel][pitch]){
        if(first<0)first=pitch;
        if(state->held[channel][pitch]>velocity)velocity=state->held[channel][pitch];
        total+=state->weight[channel][pitch];weighted+=pitch*state->weight[channel][pitch];
    }
    state->wanted_bend[channel]=8192;state->velocity[channel]=velocity;
    if(first<0||!state->mode){
        state->active&=~bit;state->anchor[channel]=state->wanted[channel]=0;
    }else{
        if(!(state->active&bit)){
            state->current[channel]=state->target[channel]=total?weighted/total:first;
            state->anchor[channel]=first+1;state->active|=bit;
        }
        /* Pressure zero is a held gate, not a release or a division by zero. */
        if(total)state->target[channel]=weighted/total;
        double position=state->current[channel];
        int pitch=(int)(position+.5);
        if(state->mode==1){
            int previous=state->wanted[channel]-1;
            if(previous>=0&&fabs(position-previous)<.65)pitch=previous;
            state->wanted[channel]=pitch+1;
        }else{
            int range=hb_nb_range(state->mode),anchor=state->anchor[channel]-1;
            /* Rearticulate only when the receiver's selected bend span ends. */
            if(fabs(position-anchor)>range)state->anchor[channel]=(anchor=pitch)+1;
            state->wanted[channel]=anchor+1;
            int bend=(int)(8192.0+(position-anchor)*8192.0/range+.5);
            state->wanted_bend[channel]=bend<0?0:bend>16383?16383:bend;
        }
    }
    state->dirty|=bit;
}
static void hb_nb_mode(hb_note_blend *state,int mode){
    if(mode<0||mode>5||mode==state->mode)return;
    state->mode=mode;
    for(int channel=0;channel<16;channel++)hb_nb_goal(state,channel);
}
static void hb_nb_tick(hb_note_blend *state,double seconds){
    if(!state->mode||!state->active||seconds<=0)return;
    double amount=1-exp(-seconds/.025);
    for(int channel=0;channel<16;channel++)if(state->active&(1u<<channel)){
        double delta=state->target[channel]-state->current[channel];
        if(fabs(delta)<.00001)continue;
        state->current[channel]+=delta*amount;
        if(fabs(state->target[channel]-state->current[channel])<.00001)state->current[channel]=state->target[channel];
        hb_nb_goal(state,channel);
    }
}
static int hb_nb_wanted(const hb_note_blend *state,int channel,int pitch){
    return state->mode?(state->wanted[channel]==pitch+1?state->velocity[channel]:0):state->held[channel][pitch];
}
static int hb_nb_pop(hb_note_blend *state,unsigned char message[3]){
    for(int channel=0;channel<16;channel++)if(state->dirty&(1u<<channel)){
        for(int pitch=0;pitch<128;pitch++)if(state->heard[channel][pitch]&&!hb_nb_wanted(state,channel,pitch)){
            state->heard[channel][pitch]=0;message[0]=0x80|channel;message[1]=pitch;message[2]=0;return 1;
        }
        if(state->bend[channel]!=state->wanted_bend[channel]){
            int bend=state->bend[channel]=state->wanted_bend[channel];
            message[0]=0xe0|channel;message[1]=bend&127;message[2]=bend>>7;return 1;
        }
        for(int pitch=0;pitch<128;pitch++){
            int velocity=hb_nb_wanted(state,channel,pitch);
            if(velocity&&!state->heard[channel][pitch]){
                state->heard[channel][pitch]=velocity;message[0]=0x90|channel;message[1]=pitch;message[2]=velocity;return 1;
            }
        }
        state->dirty&=~(1u<<channel);
    }
    return 0;
}
static int hb_nb_message(hb_note_blend *state,unsigned char message[3]){
    int type=message[0]&240,channel=message[0]&15,pitch=message[1]&127;
    if(type==0xb0&&(pitch==120||pitch==123)){
        memset(state->held[channel],0,128);memset(state->heard[channel],0,128);
        hb_nb_goal(state,channel);return 1;
    }
    /* The selected mode owns channel bend; unrelated controllers pass through. */
    if(type==0xe0){
        if(state->mode)return 0;
        state->bend[channel]=state->wanted_bend[channel]=(message[2]<<7)|message[1];return 1;
    }
    if(type!=0x80&&type!=0x90&&type!=0xa0)return 1;
    if(type==0xa0){
        if(!state->held[channel][pitch])return 0;
        state->weight[channel][pitch]=message[2];
    }else{
        state->held[channel][pitch]=type==0x90?message[2]:0;
        state->weight[channel][pitch]=state->held[channel][pitch];
    }
    if(!state->mode){
        if(type!=0xa0)state->heard[channel][pitch]=state->held[channel][pitch];
        return 1;
    }
    hb_nb_goal(state,channel);return 0;
}
static void hb_nb_panic(hb_note_blend *state){
    memset(state->held,0,sizeof(state->held));
    if(!state->mode&&!state->dirty){memset(state->heard,0,sizeof(state->heard));return;}
    for(int channel=0;channel<16;channel++)hb_nb_goal(state,channel);
}
#endif
