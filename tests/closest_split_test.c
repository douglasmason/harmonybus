#include <assert.h>
#include <stdio.h>
#include "../src/closest_split.h"

static void assert_strictly_increasing(const int outputs[HB_CLOSEST_SPLIT_DEGREES]) {
    for (int degree = 1; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        assert(outputs[degree] > outputs[degree - 1]);
}

static void assert_unique_pitch_classes(const int outputs[HB_CLOSEST_SPLIT_DEGREES]) {
    unsigned int seen = 0;
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree) {
        unsigned int bit = 1u << (outputs[degree] % 12);
        assert((seen & bit) == 0);
        seen |= bit;
    }
}

int main(void) {
    const int nominals[HB_CLOSEST_SPLIT_DEGREES] = {60, 62, 64, 65, 67, 69, 71};
    const unsigned int degree_mask[HB_CLOSEST_SPLIT_DEGREES] = {
        1u << 0, 1u << 2, 1u << 4, 1u << 5, 1u << 7, 1u << 9, 1u << 11
    };
    unsigned int masks[HB_CLOSEST_SPLIT_DEGREES];
    int outputs[HB_CLOSEST_SPLIT_DEGREES];

    /* 135 / 2467: the two sides partition all seven C-major scale tones.
       All seven follower degrees must therefore be unique pitch classes and
       strictly increasing MIDI notes. */
    const unsigned int on_135 = degree_mask[0] | degree_mask[2] | degree_mask[4];
    const unsigned int off_2467 = degree_mask[1] | degree_mask[3] | degree_mask[5] | degree_mask[6];
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        masks[degree] = (degree == 0 || degree == 2 || degree == 4) ? on_135 : off_2467;
    assert(hb_build_monotonic_degree_ladder(nominals, masks, outputs));
    assert_strictly_increasing(outputs);
    assert_unique_pitch_classes(outputs);
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        assert(masks[degree] & (1u << (outputs[degree] % 12)));

    /* Regression for the reported shape: 4 < 5 < 6 in rendered register. */
    assert(outputs[3] < outputs[4]);
    assert(outputs[4] < outputs[5]);

    /* 1357 / 246 has the same seven-tone completeness guarantee. */
    const unsigned int on_1357 = on_135 | degree_mask[6];
    const unsigned int off_246 = degree_mask[1] | degree_mask[3] | degree_mask[5];
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        masks[degree] = (degree == 0 || degree == 2 || degree == 4 || degree == 6) ? on_1357 : off_246;
    assert(hb_build_monotonic_degree_ladder(nominals, masks, outputs));
    assert_strictly_increasing(outputs);
    assert_unique_pitch_classes(outputs);
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        assert(masks[degree] & (1u << (outputs[degree] % 12)));

    /* If Content narrows the legal set so seven unique pitch classes are
       impossible, keep the stronger practical invariant: seven distinct,
       monotonically increasing MIDI notes, allowing octave repeats. */
    const unsigned int tonic_only = 1u << 0;
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        masks[degree] = tonic_only;
    assert(hb_build_monotonic_degree_ladder(nominals, masks, outputs));
    assert_strictly_increasing(outputs);
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        assert((outputs[degree] % 12) == 0);

    printf("closest_split_test: ok\n");
    return 0;
}
