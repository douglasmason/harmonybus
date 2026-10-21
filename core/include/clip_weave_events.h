#ifndef HB_CLIP_WEAVE_EVENTS_H
#define HB_CLIP_WEAVE_EVENTS_H
/* Pure clip-event weaving. All times use the host's integer tick clock.
   Sources retain their own periods and origins. No allocation or MIDI I/O. */
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t onset; uint32_t duration; uint8_t note; uint8_t velocity; uint8_t active; } hb_weave_event_t;
typedef struct { const hb_weave_event_t *events; uint32_t count; uint32_t period; uint64_t origin; } hb_weave_stream_t;
typedef struct { uint64_t tick; uint32_t duration; uint8_t note; uint8_t velocity; } hb_woven_note_t;
/* Source events are ordered by onset. Rests are active=0, so they do not
   consume pitch material. Returns false for invalid sources or non-attacks. */
static inline int hb_weave_resolve_attack(hb_weave_stream_t pitch,
                                         hb_weave_stream_t rhythm,
                                         uint64_t rhythm_tick,
                                         hb_woven_note_t *output) {
    if (!output || !pitch.events || !rhythm.events || !pitch.count || !rhythm.count ||
        !pitch.period || !rhythm.period || rhythm_tick < rhythm.origin) return 0;
    uint64_t offset = rhythm_tick - rhythm.origin;
    uint32_t step = (uint32_t)(offset % rhythm.period);
    uint64_t loops = offset / rhythm.period;
    uint32_t attacks_per_loop = 0;
    uint32_t attacks_before = 0;
    const hb_weave_event_t *selected = NULL;
    uint32_t previous_onset = 0;
    for (uint32_t index = 0; index < rhythm.count; ++index) {
        const hb_weave_event_t *event = &rhythm.events[index];
        if (event->onset >= rhythm.period || (index && event->onset < previous_onset)) return 0;
        previous_onset = event->onset;
        if (!event->active) continue;
        if (event->onset < step) ++attacks_before;
        if (event->onset == step && !selected) selected = event;
        ++attacks_per_loop;
    }
    if (!selected || !attacks_per_loop || loops > (UINT64_MAX - attacks_before) / attacks_per_loop) return 0;
    uint64_t ordinal = loops * attacks_per_loop + attacks_before;
    uint32_t pitch_count = 0;
    previous_onset = 0;
    for (uint32_t index = 0; index < pitch.count; ++index) {
        const hb_weave_event_t *event = &pitch.events[index];
        if (event->onset >= pitch.period || (index && event->onset < previous_onset)) return 0;
        previous_onset = event->onset;
        pitch_count += event->active ? 1u : 0u;
    }
    if (!pitch_count) return 0;
    uint32_t pitch_index = (uint32_t)(ordinal % pitch_count);
    for (uint32_t index = 0; index < pitch.count; ++index) {
        const hb_weave_event_t *event = &pitch.events[index];
        if (event->active && pitch_index-- == 0) {
            *output = (hb_woven_note_t){rhythm_tick, selected->duration, event->note, selected->velocity};
            return 1;
        }
    }
    return 0;
}
#endif
