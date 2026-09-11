#ifndef HB_VIRTUAL_BANK_H
#define HB_VIRTUAL_BANK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HB_VIRTUAL_ROLE_CONDUCTOR = 0,
    HB_VIRTUAL_ROLE_FOLLOWER = 1
} hb_virtual_role_t;

typedef struct {
    int logical_track;             /* User-facing virtual track number: 5..8. */
    hb_virtual_role_t role;
    int render_channel;            /* Zero-based MIDI channel, -1 for Off. */
    int monitor_channel;           /* Zero-based raw-monitor channel, -1 for Off. */
} hb_virtual_track_config_t;

#define HB_VIRTUAL_BANK_TRACK_COUNT 4
#define HB_VIRTUAL_BANK_FIRST_TRACK 5
#define HB_VIRTUAL_BANK_LAST_TRACK 8

/*
 * Populate the default HarmonyBus source bank:
 *   Track 5: Conductor, raw monitor -> MIDI channel 3
 *   Track 6: Follower  -> MIDI channel 2
 *   Track 7: Follower  -> MIDI channel 3
 *   Track 8: Follower  -> MIDI channel 4
 *
 * Channels are stored zero-based to match HarmonyBus' existing render_channel
 * representation. The user-facing channel numbers above are therefore stored
 * as 2, 1, 2, and 3 respectively.
 */
void hb_virtual_bank_default(hb_virtual_track_config_t out_tracks[HB_VIRTUAL_BANK_TRACK_COUNT]);

/* Return 0..3 for logical tracks 5..8, otherwise -1. */
int hb_virtual_bank_index_for_track(int logical_track);

/* Return the configured track for a 0..3 bank index, otherwise NULL. */
const hb_virtual_track_config_t *hb_virtual_bank_track(
    const hb_virtual_track_config_t tracks[HB_VIRTUAL_BANK_TRACK_COUNT],
    int bank_index);

#ifdef __cplusplus
}
#endif

#endif
