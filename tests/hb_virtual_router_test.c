#include <assert.h>
#include <stdio.h>

#include "../src/hb_virtual_bank.h"
#include "../src/hb_virtual_router.h"

static void test_muting_source_audio_does_not_stop_follower_render(void) {
    hb_virtual_track_config_t tracks[HB_VIRTUAL_BANK_TRACK_COUNT];
    hb_virtual_bank_default(tracks);

    hb_virtual_track_runtime_t runtime = {
        .source_audio_muted = true,
        .render_enabled = true,
    };
    hb_virtual_route_decision_t decision = hb_virtual_route_decide(&tracks[1], &runtime);

    assert(decision.send_to_source_instrument);
    assert(!decision.source_audio_audible);
    assert(decision.send_to_harmonybus);
    assert(decision.render_channel == 1);
    assert(!decision.send_raw_monitor);
}

static void test_conductor_monitor_defaults_to_channel_three(void) {
    hb_virtual_track_config_t tracks[HB_VIRTUAL_BANK_TRACK_COUNT];
    hb_virtual_bank_default(tracks);

    hb_virtual_track_runtime_t runtime = {
        .source_audio_muted = true,
        .render_enabled = true,
    };
    hb_virtual_route_decision_t decision = hb_virtual_route_decide(&tracks[0], &runtime);

    assert(decision.send_to_harmonybus);
    assert(decision.render_channel == -1);
    assert(decision.send_raw_monitor);
    assert(decision.monitor_channel == 2);
}

static void test_render_can_be_disabled_without_disabling_source_instrument(void) {
    hb_virtual_track_config_t tracks[HB_VIRTUAL_BANK_TRACK_COUNT];
    hb_virtual_bank_default(tracks);

    hb_virtual_track_runtime_t runtime = {
        .source_audio_muted = false,
        .render_enabled = false,
    };
    hb_virtual_route_decision_t decision = hb_virtual_route_decide(&tracks[3], &runtime);

    assert(decision.send_to_source_instrument);
    assert(decision.source_audio_audible);
    assert(!decision.send_to_harmonybus);
    assert(decision.render_channel == -1);
}

int main(void) {
    test_muting_source_audio_does_not_stop_follower_render();
    test_conductor_monitor_defaults_to_channel_three();
    test_render_can_be_disabled_without_disabling_source_instrument();
    puts("hb_virtual_router_test: PASS");
    return 0;
}
