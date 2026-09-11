#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/hb_virtual_bank.h"

static void test_default_mapping(void) {
    hb_virtual_track_config_t tracks[HB_VIRTUAL_BANK_TRACK_COUNT];
    hb_virtual_bank_default(tracks);

    assert(tracks[0].logical_track == 5);
    assert(tracks[0].role == HB_VIRTUAL_ROLE_CONDUCTOR);
    assert(tracks[0].render_channel == -1);
    assert(tracks[0].monitor_channel == 2);

    assert(tracks[1].logical_track == 6);
    assert(tracks[1].role == HB_VIRTUAL_ROLE_FOLLOWER);
    assert(tracks[1].render_channel == 1);
    assert(tracks[1].monitor_channel == -1);

    assert(tracks[2].logical_track == 7);
    assert(tracks[2].role == HB_VIRTUAL_ROLE_FOLLOWER);
    assert(tracks[2].render_channel == 2);
    assert(tracks[2].monitor_channel == -1);

    assert(tracks[3].logical_track == 8);
    assert(tracks[3].role == HB_VIRTUAL_ROLE_FOLLOWER);
    assert(tracks[3].render_channel == 3);
    assert(tracks[3].monitor_channel == -1);
}

static void test_track_indices(void) {
    assert(hb_virtual_bank_index_for_track(4) == -1);
    assert(hb_virtual_bank_index_for_track(5) == 0);
    assert(hb_virtual_bank_index_for_track(6) == 1);
    assert(hb_virtual_bank_index_for_track(7) == 2);
    assert(hb_virtual_bank_index_for_track(8) == 3);
    assert(hb_virtual_bank_index_for_track(9) == -1);
}

static void test_generated_harmonybus_states(void) {
    hb_virtual_track_config_t tracks[HB_VIRTUAL_BANK_TRACK_COUNT];
    char state[256];
    hb_virtual_bank_default(tracks);

    int written = hb_virtual_bank_make_harmonybus_state(&tracks[0], state, sizeof(state));
    assert(written > 0 && (size_t)written < sizeof(state));
    assert(strcmp(
        state,
        "hb15,0,0,0,25,2,0,0,0,0,0,0,-1,-1,0,0,1,0,0,0,20,60,0,0,0,0") == 0);

    written = hb_virtual_bank_make_harmonybus_state(&tracks[1], state, sizeof(state));
    assert(written > 0 && (size_t)written < sizeof(state));
    assert(strcmp(
        state,
        "hb15,1,0,0,25,2,0,0,0,0,0,0,1,-1,0,0,1,0,0,0,20,60,0,0,0,0") == 0);

    written = hb_virtual_bank_make_harmonybus_state(&tracks[2], state, sizeof(state));
    assert(written > 0 && (size_t)written < sizeof(state));
    assert(strstr(state, ",2,-1,0,0,1,") != NULL);

    written = hb_virtual_bank_make_harmonybus_state(&tracks[3], state, sizeof(state));
    assert(written > 0 && (size_t)written < sizeof(state));
    assert(strstr(state, ",3,-1,0,0,1,") != NULL);
}

int main(void) {
    test_default_mapping();
    test_track_indices();
    test_generated_harmonybus_states();
    puts("hb_virtual_bank_test: PASS");
    return 0;
}
