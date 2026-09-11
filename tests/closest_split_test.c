#include <assert.h>
#include <stdio.h>
#include "../src/closest_split.h"

static void assert_strictly_increasing(const int outputs[HB_CLOSEST_SPLIT_DEGREES]) {
    for (int degree = 1; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        assert(outputs[degree] > outputs[degree - 1]);
}

int main(void) {
    /* C-major-like nominals, with 1/3/5 constrained to chord tones and the
       remaining degrees constrained to non-chord scale tones. */
    const int nominals[HB_CLOSEST_SPLIT_DEGREES] = {60, 62, 64, 65, 67, 69, 71};
    const unsigned int chord_mask = (1u << 0) | (1u << 4) | (1u << 7);
    const unsigned int out_mask = (1u << 2) | (1u << 5) | (1u << 9) | (1u << 11);
    unsigned int masks[HB_CLOSEST_SPLIT_DEGREES];
    int outputs[HB_CLOSEST_SPLIT_DEGREES];

    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        masks[degree] = (degree == 0 || degree == 2 || degree == 4) ? chord_mask : out_mask;

    assert(hb_build_monotonic_degree_ladder(nominals, masks, outputs));
    assert_strictly_increasing(outputs);
    for (int degree = 0; degree < HB_CLOSEST_SPLIT_DEGREES; ++degree)
        assert(masks[degree] & (1u << (outputs[degree] % 12)));

    /* Regression for the reported shape: degree 4 and 6 must never both fall
       below degree 5, nor can two degrees collapse to the same MIDI pitch. */
    assert(outputs[4] < outputs[5]);
    assert(outputs[5] < outputs[6]);

    /* Even a very narrow legal mask still yields distinct ordered MIDI notes
       by using successive octaves. */
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
