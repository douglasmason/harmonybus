#ifndef HB_VIRTUAL_MIDI_H
#define HB_VIRTUAL_MIDI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int hb_virtual_make_injected_note_packet(
    int midi_channel,
    int note,
    int velocity,
    int is_note_on,
    uint8_t packet[4]);

#ifdef __cplusplus
}
#endif

#endif
