/* Load the complete shared library with the same eager binding as the host. */
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include "../modules/harmonybus/dsp/schwung_midi_api.h"

int main(int argument_count, char **arguments) {
    assert(argument_count == 2);
    void *library = dlopen(arguments[1], RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        fprintf(stderr, "HarmonyBus cannot load: %s\n", dlerror());
        return 1;
    }
    midi_fx_api_v1_t *(*initialize)(const host_api_v1_t *) =
        (midi_fx_api_v1_t *(*)(const host_api_v1_t *))dlsym(library, "move_midi_fx_init");
    assert(initialize);
    midi_fx_api_v1_t *api = initialize(NULL);
    assert(api && api->api_version == MIDI_FX_API_VERSION);
    const char *states[] = {
        "hb16,1,0,0,25,2,0,0,0,0,0,0,3,0,0,0,0,0,0,0,20,60,0,0,0,3",
        "hb15,1,0,0,25,2,0,0,0,0,0,0,3,0,0,0,0,0,0,0,20,60,0,0,0,3",
    };
    for (int variant = 0; variant < 2; variant++) {
        void *instance = api->create_instance("", NULL);
        assert(instance);
        api->set_param(instance, "state", states[variant]);
        char value[512];
        assert(api->get_param(instance, "next_lookahead", value, sizeof(value)) > 0);
        assert(strcmp(value, "Off") == 0);
        api->set_param(instance, "next_lookahead", "1/8");
        assert(api->get_param(instance, "next_lookahead", value, sizeof(value)) > 0);
        assert(strcmp(value, "1/8") == 0);
        api->set_param(instance, "next_lookahead", "Off");
        assert(api->get_param(instance, "role", value, sizeof(value)) > 0);
        assert(strcmp(value, "Follower") == 0);
        assert(api->get_param(instance, "render_channel", value, sizeof(value)) > 0);
        assert(strcmp(value, "4") == 0);
        assert(api->get_param(instance, "source_channel", value, sizeof(value)) > 0);
        assert(strcmp(value, "1") == 0);
        assert(api->get_param(instance, "quant_timing", value, sizeof(value)) > 0);
        assert(strcmp(value, "1/4") == 0);
        assert(api->get_param(instance, "state", value, sizeof(value)) > 0);
        assert(strncmp(value, "hb16,1,", 7) == 0);
        api->destroy_instance(instance);
    }
    dlclose(library);
    puts("HarmonyBus loads with RTLD_NOW and restores hb16/hb15 presets");
    return 0;
}
