#ifndef HB_VOICING_ROTATE_H
#define HB_VOICING_ROTATE_H
/* Move an outside voice past the opposite edge in octave increments. This
   preserves sorted, distinct MIDI notes even for wide or spread voicings. */
static void hb_voicing_rotate(int *notes,int count,int steps){
    if(count<2)return;
    if(steps>12)steps=12;
    if(steps<-12)steps=-12;
    int direction=steps<0?-1:1;
    if(steps<0)steps=-steps;
    for(int step=0;step<steps;step++){
        if(direction>0){
            int pitch=notes[0];while(pitch<=notes[count-1])pitch+=12;
            if(pitch>127)break;
            for(int voice=0;voice<count-1;voice++)notes[voice]=notes[voice+1];
            notes[count-1]=pitch;
        }else{
            int pitch=notes[count-1];while(pitch>=notes[0])pitch-=12;
            if(pitch<0)break;
            for(int voice=count-1;voice>0;voice--)notes[voice]=notes[voice-1];
            notes[0]=pitch;
        }
    }
}
#endif
