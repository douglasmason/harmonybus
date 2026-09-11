#include "hb_virtual_router.h"

hb_virtual_route_decision_t hb_virtual_route_decide(
    const hb_virtual_track_config_t *config,
    const hb_virtual_track_runtime_t *runtime) {
    hb_virtual_route_decision_t decision = {
        .send_to_source_instrument = false,
        .source_audio_audible = false,
        .send_to_harmonybus = false,
        .send_raw_monitor = false,
        .render_channel = -1,
        .monitor_channel = -1,
    };

    if (config == NULL || runtime == NULL) {
        return decision;
    }

    decision.send_to_source_instrument = true;
    decision.source_audio_audible = !runtime->source_audio_muted;
    decision.send_to_harmonybus = runtime->render_enabled;

    if (config->role == HB_VIRTUAL_ROLE_FOLLOWER) {
        decision.render_channel = runtime->render_enabled ? config->render_channel : -1;
        return decision;
    }

    if (config->role == HB_VIRTUAL_ROLE_CONDUCTOR) {
        decision.send_raw_monitor = runtime->render_enabled && config->monitor_channel >= 0;
        decision.monitor_channel = decision.send_raw_monitor ? config->monitor_channel : -1;
    }

    return decision;
}
