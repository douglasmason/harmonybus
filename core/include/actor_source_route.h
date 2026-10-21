#ifndef HB_ACTOR_SOURCE_ROUTE_H
#define HB_ACTOR_SOURCE_ROUTE_H
/* Stateless, portable references to actor content. A source is a track/clip
   identity, not a copy of note data. The host resolves IDs into clip events. */
#include <stdint.h>
typedef struct {
    int16_t track;
    int16_t clip;
} hb_clip_ref_t;
typedef enum {
    HB_SOURCE_OWN_CLIP = 0,
    HB_SOURCE_CLIP_REF = 1,
    HB_SOURCE_CONDUCTOR_ROOT = 2,
    HB_SOURCE_GENERATED_CHORD = 3
} hb_pitch_source_kind_t;
typedef enum {
    HB_RHYTHM_OWN_CLIP = 0,
    HB_RHYTHM_CLIP_REF = 1,
    HB_RHYTHM_GENERATOR = 2
} hb_rhythm_source_kind_t;
typedef struct {
    hb_pitch_source_kind_t pitch_kind;
    hb_rhythm_source_kind_t rhythm_kind;
    hb_clip_ref_t pitch_ref;
    hb_clip_ref_t rhythm_ref;
} hb_actor_source_route_t;
typedef struct {
    hb_clip_ref_t pitch;
    hb_clip_ref_t rhythm;
    hb_pitch_source_kind_t pitch_kind;
    hb_rhythm_source_kind_t rhythm_kind;
    int valid;
} hb_resolved_actor_source_t;
/* Resolve only source identity; do not change playback, clip phase or note
   ownership. Track and clip bounds are supplied by the host (Movy). */
static inline hb_resolved_actor_source_t hb_actor_source_resolve(
    hb_actor_source_route_t route,
    hb_clip_ref_t owning_clip,
    int track_count,
    int clip_count) {
    hb_resolved_actor_source_t result = {0};
    if (track_count <= 0 || clip_count <= 0 ||
        owning_clip.track < 0 || owning_clip.track >= track_count ||
        owning_clip.clip < 0 || owning_clip.clip >= clip_count ||
        route.pitch_kind < HB_SOURCE_OWN_CLIP ||
        route.pitch_kind > HB_SOURCE_GENERATED_CHORD ||
        route.rhythm_kind < HB_RHYTHM_OWN_CLIP ||
        route.rhythm_kind > HB_RHYTHM_GENERATOR) return result;
    result.pitch_kind = route.pitch_kind;
    result.rhythm_kind = route.rhythm_kind;
    result.pitch = owning_clip;
    result.rhythm = owning_clip;
    if (route.pitch_kind == HB_SOURCE_CLIP_REF) {
        if (route.pitch_ref.track < 0 || route.pitch_ref.track >= track_count ||
            route.pitch_ref.clip < 0 || route.pitch_ref.clip >= clip_count) return result;
        result.pitch = route.pitch_ref;
    }
    if (route.rhythm_kind == HB_RHYTHM_CLIP_REF) {
        if (route.rhythm_ref.track < 0 || route.rhythm_ref.track >= track_count ||
            route.rhythm_ref.clip < 0 || route.rhythm_ref.clip >= clip_count) return result;
        result.rhythm = route.rhythm_ref;
    }
    result.valid = 1;
    return result;
}
#endif
