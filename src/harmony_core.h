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

typedef struct {
    int valid;
    int root_pc;
    int bass_pc;
    uint16_t pitch_mask;
    int chord_index;
    int confidence;
    char name[24];
} hb_harmony_t;

typedef enum {
    HB_MAP_TRANSPOSE = 0,
    HB_MAP_CHORD = 1,
    HB_MAP_NEAREST = 2
} hb_map_mode_t;

hb_harmony_t hb_infer_harmony(const uint8_t *notes, int note_count);
hb_harmony_t hb_refine_harmony_with_root(const uint8_t *notes, int note_count,
                                         hb_harmony_t established);
hb_harmony_t hb_transpose_harmony(hb_harmony_t h, int semitones);
uint16_t hb_harmony_chord_mask(hb_harmony_t harmony);
int hb_map_note(int midi_note, int reference_root_pc, hb_harmony_t target,
                hb_map_mode_t mode);
const char *hb_pc_name(int pc);

#ifdef __cplusplus
}
#endif
#endif
