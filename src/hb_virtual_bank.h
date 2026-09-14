#ifndef HB_VIRTUAL_BANK_H
#define HB_VIRTUAL_BANK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HB_VIRTUAL_ROLE_CONDUCTOR = 0,
    HB_VIRTUAL_ROLE_FOLLOWER = 1
} hb_virtual_role_t;

typedef struct {
    int logical_track;
    hb_virtual_role_t role;
    int render_channel;
    int monitor_channel;
} hb_virtual_track_config_t;

#define HB_VIRTUAL_BANK_TRACK_COUNT 4
#define HB_VIRTUAL_BANK_FIRST_TRACK 5
#define HB_VIRTUAL_BANK_LAST_TRACK 8

void hb_virtual_bank_default(hb_virtual_track_config_t out_tracks[HB_VIRTUAL_BANK_TRACK_COUNT]);
int hb_virtual_bank_index_for_track(int logical_track);
const hb_virtual_track_config_t *hb_virtual_bank_track(
    const hb_virtual_track_config_t tracks[HB_VIRTUAL_BANK_TRACK_COUNT],
    int bank_index);
int hb_virtual_bank_make_harmonybus_state(
    const hb_virtual_track_config_t *track,
    char *buffer,
    size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif
