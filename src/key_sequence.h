#ifndef HB_KEY_SEQUENCE_H
#define HB_KEY_SEQUENCE_H
/* Recorded key action: c bits 26..27 = set/shift/back/return. For shifts,
   a contains (semitones+12) in bits 0..4 and return-after in bits 5..11.
   The remaining fields retain the existing landing/input-context contract. */
#define HB_KS_HISTORY 64
typedef struct { int root,mask,blues; } hb_ks_key;
typedef struct { int valid,count,depth; hb_ks_key origin,current,history[HB_KS_HISTORY]; } hb_ks_state;
static hb_ks_key hb_ks_before(hb_sc_value event){
    int mask=(event.c>>13)&4095;
    return mask?(hb_ks_key){(event.c>>9)&15,mask,(event.c>>25)&1}:
        (hb_ks_key){event.a%12,event.b&4095,event.c&1};
}
static hb_sc_value hb_ks_apply(hb_ks_state *state,hb_sc_value event){
    if(!state->valid){state->valid=1;state->origin=state->current=hb_ks_before(event);}
    hb_ks_key before=state->current;int action=(event.c>>26)&3;
    if(action==3||(action==1&&(event.a>>5)>0&&state->count>=(event.a>>5))){
        state->current=state->origin;state->count=state->depth=0;
    }else if(action==2){
        if(state->depth){state->current=state->history[--state->depth];if(state->count)state->count--;}
    }else{
        if(state->depth==HB_KS_HISTORY){for(int index=1;index<HB_KS_HISTORY;index++)state->history[index-1]=state->history[index];state->depth--;}
        state->history[state->depth++]=state->current;
        if(state->count<64)state->count++;
        if(action==1){int shift=(event.a&31)-12;
            state->current.root=(state->current.root+shift+12)%12;
            unsigned mask=(unsigned)state->current.mask;shift=(shift+12)%12;
            state->current.mask=(int)(((mask<<shift)|(mask>>(12-shift)))&4095);
        }else state->current=(hb_ks_key){event.a,event.b&4095,event.c&1};
    }
    event.a=state->current.root;event.b=(event.b&~4095)|state->current.mask;
    event.c=(event.c&510)|state->current.blues|(before.root<<9)|(before.mask<<13)|(before.blues<<25);
    return event;
}
#endif
