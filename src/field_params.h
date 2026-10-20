#ifndef HB_FIELD_PARAMS_H
#define HB_FIELD_PARAMS_H
/* Shared field settings; destination mappings remain in existing motion lanes. */
typedef struct {const char *key;int *value;int low,high;} hb_field_param;
static hb_field_param HB_FIELD_PARAMS[]={
 {"field_speed",&g_hb_field.speed,1,256},{"field_direction",&g_hb_field.direction,0,7},
 {"field_depth",&g_hb_field.depth,-100,100},{"field_detail",&g_hb_field.detail,1,5},
 {"field_roughness",&g_hb_field.roughness,0,90},{"field_scale",&g_hb_field.scale,10,800},
 {"field_seed",&g_hb_field.seed,1,65535},{"field_glide",&g_hb_field.glide,0,1000},
 {"field_animation",&g_hb_field.animation,0,1}};
static int hb_field_set(const char *key,const char *value){
    for(unsigned index=0;index<sizeof(HB_FIELD_PARAMS)/sizeof(*HB_FIELD_PARAMS);index++)if(!strcmp(key,HB_FIELD_PARAMS[index].key)){
        char *end=0;long number=strtol(value,&end,10);if(end==value||*end)return 1;
        *HB_FIELD_PARAMS[index].value=(int)hb_nf_bound(number,HB_FIELD_PARAMS[index].low,HB_FIELD_PARAMS[index].high);return 1;
    }
    if(!strcmp(key,"field_gesture")){
        int rate,zoom;char extra;
        if(sscanf(value,"%d,%d%c",&rate,&zoom,&extra)==2&&rate>=-400&&rate<=400&&zoom>=25&&zoom<=400){g_hb_field.target_rate=rate*.01;g_hb_field.target_zoom=zoom*.01;g_hb_field.lease=1.5;}
        return 1;
    }
    if(!strcmp(key,"field_release")){g_hb_field.target_rate=g_hb_field.target_zoom=1;g_hb_field.lease=0;return 1;}
    return 0;
}
static int hb_field_get(const char *key,char *buffer,int length){
    for(unsigned index=0;index<sizeof(HB_FIELD_PARAMS)/sizeof(*HB_FIELD_PARAMS);index++)if(!strcmp(key,HB_FIELD_PARAMS[index].key))return snprintf(buffer,(size_t)length,"%d",*HB_FIELD_PARAMS[index].value);
    if(!strcmp(key,"field_view")){
        int used=snprintf(buffer,(size_t)length,"nf1,%.3f,%.3f",g_hb_field.rate,g_hb_field.zoom);
        for(int index=0;index<64;index++){
            if(used<0||used>=length)return -1;
            used+=snprintf(buffer+used,(size_t)(length-used),",%d",g_hb_field.animation?(int)(hb_nf_sample((index%8-3.5)*.35,(index/8-3.5)*.35,0)*127):64);
        }
        return used<length?used:-1;
    }
    return -1;
}
static int hb_field_save(char *buffer,int length,int used){
    if(g_hb_field.speed==16&&g_hb_field.direction==0&&g_hb_field.depth==0&&g_hb_field.detail==3&&g_hb_field.roughness==50&&g_hb_field.scale==100&&g_hb_field.seed==1&&g_hb_field.glide==100&&g_hb_field.animation==1)return used;
    if(used<0||used>=length)return used;
    return used+snprintf(buffer+used,(size_t)(length-used),";nf1,%d,%d,%d,%d,%d,%d,%d,%d,%d",g_hb_field.speed,g_hb_field.direction,g_hb_field.depth,g_hb_field.detail,g_hb_field.roughness,g_hb_field.scale,g_hb_field.seed,g_hb_field.glide,g_hb_field.animation);
}

static void hb_field_restore(const char *state){
    if(g_hb_field_restored)return;
    const char *cursor=strstr(state,";nf1,");int values[9];char text[32];
    if(cursor&&sscanf(cursor,";nf1,%d,%d,%d,%d,%d,%d,%d,%d,%d",values,values+1,values+2,values+3,values+4,values+5,values+6,values+7,values+8)==9){
        for(int index=0;index<9;index++){snprintf(text,sizeof(text),"%d",values[index]);hb_field_set(HB_FIELD_PARAMS[index].key,text);}g_hb_field_restored=1;
    }
}
#endif
