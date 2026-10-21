#include "host/plugin_api_v1.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static void *create(const char *dir,const char *config){(void)dir;(void)config;return calloc(1,sizeof(float));}
static void destroy(void *instance){free(instance);}
static void set(void *instance,const char *key,const char *value){if(!strcmp(key,"gain"))*(float*)instance=strtof(value,0);}
static int get(void *instance,const char *key,char *text,int size){
    if(!strcmp(key,"gain"))return snprintf(text,size,"%.6f",*(float*)instance);
    if(!strcmp(key,"chain_params"))return snprintf(text,size,"[{\"key\":\"gain\",\"type\":\"float\",\"min\":0,\"max\":1}]");
    return 0;
}
static void render(void *instance,int16_t *output,int frames){(void)instance;memset(output,0,frames*2*sizeof(*output));}
static plugin_api_v2_t api={.api_version=2,.create_instance=create,.destroy_instance=destroy,.set_param=set,.get_param=get,.render_block=render};
plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host){(void)host;return &api;}
