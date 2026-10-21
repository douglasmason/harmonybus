/* Real, unmodified Schwung chain + actual HB DSP. Only the synth is a probe. */
#include <assert.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "modules/chain/dsp/chain_internal.h"
static float get(plugin_api_v2_t *api,void *instance,const char *key){char text[128]={0};if(api->get_param(instance,key,text,sizeof(text))<=0){fprintf(stderr,"missing parameter %s\n",key);abort();}return strtof(text,0);}
static void tick(plugin_api_v2_t *api,void *instance){static unsigned long long block;int16_t pcm[256]={0};for(int i=0;i<8;i++){
    char message[96];snprintf(message,sizeof(message),"%llu,128,44100,%d",++block,(int)get(api,instance,"midi_fx1:bypassed"));
    api->set_param(instance,"midi_fx1:hb_movy_block",message);api->render_block(instance,pcm,128);
}}
static void route(plugin_api_v2_t *api,void *instance,const char *value){
    if(!strcmp(value,"off")){api->set_param(instance,"midi_fx1:field_lfo1:enabled","0");return;}
    int channel,depth,bipolar;char target[32],param[64],v[32];
    assert(sscanf(value,"%d,%d,%d,%31[^,],%63s",&channel,&depth,&bipolar,target,param)==5);
    snprintf(v,sizeof(v),"%d",channel+5);api->set_param(instance,"midi_fx1:field_lfo1:shape",v);
    snprintf(v,sizeof(v),"%.4f",depth*.0001);api->set_param(instance,"midi_fx1:field_lfo1:depth",v);
    snprintf(v,sizeof(v),"%d",bipolar);api->set_param(instance,"midi_fx1:field_lfo1:polarity",v);
    api->set_param(instance,"midi_fx1:field_lfo1:target",target);api->set_param(instance,"midi_fx1:field_lfo1:target_param",param);
    api->set_param(instance,"midi_fx1:field_lfo1:enabled","1");
}
static void near(float a,float b){if(fabsf(a-b)>1e-5f){fprintf(stderr,"actual %f expected %f\n",a,b);abort();}}
int main(int argc,char **argv){
    assert(argc==3);void *handle=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!handle){puts(dlerror());return 1;}
    plugin_api_v2_t *(*init)(const host_api_v1_t*)=dlsym(handle,"move_plugin_init_v2");assert(init);
    host_api_v1_t host={.api_version=1,.sample_rate=44100,.frames_per_block=128};
    plugin_api_v2_t *api=init(&host);assert(api);
    void *a=api->create_instance(argv[2],NULL),*b=api->create_instance(argv[2],NULL);assert(a&&b);
    api->set_param(a,"synth:module","probe");api->set_param(b,"synth:module","probe");
    api->set_param(a,"midi_fx1:module","harmonybus");api->set_param(b,"midi_fx1:module","harmonybus");
    tick(api,a);tick(api,b);
    chain_instance_t *ca=a,*cb=b;assert(ca->midi_fx_instances[0]&&cb->midi_fx_instances[0]);
    api->set_param(a,"synth:gain","0.2");api->set_param(b,"synth:gain","0.3");
    api->set_param(a,"midi_fx1:field_glide","0");
    api->set_param(a,"midi_fx1:field_outputs","fo1,10000,0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1");
    route(api,a,"1,5000,1,synth,gain");tick(api,a);tick(api,b);
    near(get(api,a,"synth:gain"),.2f);near(get(api,a,"synth:gain:effective"),.45f);
    near(get(api,b,"synth:gain:effective"),.3f);
    puts("PASS: Field override, bipolar depth, base preservation, chain ownership");
    route(api,a,"2,5000,0,synth,gain");tick(api,a);
    near(get(api,a,"synth:gain:effective"),.2f);
    route(api,a,"1,5000,0,synth,gain");tick(api,a);
    near(get(api,a,"synth:gain:effective"),.7f);
    route(api,a,"off");tick(api,a);
    near(get(api,a,"synth:gain:effective"),.2f);
    puts("PASS: channel selection, unipolar range and explicit disable restore");
    route(api,a,"1,5000,1,synth,gain");tick(api,a);
    api->set_param(a,"synth:gain","0.4");tick(api,a);
    near(get(api,a,"synth:gain"),.4f);near(get(api,a,"synth:gain:effective"),.65f);
    route(api,a,"1,5000,1,synth,missing");tick(api,a);
    near(get(api,a,"synth:gain:effective"),.4f);
    puts("PASS: base edits during modulation and invalid destination cleanup");
    route(api,a,"1,5000,1,synth,gain");tick(api,a);
    api->set_param(a,"midi_fx1:bypassed","1");tick(api,a);
    near(get(api,a,"synth:gain:effective"),.4f);
    puts("PASS: managed bypass restores base without changing Schwung");
    route(api,a,"off");tick(api,a);
    near(get(api,a,"synth:gain:effective"),.4f);
    api->set_param(a,"midi_fx1:bypassed","0");
    route(api,a,"1,5000,1,synth,gain");tick(api,a);
    api->set_param(a,"midi_fx1:module","none");tick(api,a);
    near(get(api,a,"synth:gain:effective"),.4f);
    near(get(api,b,"synth:gain:effective"),.3f);
    puts("PASS: explicit disable while bypassed and module unload restore base");
    api->set_param(b,"fx1:module","probe");tick(api,b);
    api->set_param(b,"fx1:gain","0.2");
    route(api,b,"1,5000,1,fx1,gain");tick(api,b);
    near(get(api,b,"fx1:gain"),.2f);near(get(api,b,"fx1:gain:effective"),.45f);
    /* A separate source must survive removal of the Field contribution. */
    assert(cb->subplugin_host_api.mod_emit_value(cb,"other", "fx1","gain",1,.2f,0,1,1)==0);
    tick(api,b);near(get(api,b,"fx1:gain:effective"),.55f);
    route(api,b,"off");tick(api,b);
    near(get(api,b,"fx1:gain:effective"),.3f);
    cb->subplugin_host_api.mod_clear_source(cb,"other");tick(api,b);
    near(get(api,b,"fx1:gain:effective"),.2f);
    puts("PASS: audio FX routing and independent contributions survive Field disable");
    api->destroy_instance(a);api->destroy_instance(b);
    /* Stock chain caches module-staging scratch at library scope. Keep the
       library loaded until exit, as the host does, so LSan sees those roots. */
    return 0;
}
