#include "hb_virtual_bank.h"

#include <stddef.h>
#include <stdio.h>

void hb_virtual_bank_default(hb_virtual_track_config_t out_tracks[HB_VIRTUAL_BANK_TRACK_COUNT]) {
    if (out_tracks == NULL) {
        return;
    }

    out_tracks[0].logical_track = 5;
    out_tracks[0].role = HB_VIRTUAL_ROLE_CONDUCTOR;
    out_tracks[0].render_channel = -1;
    out_tracks[0].monitor_channel = 2; /* MIDI channel 3. */

    out_tracks[1].logical_track = 6;
    out_tracks[1].role = HB_VIRTUAL_ROLE_FOLLOWER;
    out_tracks[1].render_channel = 1; /* MIDI channel 2. */
    out_tracks[1].monitor_channel = -1;

    out_tracks[2].logical_track = 7;
    out_tracks[2].role = HB_VIRTUAL_ROLE_FOLLOWER;
    out_tracks[2].render_channel = 2; /* MIDI channel 3. */
    out_tracks[2].monitor_channel = -1;

    out_tracks[3].logical_track = 8;
    out_tracks[3].role = HB_VIRTUAL_ROLE_FOLLOWER;
    out_tracks[3].render_channel = 3; /* MIDI channel 4. */
    out_tracks[3].monitor_channel = -1;
}

int hb_virtual_bank_index_for_track(int logical_track) {
    if (logical_track < HB_VIRTUAL_BANK_FIRST_TRACK || logical_track > HB_VIRTUAL_BANK_LAST_TRACK) {
        return -1;
    }
    return logical_track - HB_VIRTUAL_BANK_FIRST_TRACK;
}

const hb_virtual_track_config_t *hb_virtual_bank_track(
    const hb_virtual_track_config_t tracks[HB_VIRTUAL_BANK_TRACK_COUNT],
    int bank_index) {
    if (tracks == NULL || bank_index < 0 || bank_index >= HB_VIRTUAL_BANK_TRACK_COUNT) {
        return NULL;
    }
    return &tracks[bank_index];
}

int hb_virtual_bank_make_harmonybus_state(
    const hb_virtual_track_config_t *track,
    char *buffer,
    size_t buffer_size) {
    if (track == NULL || buffer == NULL || buffer_size == 0) {
        return -1;
    }
    if (track->role != HB_VIRTUAL_ROLE_CONDUCTOR && track->role != HB_VIRTUAL_ROLE_FOLLOWER) {
        return -1;
    }
    if (track->render_channel < -1 || track->render_channel > 15) {
        return -1;
    }

    /*
     * HarmonyBus hb15 state fields, in order:
     * role, mode, map_target, inference_window_ms, root_policy, explicit_root,
     * input_root, transpose, split_map, stability, accidentals, render_channel,
     * source_channel, chord_timing, context, clip_context, follow_lookahead_ms,
     * retrigger_held, anticipation, boundary_buffer_ms, analysis_release_ms,
     * content_map, travel_map, follower_scale, quant_timing.
     *
     * These match a fresh HarmonyBus instance/global bus. source_channel stays
     * Auto (-1); Movy's hosted chain already gives us stable virtual-track
     * ownership and the follower's output is selected solely by render_channel.
     */
    return snprintf(
        buffer,
        buffer_size,
        "hb15,%d,0,0,25,2,0,0,0,0,0,0,%d,-1,0,0,1,0,0,0,20,60,0,0,0,0",
        (int)track->role,
        track->render_channel);
}
