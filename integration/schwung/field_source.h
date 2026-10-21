/* Optional process-wide field source. No host_api_v1_t ABI changes. */
#ifndef SCHWUNG_FIELD_SOURCE_H
#define SCHWUNG_FIELD_SOURCE_H
#define SCHWUNG_FIELD_CHANNELS 16
#define LFO_FIELD_FIRST 6
#define LFO_FIELD_END (LFO_FIELD_FIRST + SCHWUNG_FIELD_CHANNELS)
typedef int (*schwung_field_publish_fn)(const void *owner, const float *values, int count);
typedef int (*schwung_field_read_fn)(int channel, float *value);
void schwung_field_begin_block(void);
typedef unsigned long long (*schwung_field_clock_fn)(void);
#endif
