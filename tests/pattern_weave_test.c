#include <assert.h>
#include <stdio.h>
#include "../core/include/pattern_weave.h"
int main(void) {
    const hb_weave_step_t melody[] = {{60,90,1},{62,90,1},{64,90,1},{65,90,1}};
    const hb_weave_step_t rhythm[] = {{72,50,1},{74,51,0}};
    hb_weave_step_t result;
    result = hb_weave_at(melody,4,rhythm,2,1,4,HB_WEAVE_A_PITCH_B_RHYTHM);
    assert(result.note == 62 && result.velocity == 51 && result.gate == 0);
    result = hb_weave_at(melody,4,rhythm,2,2,4,HB_WEAVE_ALTERNATE_STEPS);
    assert(result.note == 64 && result.gate == 1);
    result = hb_weave_at(melody,4,rhythm,2,5,4,HB_WEAVE_ALTERNATE_BARS);
    assert(result.note == 74 && result.gate == 0);
    result = hb_weave_at(melody,4,rhythm,2,4,4,HB_WEAVE_B_PITCH_A_RHYTHM);
    assert(result.note == 72 && result.velocity == 90 && result.gate == 1);
    result = hb_weave_at(melody,4,rhythm,2,0,0,HB_WEAVE_ALTERNATE_BARS);
    assert(result.gate == 0);
    result = hb_weave_at(NULL,4,rhythm,2,0,4,HB_WEAVE_ALTERNATE_STEPS);
    assert(result.gate == 0);
    result = hb_weave_at(melody,4,rhythm,2,0,4,(hb_weave_mode_t)99);
    assert(result.gate == 0);
    for (unsigned step_index = 0; step_index < 4096; ++step_index) {
        result = hb_weave_at(melody,4,rhythm,2,step_index,4,HB_WEAVE_A_PITCH_B_RHYTHM);
        assert(result.note == melody[step_index % 4].note);
        assert(result.gate == rhythm[step_index % 2].gate);
    }
    puts("Pattern weave: all assertions pass");
    return 0;
}
