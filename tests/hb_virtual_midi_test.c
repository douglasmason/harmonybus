#include <assert.h>
#include <stdio.h>

#include "../src/hb_virtual_midi.h"

static void test_note_on_packet(void) {
    uint8_t packet[4] = {0};
    assert(hb_virtual_make_injected_note_packet(2, 60, 101, 1, packet) == 4);
    assert(packet[0] == 0x29);
    assert(packet[1] == 0x92);
    assert(packet[2] == 60);
    assert(packet[3] == 101);
}

static void test_note_off_packet(void) {
    uint8_t packet[4] = {0};
    assert(hb_virtual_make_injected_note_packet(1, 64, 127, 0, packet) == 4);
    assert(packet[0] == 0x28);
    assert(packet[1] == 0x81);
    assert(packet[2] == 64);
    assert(packet[3] == 0);
}

static void test_invalid_values(void) {
    uint8_t packet[4] = {0};
    assert(hb_virtual_make_injected_note_packet(-1, 60, 100, 1, packet) == -1);
    assert(hb_virtual_make_injected_note_packet(16, 60, 100, 1, packet) == -1);
    assert(hb_virtual_make_injected_note_packet(0, 128, 100, 1, packet) == -1);
    assert(hb_virtual_make_injected_note_packet(0, 60, 128, 1, packet) == -1);
    assert(hb_virtual_make_injected_note_packet(0, 60, 100, 1, NULL) == -1);
}

int main(void) {
    test_note_on_packet();
    test_note_off_packet();
    test_invalid_values();
    puts("hb_virtual_midi_test: PASS");
    return 0;
}
