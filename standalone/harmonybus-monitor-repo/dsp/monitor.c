/*
 * HarmonyBus Monitor — source-aware Move MIDI_OUT cable-2 monitor.
 * Based on the ChordDex monitor architecture (MIT).
 *
 * Receives Move's played notes via plugin_api_v2.on_midi(..., source), accepts
 * only MOVE_MIDI_SOURCE_EXTERNAL, and publishes per-channel active-note state
 * to a POSIX shared-memory block consumed by the HarmonyBus MIDI FX.
 */
#include <stdint.h>
#include <stddef.h>

#define MOVE_PLUGIN_API_VERSION_2 2
#define MOVE_MIDI_SOURCE_EXTERNAL 2
#define HB_MONITOR_MAGIC 0x48424d31u
#define HB_MONITOR_VERSION 1u
#define HB_MONITOR_SHM "/harmonybus-monitor-v1"

#define O_RDWR 2
#define O_CREAT 64
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define MAP_FAILED ((void *)-1)

typedef struct {
    uint32_t magic;
    uint32_t version;
    volatile uint32_t seq;
    volatile uint32_t generation;
    volatile uint32_t external_note_events;
    volatile uint32_t realtime_events;
    volatile uint8_t playing;
    volatile uint8_t velocities[16][128];
} hb_monitor_shared_t;

typedef struct host_api_v1 host_api_v1_t;
typedef struct {
    uint32_t api_version;
    void *(*create_instance)(const char *, const char *);
    void (*destroy_instance)(void *);
    void (*on_midi)(void *, const uint8_t *, int, int);
    void (*set_param)(void *, const char *, const char *);
    int (*get_param)(void *, const char *, char *, int);
    int (*get_error)(void *, char *, int);
    void (*render_block)(void *, int16_t *, int);
} plugin_api_v2_t;

extern int shm_open(const char *name, int oflag, unsigned mode);
extern int ftruncate(int fd, long length);
extern void *mmap(void *addr, unsigned long length, int prot, int flags, int fd, long offset);
extern int close(int fd);
extern int snprintf(char *buffer, unsigned long size, const char *format, ...);
extern void *memset(void *s, int c, unsigned long n);
extern int strcmp(const char *a, const char *b);

typedef struct {
    hb_monitor_shared_t *shared;
} monitor_instance_t;

static hb_monitor_shared_t *open_shared(void) {
    int fd = shm_open(HB_MONITOR_SHM, O_CREAT | O_RDWR, 0666);
    if (fd < 0) return 0;
    if (ftruncate(fd, (long)sizeof(hb_monitor_shared_t)) != 0) {
        close(fd);
        return 0;
    }
    void *ptr = mmap(0, sizeof(hb_monitor_shared_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (ptr == MAP_FAILED) return 0;
    hb_monitor_shared_t *shared = (hb_monitor_shared_t *)ptr;
    if (shared->magic != HB_MONITOR_MAGIC || shared->version != HB_MONITOR_VERSION) {
        memset(shared, 0, sizeof(*shared));
        shared->magic = HB_MONITOR_MAGIC;
        shared->version = HB_MONITOR_VERSION;
    }
    return shared;
}

static void clear_all(hb_monitor_shared_t *shared) {
    if (!shared) return;
    shared->seq++;
    memset((void *)shared->velocities, 0, sizeof(shared->velocities));
    shared->generation++;
    shared->seq++;
}

static void *create_instance(const char *module_dir, const char *json_defaults) {
    (void)module_dir;
    (void)json_defaults;
    static monitor_instance_t instance;
    instance.shared = open_shared();
    return &instance;
}

static void destroy_instance(void *value) {
    (void)value;
}

static void on_midi(void *value, const uint8_t *msg, int len, int source) {
    monitor_instance_t *instance = (monitor_instance_t *)value;
    hb_monitor_shared_t *shared = instance ? instance->shared : 0;
    if (!shared || !msg || len < 1) return;

    if (len == 1) {
        shared->realtime_events++;
        if (msg[0] == 0xFA || msg[0] == 0xFB) {
            shared->playing = 1;
        } else if (msg[0] == 0xFC) {
            shared->playing = 0;
            clear_all(shared);
        }
        return;
    }

    if (source != MOVE_MIDI_SOURCE_EXTERNAL || len < 3) return;

    uint8_t type = msg[0] & 0xF0;
    if (type != 0x80 && type != 0x90 && type != 0xB0) return;
    uint8_t channel = msg[0] & 0x0F;
    uint8_t data1 = msg[1];
    uint8_t data2 = msg[2];

    if (type == 0xB0 && (data1 == 120 || data1 == 123)) {
        shared->seq++;
        memset((void *)shared->velocities[channel], 0, 128);
        shared->generation++;
        shared->seq++;
        return;
    }

    if (data1 > 127) return;
    shared->external_note_events++;
    shared->seq++;
    if (type == 0x90 && data2 > 0) {
        shared->velocities[channel][data1] = data2;
    } else {
        shared->velocities[channel][data1] = 0;
    }
    shared->generation++;
    shared->seq++;
}

static void set_param(void *value, const char *key, const char *val) {
    (void)val;
    monitor_instance_t *instance = (monitor_instance_t *)value;
    if (instance && instance->shared && key && strcmp(key, "clear") == 0) clear_all(instance->shared);
}

static int get_param(void *value, const char *key, char *buffer, int length) {
    monitor_instance_t *instance = (monitor_instance_t *)value;
    hb_monitor_shared_t *shared = instance ? instance->shared : 0;
    if (!key || !buffer || length < 2) return -1;
    if (strcmp(key, "ping") == 0) return snprintf(buffer, (unsigned long)length, "pong hbmon1");
    if (!shared) return snprintf(buffer, (unsigned long)length, "absent");
    if (strcmp(key, "status") == 0) {
        return snprintf(buffer, (unsigned long)length, "g%u e%u rt%u p%u",
                        (unsigned)shared->generation,
                        (unsigned)shared->external_note_events,
                        (unsigned)shared->realtime_events,
                        (unsigned)shared->playing);
    }
    return -1;
}

static int get_error(void *value, char *buffer, int length) {
    (void)value;
    if (buffer && length > 0) buffer[0] = 0;
    return 0;
}

static void render_block(void *value, int16_t *out, int frames) {
    (void)value; (void)out; (void)frames;
}

static plugin_api_v2_t API = {
    MOVE_PLUGIN_API_VERSION_2,
    create_instance,
    destroy_instance,
    on_midi,
    set_param,
    get_param,
    get_error,
    render_block
};

__attribute__((visibility("default")))
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host) {
    (void)host;
    return &API;
}
