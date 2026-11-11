#ifndef HB_CHORD_SPREAD_H
#define HB_CHORD_SPREAD_H
/* One step raises one upper voice by an octave, highest voice first.
   The bass, voice count and pitch classes stay fixed. Stop at the first
   MIDI ceiling instead of clamping or dropping a chord tone. */
static void hb_chord_spread(int *notes,int count,int steps){
    if(count<2)return;
    if(steps>12)steps=12;
    for(int step=0;step<steps;step++){
        int voice=count-1-step%(count-1);
        if(notes[voice]>115)break;
        notes[voice]+=12;
    }
}
#endif
