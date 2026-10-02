#ifndef HARMONY_CORE_H
#define HARMONY_CORE_H

#ifdef HB_FREESTANDING
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Generated chords may carry exact semantic tones beyond a named template. */
#define HB_HARMONY_EXPLICIT_TONES 256
typedef struct {
    int valid;
    int root_pc;
    int bass_pc;
    uint16_t pitch_mask;
    uint16_t detected_mask; /* Exact generated voicing; zero uses observed pitch_mask. */
    int chord_index;
    int confidence;
    char name[24];
    int intent_kind,intent_target,intent_minor;uint16_t intent_scale; /* 0 unknown, 1 degree, 2 V, 3 leading, 4 subV, 5 connector */
} hb_harmony_t;

typedef enum {
    HB_MAP_TRANSPOSE = 0,
    HB_MAP_CHORD = 1,
    HB_MAP_NEAREST = 2
} hb_map_mode_t;

hb_harmony_t hb_infer_harmony(const uint8_t *notes, int note_count);
hb_harmony_t hb_refine_harmony_with_root(const uint8_t *notes, int note_count,
                                         hb_harmony_t established);
hb_harmony_t hb_infer_harmony_contextual(const uint8_t *notes, int note_count,
                                         hb_harmony_t committed);
hb_harmony_t hb_transpose_harmony(hb_harmony_t h, int semitones);
uint16_t hb_harmony_chord_mask(hb_harmony_t harmony);
static inline uint16_t hb_harmony_detected_mask(hb_harmony_t h){return h.valid?(h.detected_mask?h.detected_mask:h.pitch_mask):0;}
int hb_map_note(int midi_note, int reference_root_pc, hb_harmony_t target,
                hb_map_mode_t mode);
void hb_map_held_voices(const uint8_t *source_notes, int voice_count,
                        int reference_root_pc, hb_harmony_t target,
                        hb_map_mode_t mode, const int *previous_outputs,
                        int *mapped_outputs);
int hb_map_note_by_role(int midi_note, hb_harmony_t source_harmony,
                        hb_harmony_t target_harmony, int smooth);
void hb_map_held_voices_by_role(const uint8_t *source_notes, int voice_count,
                                hb_harmony_t source_harmony,
                                hb_harmony_t target_harmony, int smooth,
                                const int *previous_outputs,
                                int *mapped_outputs);
const char *hb_pc_name(int pc);

#ifdef __cplusplus
}
#endif
#endif
