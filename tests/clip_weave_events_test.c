#include <assert.h>
#include <stdio.h>
#include "../core/include/clip_weave_events.h"
int main(void) {
    const hb_weave_event_t pitches[]={{0,12,60,80,1},{24,12,62,80,1},{48,12,64,80,1},{72,12,67,80,1}};
    const hb_weave_event_t rhythm[]={{0,6,0,90,1},{12,6,0,70,1},{24,6,0,0,0},{36,6,0,80,1},{48,6,0,95,1},{60,6,0,0,0},{72,6,0,75,1},{84,6,0,60,1}};
    hb_weave_stream_t pitch={pitches,4,96,0}, beat={rhythm,8,96,7};
    const uint8_t expected[]={60,62,64,67,60,62,64,67,60,62,64,67};
    unsigned emitted=0;
    for(uint64_t tick=0;tick<199;++tick){
        hb_woven_note_t note={0};
        if(hb_weave_resolve_attack(pitch,beat,tick,&note)){
            assert(note.note==expected[emitted]);
            assert(note.tick==tick);
            assert(note.duration==6);
            ++emitted;
        }
    }
    assert(emitted==12);
    hb_woven_note_t out={0};
    assert(!hb_weave_resolve_attack(pitch,beat,6,&out));
    assert(!hb_weave_resolve_attack(pitch,beat,31,&out));
    assert(!hb_weave_resolve_attack(pitch,beat,7,NULL));
    const hb_weave_event_t empty[]={{0,1,0,0,0}};
    assert(!hb_weave_resolve_attack((hb_weave_stream_t){empty,1,96,0},beat,7,&out));
    puts("Clip event weaving: independent loops, skipped rests, attack order and guards pass");
    return 0;
}
