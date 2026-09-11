#ifndef HB_VIRTUAL_ROUTER_H
#define HB_VIRTUAL_ROUTER_H

#include <stdbool.h>

#include "hb_virtual_bank.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool source_audio_muted;
    bool render_enabled;
} hb_virtual_track_runtime_t;

typedef struct {
    bool send_to_source_instrument;
    bool source_audio_audible;
    bool send_to_harmonybus;
    bool send_raw_monitor;
    int render_channel;
    int monitor_channel;
} hb_virtual_route_decision_t;

/*
 * Decide how one MIDI event from a virtual source track is fanned out.
 *
 * The source instrument always receives MIDI so its voice state remains
 * correct; source_audio_muted controls only whether that local audio is heard.
 * Muting source audio MUST NOT suppress HarmonyBus processing.
 *
 * Followers send to HarmonyBus when render_enabled is true. The existing
 * HarmonyBus render_channel setting determines the native Move destination.
 * Conductors publish harmony in the same way and may additionally send a raw
 * monitor copy to monitor_channel (default user MIDI channel 3).
 */
hb_virtual_route_decision_t hb_virtual_route_decide(
    const hb_virtual_track_config_t *config,
    const hb_virtual_track_runtime_t *runtime);

#ifdef __cplusplus
}
#endif

#endif
