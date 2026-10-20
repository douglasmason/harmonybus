#ifndef HARMONYBUS_APPROACH_PITCH_H
#define HARMONYBUS_APPROACH_PITCH_H

/* Return the next STRICTLY higher MIDI note whose pitch class is present in
 * scale_mask.  The mask uses the low 12 bits for C..B.  If the mask is empty
 * or no higher MIDI note exists, return mapped unchanged. */
static inline int hb_next_scale_pitch_above(int mapped, unsigned int scale_mask) {
    scale_mask &= 0x0FFFu;
    if (!scale_mask) return mapped;
    for (int semitones = 1; semitones <= 12; ++semitones) {
        int candidate = mapped + semitones;
        if (candidate > 127) break;
        int pitch_class = candidate % 12;
        if (pitch_class < 0) pitch_class += 12;
        if (scale_mask & (1u << pitch_class)) return candidate;
    }
    return mapped;
}

#endif
