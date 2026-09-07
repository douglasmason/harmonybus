#ifndef SCHWUNG_MIDI_API_H
#define SCHWUNG_MIDI_API_H
#ifdef HB_FREESTANDING
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
#else
#include <stdint.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

typedef struct host_api_v1 host_api_v1_t;
#define MIDI_FX_API_VERSION 1
#define MIDI_FX_MAX_OUT_MSGS 16

typedef struct midi_fx_api_v1 {
    uint32_t api_version;
    void* (*create_instance)(const char *module_dir, const char *config_json);
    void (*destroy_instance)(void *instance);
    int (*process_midi)(void *instance,
                        const uint8_t *in_msg, int in_len,
                        uint8_t out_msgs[][3], int out_lens[],
                        int max_out);
    int (*tick)(void *instance,
                int frames, int sample_rate,
                uint8_t out_msgs[][3], int out_lens[],
                int max_out);
    void (*set_param)(void *instance, const char *key, const char *val);
    int (*get_param)(void *instance, const char *key, char *buf, int buf_len);
} midi_fx_api_v1_t;

#ifdef __cplusplus
}
#endif
#endif
