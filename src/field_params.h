#ifndef HB_FIELD_PARAMS_H
#define HB_FIELD_PARAMS_H
/* Shared field settings; destination mappings remain in existing motion lanes. */
typedef struct {const char *key;int *value;int low,high;} hb_field_param;
static hb_field_param HB_FIELD_PARAMS[]={
 {"field_speed",&g_hb_field.speed,1,256},{"field_direction",&g_hb_field.direction,0,7},
 {"field_depth",&g_hb_field.depth,-100,100},{"field_detail",&g_hb_field.detail,1,5},
 {"field_roughness",&g_hb_field.roughness,0,90},{"field_scale",&g_hb_field.scale,10,800},
 {"field_seed",&g_hb_field.seed,1,65535},{"field_glide",&g_hb_field.glide,0,1000},
 {"field_animation",&g_hb_field.animation,0,1},
 {"field_display",&g_hb_field_view,0,1},{"field_orientation",&g_hb_field_orientation,0,1},
 {"field_bank",&g_hb_field_bank,0,3},{"field_override",&g_hb_field_latch_mode,0,1}};
static int hb_field_set(const char *key,const char *value){
    for(unsigned index=0;index<sizeof(HB_FIELD_PARAMS)/sizeof(*HB_FIELD_PARAMS);index++)if(!strcmp(key,HB_FIELD_PARAMS[index].key)){
        char *end=0;long number=strtol(value,&end,10);if(end==value||*end)return 1;
        *HB_FIELD_PARAMS[index].value=(int)hb_nf_bound(number,HB_FIELD_PARAMS[index].low,HB_FIELD_PARAMS[index].high);
        if(!strcmp(key,"field_override")&&!g_hb_field_latch_mode)g_hb_field_latched=0;
        return 1;
    }
    if(!strcmp(key,"field_output_clear")){
        char *end;long lane=strtol(value,&end,10);
        if(end!=value&&!*end&&lane>=0&&lane<=16)hb_nf_clear_outputs((int)lane);
        return 1;
    }
    if(!strcmp(key,"field_output_finish")){
        int source,lane,target;char extra;
        if(sscanf(value,"%d,%d,%d%c",&source,&lane,&target,&extra)!=3||source<0||source>1||lane<0||lane>=16||target<0||target>10000)return 1;
        hb_field_input *input=&g_hb_field_inputs[source][lane];
        /* Only an explicit release of a still-valid hold can create a latch. */
        if(input->lease>0&&!input->blocked&&g_hb_field_latch_mode){g_hb_field_latched|=1u<<lane;g_hb_field_latch_values[lane]=target*.0001;}
        input->lease=0;return 1;
    }
    if(!strcmp(key,"field_outputs")||!strcmp(key,"field_outputs_remote")){
        int source=!strcmp(key,"field_outputs_remote");
        /* Validate the whole frame before mutating any output. */
        if(strncmp(value,"fo1",3))return 1;
        const char *cursor=value+3;long values[16];
        for(int index=0;index<16;index++){
            if(*cursor++!=',')return 1;
            char *end;values[index]=strtol(cursor,&end,10);
            if(end==cursor||values[index]<-1||values[index]>10000)return 1;
            cursor=end;
        }
        if(*cursor)return 1;
        for(int index=0;index<16;index++){
            hb_field_input *output=&g_hb_field_inputs[source][index];
            if(values[index]<0){output->lease=0;output->blocked=0;continue;}
            if(output->blocked)continue;
            if(output->lease<=0)g_hb_field_latched&=~(1u<<index);
            double target=values[index]*.0001;
            if(output->lease<=0||output->target!=target)output->order=++g_hb_field_input_order;
            output->target=target;
            output->lease=1.5;
        }return 1;
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
    if(used<0||used>=length)return used;
    if(g_hb_field_latch_mode)used+=snprintf(buffer+used,(size_t)(length-used),";nl1,1");
    if(used<0||used>=length)return used;
    if(g_hb_field_view||g_hb_field_orientation||g_hb_field_bank)used+=snprintf(buffer+used,(size_t)(length-used),";nv1,%d,%d,%d",g_hb_field_view,g_hb_field_orientation,g_hb_field_bank);
    if(g_hb_field.speed==16&&g_hb_field.direction==0&&g_hb_field.depth==0&&g_hb_field.detail==3&&g_hb_field.roughness==50&&g_hb_field.scale==100&&g_hb_field.seed==1&&g_hb_field.glide==100&&g_hb_field.animation==1)return used;
    if(used<0||used>=length)return used;
    return used+snprintf(buffer+used,(size_t)(length-used),";nf1,%d,%d,%d,%d,%d,%d,%d,%d,%d",g_hb_field.speed,g_hb_field.direction,g_hb_field.depth,g_hb_field.detail,g_hb_field.roughness,g_hb_field.scale,g_hb_field.seed,g_hb_field.glide,g_hb_field.animation);
}

static void hb_field_restore(const char *state){
    hb_nf_clear_outputs(16);
    if(g_hb_field_restored)return;
    g_hb_field_latch_mode=strstr(state,";nl1,1")!=0;
    const char *view=strstr(state,";nv1,");int display,orientation,bank;
    if(view&&sscanf(view,";nv1,%d,%d,%d",&display,&orientation,&bank)==3){
        g_hb_field_view=(int)hb_nf_bound(display,0,1);g_hb_field_orientation=(int)hb_nf_bound(orientation,0,1);g_hb_field_bank=(int)hb_nf_bound(bank,0,3);
    }
    const char *cursor=strstr(state,";nf1,");int values[9];char text[32];
    if(cursor&&sscanf(cursor,";nf1,%d,%d,%d,%d,%d,%d,%d,%d,%d",values,values+1,values+2,values+3,values+4,values+5,values+6,values+7,values+8)==9){
        for(int index=0;index<9;index++){snprintf(text,sizeof(text),"%d",values[index]);hb_field_set(HB_FIELD_PARAMS[index].key,text);}g_hb_field_restored=1;
    }
    if(view||g_hb_field_latch_mode)g_hb_field_restored=1;
}
#endif
