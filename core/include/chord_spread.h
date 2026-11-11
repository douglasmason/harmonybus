#ifndef HB_CHORD_SPREAD_H
#define HB_CHORD_SPREAD_H
#define HB_CHORD_SPREAD_SIGNED 1
/* Positive steps raise upper voices, highest first, anchoring the bass.
   Negative steps lower lower voices, lowest first, anchoring the top.
   Stop at the first MIDI boundary; never clamp or drop a chord tone. */
static void hb_chord_spread(int *notes,int count,int steps){
    if(count<2)return;
    if(steps>12)steps=12;
    if(steps<-12)steps=-12;
    int direction=steps<0?-1:1;
    if(steps<0)steps=-steps;
    for(int step=0;step<steps;step++){
        int voice=direction>0?count-1-step%(count-1):step%(count-1);
        if(direction>0?notes[voice]>115:notes[voice]<12)break;
        notes[voice]+=12*direction;
    }
}
#endif
