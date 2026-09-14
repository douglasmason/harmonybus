#include "hb_virtual_midi.h"

int hb_virtual_make_injected_note_packet(
    int midi_channel,
    int note,
    int velocity,
    int is_note_on,
    uint8_t packet[4]) {
    if (packet == NULL) {
        return -1;
    }
    if (midi_channel < 0 || midi_channel > 15 || note < 0 || note > 127) {
        return -1;
    }
    if (velocity < 0 || velocity > 127) {
        return -1;
    }

    packet[0] = (uint8_t)(0x20 | (is_note_on ? 0x09 : 0x08));
    packet[1] = (uint8_t)((is_note_on ? 0x90 : 0x80) | (midi_channel & 0x0F));
    packet[2] = (uint8_t)(note & 0x7F);
    packet[3] = (uint8_t)(is_note_on ? velocity : 0);
    return 4;
}
