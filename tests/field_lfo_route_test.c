#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modules/chain/dsp/chain_internal.h"
#include "host/field_source_runtime.h"
static float get(plugin_api_v2_t *api,void *instance,const char *key){char text[128]={0};assert(api->get_param(instance,key,text,sizeof(text))>0);return strtof(text,0);}
int main(int argc,char **argv){
    assert(argc==3);
    void *handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!handle){puts(dlerror());return 1;}
    plugin_api_v2_t *(*init)(const host_api_v1_t*)=dlsym(handle,"move_plugin_init_v2");assert(init);
    host_api_v1_t host={.api_version=1,.sample_rate=44100,.frames_per_block=128};
    plugin_api_v2_t *api=init(&host);assert(api);
    void *instance=api->create_instance(argv[2],NULL);assert(instance);
    assert(get(api,instance,"lfo1:field_source_version")==1);
    api->set_param(instance,"lfo2:depth","0.2");
    api->set_param(instance,"lfo1:target","lfo2");api->set_param(instance,"lfo1:target_param","depth");
    api->set_param(instance,"lfo1:shape","6");api->set_param(instance,"lfo1:depth","0.5");
    api->set_param(instance,"lfo1:enabled","1");
    float values[16];for(int index=0;index<16;index++)values[index]=1;
    assert(schwung_field_publish_v1(values,values,16));schwung_field_begin_block();
    int16_t pcm[256]={0};api->render_block(instance,pcm,128);
    float base=get(api,instance,"lfo2:depth");
    printf("LFO-to-LFO saved base after Field modulation: %.3f (expected 0.200)\n",base);
    api->set_param(instance,"lfo1:enabled","0");api->render_block(instance,pcm,128);
    float disabled=get(api,instance,"lfo2:depth");
    printf("Destination after disable: %.3f (expected 0.200)\n",disabled);
    assert(fabsf(base-.2f)<1e-6f && fabsf(disabled-.2f)<1e-6f);
    api->set_param(instance,"synth:module","probe");
    api->set_param(instance,"synth:gain","0.2");
    api->set_param(instance,"lfo1:target","synth");api->set_param(instance,"lfo1:target_param","gain");
    api->set_param(instance,"lfo1:polarity","1");api->set_param(instance,"lfo1:enabled","1");
    for(int block=0;block<4;block++)api->render_block(instance,pcm,128);
    float effective=get(api,instance,"synth:gain:effective");
    printf("Synth base %.3f effective %.3f (expected 0.200, 0.450)\n",get(api,instance,"synth:gain"),effective);
    assert(fabsf(get(api,instance,"synth:gain")-.2f)<1e-6f);
    assert(fabsf(effective-.45f)<1e-5f);
    api->set_param(instance,"lfo1:enabled","0");api->render_block(instance,pcm,128);
    assert(fabsf(get(api,instance,"synth:gain:effective")-.2f)<1e-5f);
    chain_instance_t *chain=instance;
    chain->main_send_level[0]=40;
    api->set_param(instance,"lfo1:target","buses");api->set_param(instance,"lfo1:target_param","main_send1");
    api->set_param(instance,"lfo1:enabled","1");api->render_block(instance,pcm,128);
    assert(chain->main_send_level[0]==40 && chain->main_send_mod[0]==32);
    api->set_param(instance,"lfo1:enabled","0");api->render_block(instance,pcm,128);
    assert(chain->main_send_level[0]==40 && chain->main_send_mod[0]==0);
    puts("Field send modulation and disable preserve the send base");
    api->destroy_instance(instance);dlclose(handle);
    return fabsf(base-.2f)<1e-6f&&fabsf(disabled-.2f)<1e-6f?0:1;
}
