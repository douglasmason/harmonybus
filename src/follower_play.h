#ifndef HB_FOLLOWER_PLAY_H
#define HB_FOLLOWER_PLAY_H
/* Musical-index transforms. The caller supplies the effective, already
   transposed harmony and the permitted collection for this input's group. */
typedef struct { int rotate, wrap, mirror, octave, range, scope, bypass; } hb_fp_config;
static int hb_fp_floor_div(int value,int divisor){
    int quotient=value/divisor;
    return quotient-(value%divisor<0);
}
static int hb_fp_note(hb_fp_config config,int note,int root,unsigned mask){
    if(config.bypass||!(mask&0xFFFu)||(!config.rotate&&!config.mirror&&!config.octave))return note;
    int tones[12],count=0;
    for(int interval=0;interval<12;interval++)
        if(mask&(1u<<((root+interval)%12)))tones[count++]=interval;
    int register_index=hb_fp_floor_div(note-root,12);
    int interval=note-root-register_index*12;
    int degree=0,distance=128;
    for(int index=0;index<count;index++){
        int difference=interval-tones[index];if(difference<0)difference=-difference;
        if(difference<distance){distance=difference;degree=index;}
    }
    /* If the root is excluded (for example the non-chord split), the first
       tone above it reflects to the last permitted tone below it. */
    int ordinal=(config.mirror?-degree-(tones[0]!=0):degree)+config.rotate;
    int carry=hb_fp_floor_div(ordinal,count);
    int result=root+12*(register_index+config.octave+(config.wrap?0:carry))+tones[ordinal-carry*count];
    /* Preserve pitch class at MIDI limits instead of clamping to a foreign tone. */
    while(result<0)result+=12;
    while(result>127)result-=12;
    return result;
}
#endif
