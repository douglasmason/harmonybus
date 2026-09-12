#include <assert.h>
#include <stdio.h>

#include "../src/hb_virtual_midi.h"

int main(void) {
    uint8_t packet[4] = {0};
    assert(hb_virtual_make_injected_note_packet(2, 60, 101, 1, packet) == 4);
    assert(packet[0] == 0x29 && packet[1] == 0x92 && packet[2] == 60 && packet[3] == 101);
    assert(hb_virtual_make_injected_note_packet(1, 64, 127, 0, packet) == 4);
    assert(packet[0] == 0x28 && packet[1] == 0x81 && packet[2] == 64 && packet[3] == 0);
    assert(hb_virtual_make_injected_note_packet(-1, 60, 100, 1, packet) == -1);
    assert(hb_virtual_make_injected_note_packet(16, 60, 100, 1, packet) == -1);
    assert(hb_virtual_make_injected_note_packet(0, 128, 100, 1, packet) == -1);
    assert(hb_virtual_make_injected_note_packet(0, 60, 128, 1, packet) == -1);
    puts("hb_virtual_midi_test: PASS");
    return 0;
}
