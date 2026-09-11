#ifndef HB_VIRTUAL_MIDI_H
#define HB_VIRTUAL_MIDI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Build the four-byte USB-MIDI packet HarmonyBus injects back into Move.
 *
 * The helper is intentionally role-agnostic: followers use transformed pitches,
 * while conductors use the accepted/transposed conductor pitch unchanged. The
 * caller decides which pitch belongs in the packet.
 *
 * Returns 4 on success and -1 for invalid input.
 */
int hb_virtual_make_injected_note_packet(
    int midi_channel,
    int note,
    int velocity,
    int is_note_on,
    uint8_t packet[4]);

#ifdef __cplusplus
}
#endif

#endif
