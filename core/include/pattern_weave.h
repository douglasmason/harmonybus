#ifndef HB_PATTERN_WEAVE_H
#define HB_PATTERN_WEAVE_H
/* Stateless pattern composition. Does not own transport, MIDI or harmony. */
#include <stdint.h>
typedef struct {
    uint8_t note;
    uint8_t velocity;
    uint8_t gate; /* Zero is a rest; MIDI note zero remains valid. */
} hb_weave_step_t;
typedef enum {
    HB_WEAVE_ALTERNATE_STEPS = 0,
    HB_WEAVE_ALTERNATE_BARS = 1,
    HB_WEAVE_A_PITCH_B_RHYTHM = 2,
    HB_WEAVE_B_PITCH_A_RHYTHM = 3
} hb_weave_mode_t;
/* Inputs loop independently. Rhythm source supplies gate and velocity.
   Invalid input returns rest. Steps per bar uses the output step grid. */
static inline hb_weave_step_t hb_weave_at(const hb_weave_step_t *pattern_a,
                                         unsigned count_a,
                                         const hb_weave_step_t *pattern_b,
                                         unsigned count_b,
                                         unsigned step_index,
                                         unsigned steps_per_bar,
                                         hb_weave_mode_t mode) {
    hb_weave_step_t rest = {0, 0, 0};
    if (!pattern_a || !pattern_b || !count_a || !count_b) return rest;
    const hb_weave_step_t source_a = pattern_a[step_index % count_a];
    const hb_weave_step_t source_b = pattern_b[step_index % count_b];
    switch (mode) {
        case HB_WEAVE_ALTERNATE_STEPS:
            return (step_index & 1u) ? source_b : source_a;
        case HB_WEAVE_ALTERNATE_BARS:
            if (!steps_per_bar) return rest;
            return ((step_index / steps_per_bar) & 1u) ? source_b : source_a;
        case HB_WEAVE_A_PITCH_B_RHYTHM: {
            hb_weave_step_t result = {source_a.note, source_b.velocity, source_b.gate};
            return result;
        }
        case HB_WEAVE_B_PITCH_A_RHYTHM: {
            hb_weave_step_t result = {source_b.note, source_a.velocity, source_a.gate};
            return result;
        }
        default:
            return rest;
    }
}
#endif
