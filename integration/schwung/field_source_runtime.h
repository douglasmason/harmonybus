/* Include in exactly one host translation unit. Values, never plugin pointers,
 * cross this boundary. Ownership is an opaque identity, never dereferenced.
 * Single-word atomic samples keep readers wait-free, even during publication.
 * Each word carries its own expiry timestamp; no torn value/time pairs. */
#ifndef SCHWUNG_FIELD_SOURCE_RUNTIME_H
#define SCHWUNG_FIELD_SOURCE_RUNTIME_H
#include "field_source.h"
#include <stdint.h>
#include <time.h>
#include <math.h>
static unsigned long long field_epoch;
__attribute__((visibility("default")))
unsigned long long schwung_field_clock_v1(void) { return __atomic_load_n(&field_epoch, __ATOMIC_RELAXED); }
static unsigned field_gate;
static const void *field_owner;
static uint64_t field_samples[SCHWUNG_FIELD_CHANNELS];
static uint64_t field_pending[SCHWUNG_FIELD_CHANNELS];
/* Commit once before any chain renders. Every assignment sees the same sample
 * during a block, independent of the HB instance's position in the render order. */
void schwung_field_begin_block(void) {
    for (int channel=0; channel<SCHWUNG_FIELD_CHANNELS; channel++)
        __atomic_store_n(&field_samples[channel], __atomic_load_n(&field_pending[channel], __ATOMIC_ACQUIRE), __ATOMIC_RELEASE);
    __atomic_add_fetch(&field_epoch, 1, __ATOMIC_RELAXED);
}
_Static_assert(__atomic_always_lock_free(8, 0), "Field samples require lock-free 64-bit atomics");
static uint64_t field_stamp_ms;
static uint64_t field_now_ms(void) {
    struct timespec stamp;
    clock_gettime(CLOCK_MONOTONIC, &stamp);
    return (uint64_t)stamp.tv_sec * 1000 + (uint64_t)stamp.tv_nsec / 1000000;
}
__attribute__((visibility("default")))
int schwung_field_publish_v1(const void *owner, const float *values, int count) {
    if (!owner || (count != 0 && (count != SCHWUNG_FIELD_CHANNELS || !values))) return 0;
    if (__atomic_exchange_n(&field_gate, 1, __ATOMIC_ACQUIRE)) return 0;
    int accepted = !field_owner || field_owner == owner ||
        (count && field_now_ms() - field_stamp_ms > 250);
    if (accepted) {
        if (!count) {
            field_owner = 0; field_stamp_ms = 0;
            for (int channel=0; channel<SCHWUNG_FIELD_CHANNELS; channel++) {
                __atomic_store_n(&field_pending[channel], 0, __ATOMIC_RELEASE);
                __atomic_store_n(&field_samples[channel], 0, __ATOMIC_RELEASE);
            }
        }
        else {
            field_owner = owner;
            field_stamp_ms = field_now_ms();
            for (int channel = 0; channel < SCHWUNG_FIELD_CHANNELS; channel++) {
                float value = values[channel];
                union { float value; uint32_t bits; } sample;
                sample.value = !isfinite(value) ? 0.5f : fminf(1.0f, fmaxf(0.0f, value));
                uint64_t packed = ((uint64_t)(uint32_t)field_stamp_ms << 32) | sample.bits;
                __atomic_store_n(&field_pending[channel], packed, __ATOMIC_RELEASE);
            }
        }
    }
    __atomic_store_n(&field_gate, 0, __ATOMIC_RELEASE);
    return accepted;
}
__attribute__((visibility("default")))
int schwung_field_read_v1(int channel, float *value) {
    if (!value || channel < 0 || channel >= SCHWUNG_FIELD_CHANNELS) return 0;
    uint64_t packed=__atomic_load_n(&field_samples[channel], __ATOMIC_ACQUIRE);
    if (!packed || (uint32_t)((uint32_t)field_now_ms()-(uint32_t)(packed>>32))>250) return 0;
    union { float value; uint32_t bits; } sample;
    sample.bits=(uint32_t)packed;
    *value=sample.value*2.0f-1.0f;
    return 1;
}
#endif
