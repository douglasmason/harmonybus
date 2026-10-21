#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "host/shadow_chain_mgmt.c"
static float effective_gain=.2f;
static void probe_set(void *instance,const char *key,const char *value){(void)instance;if(!strcmp(key,"gain"))effective_gain=strtof(value,0);}
static int probe_get(void *instance,const char *key,char *value,int size){(void)instance;if(!strcmp(key,"gain"))return snprintf(value,size,"%.6f",effective_gain);return 0;}
int main(void){
    float values[16];for(int index=0;index<16;index++)values[index]=1;
    assert(schwung_field_publish_v1(values,values,16));schwung_field_begin_block();
    audio_fx_api_v2_t probe={.set_param=probe_set,.get_param=probe_get};
    shadow_master_fx_slots[0].api=&probe;
    shadow_master_fx_slots[0].instance=&effective_gain;
    shadow_master_fx_slots[0].chain_params_cached=1;
    shadow_master_fx_slots[0].chain_params_cache="[{\"key\":\"gain\",\"type\":\"float\",\"min\":0,\"max\":1}]";
    lfo_state_t *source=&shadow_master_fx_lfos[0];
    source->enabled=1;source->shape=6;source->depth=.5f;source->bipolar=1;
    strcpy(source->target,"fx1");strcpy(source->param,"gain");
    shadow_master_fx_lfo_tick(128);assert(fabsf(effective_gain-.45f)<1e-6f);
    source->enabled=0;shadow_master_fx_lfo_tick(128);assert(fabsf(effective_gain-.2f)<1e-6f);
    source->enabled=1;shadow_master_fx_lfo_tick(128);assert(fabsf(effective_gain-.45f)<1e-6f);
    assert(schwung_field_publish_v1(values,0,0));
    shadow_master_fx_lfo_tick(128);assert(fabsf(effective_gain-.2f)<1e-6f);
    strcpy(source->target,"lfo2");strcpy(source->param,"depth");
    shadow_master_fx_lfos[1].depth=.2f;
    assert(schwung_field_publish_v1(values,values,16));schwung_field_begin_block();
    shadow_master_fx_lfo_tick(128);shadow_master_fx_lfo_tick(128);
    assert(shadow_master_fx_lfos[1].depth==.2f);
    puts("master Field FX signal, missing-source restoration and cross-LFO base tests passed");
}
