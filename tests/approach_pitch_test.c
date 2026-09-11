#include <assert.h>
#include "../src/approach_pitch.h"

static unsigned int mask_from_pitch_classes(const int *pitch_classes, int count) {
    unsigned int mask = 0u;
    for (int index = 0; index < count; ++index) {
        mask |= 1u << pitch_classes[index];
    }
    return mask;
}

int main(void) {
    const int c_major_pitch_classes[] = {0, 2, 4, 5, 7, 9, 11};
    const int f_minor_pitch_classes[] = {0, 1, 3, 5, 7, 8, 10};
    unsigned int c_major_mask = mask_from_pitch_classes(c_major_pitch_classes, 7);
    unsigned int f_minor_mask = mask_from_pitch_classes(f_minor_pitch_classes, 7);

    assert(hb_next_scale_pitch_above(60, c_major_mask) == 62); /* C4 -> D4 */
    assert(hb_next_scale_pitch_above(64, c_major_mask) == 65); /* E4 -> F4 */
    assert(hb_next_scale_pitch_above(71, c_major_mask) == 72); /* B4 -> C5 */
    assert(hb_next_scale_pitch_above(65, f_minor_mask) == 67); /* F4 -> G4 */
    assert(hb_next_scale_pitch_above(67, f_minor_mask) == 68); /* G4 -> Ab4 */
    assert(hb_next_scale_pitch_above(60, 0u) == 60);
    assert(hb_next_scale_pitch_above(127, c_major_mask) == 127);
    return 0;
}
