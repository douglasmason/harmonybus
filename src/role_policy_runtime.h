/* Included after Inst: configuration values are resolved before each onset. */
static int g_role_fallback[2][HB_POLICY_FIELDS];
static int g_role_ready=0,g_role_restored=0;
static void hb_role_init(void){
    if(g_role_ready)return;
    for(int role=0;role<2;role++)memcpy(g_role_fallback[role],HB_POLICY_DEFAULTS,sizeof(HB_POLICY_DEFAULTS));
    g_role_ready=1;
}
static int hb_role_default(int role,int field){
    hb_role_init();hb_global_open();
    return g_global_shared&&g_global_shared->role_ready?g_global_shared->role_values[role][field]:g_role_fallback[role][field];
}
static void hb_role_store(int role,int field,int value){
    hb_role_init();hb_global_open();
    if(g_global_shared&&!g_global_shared->role_ready){
        for(int group=0;group<2;group++)for(int index=0;index<HB_POLICY_FIELDS;index++)g_global_shared->role_values[group][index]=g_role_fallback[group][index];
        g_global_shared->role_ready=1;
    }
    g_role_fallback[role][field]=value;
    if(g_global_shared){g_global_shared->seq++;g_global_shared->role_values[role][field]=value;g_global_shared->seq++;}
}
static int *hb_policy_chord_field(Inst *instance,int field){
    hb_cp_config *config=&instance->player.config;
    switch(field){case HB_P_FORM:return &config->size;case HB_P_QUALITY:return &config->quality;
    case HB_P_INVERSION:return &config->inversion;case HB_P_VOICING:return &config->voicing;
    case HB_P_SPREAD:return &config->spread;case HB_P_CHROMATIC:return &config->chromatic_quality;default:return 0;}
}
static int hb_policy_value(Inst *instance,int field){
    if(instance&&(instance->policy_overrides&(1u<<field)))return instance->policy_values[field];
    return hb_role_default(instance&&instance->role==0?0:1,field);
}
static void hb_role_sync(Inst *instance){
    for(int field=0;field<6;field++){
        int *value=hb_policy_chord_field(instance,field);
        /* Native callers that directly edit the existing config retain their intent. */
        if(instance->policy_initialized&&*value!=instance->policy_last[field]){
            instance->policy_overrides|=1u<<field;instance->policy_values[field]=*value;
        }
        *value=hb_policy_value(instance,field);instance->policy_last[field]=*value;
    }
    instance->policy_initialized=1;
}
static void hb_role_restore(Inst *instance,const char *state){
    const char *marker=strstr(state,";rp1,");
    int values[1+HB_POLICY_FIELDS*3],count=0;char *end=0;
    if(marker){
        const char *cursor=marker+5;
        while(count<1+HB_POLICY_FIELDS*3){
            long value=strtol(cursor,&end,10);if(end==cursor)break;
            values[count++]=(int)value;if(*end!=',')break;cursor=end+1;
        }
    }
    int valid=count==1+HB_POLICY_FIELDS*3&&values[0]>=0&&values[0]<(1<<HB_POLICY_FIELDS);
    for(int index=1;valid&&index<count;index++){
        int field=(index-1)%HB_POLICY_FIELDS;
        if(values[index]<(field==HB_P_SPREAD?-9:0)||values[index]>HB_POLICY_MAX[field])valid=0;
    }
    if(valid){
        instance->policy_overrides=(unsigned)values[0];
        memcpy(instance->policy_values,values+1,sizeof(instance->policy_values));
        if(!g_role_restored){
            for(int role=0;role<2;role++)for(int field=0;field<HB_POLICY_FIELDS;field++)hb_role_store(role,field,values[1+(role+1)*HB_POLICY_FIELDS+field]);
            g_role_restored=1;
        }
    }else if(!marker){
        /* Legacy presets keep their sound; new tracks inherit role defaults. */
        instance->policy_overrides=0;
        if(strstr(state,";cp1,")||strstr(state,";cq1,"))for(int field=0;field<6;field++){
            instance->policy_overrides|=1u<<field;instance->policy_values[field]=*hb_policy_chord_field(instance,field);
        }
        for(int role=0;role<2;role++)if(!g_role_restored){
            hb_role_store(role,HB_P_DOMINANT,hb_shared_dominant_scale());hb_role_store(role,HB_P_BORROWED,hb_shared_borrowed_scale());
        }
    }
    instance->policy_initialized=0;hb_role_sync(instance);
}
static int hb_role_save(Inst *instance,char *buffer,int length,int used){
    if(used<0||used>=length)return used;
    used+=snprintf(buffer+used,(size_t)(length-used),";rp1,%u",instance->policy_overrides);
    for(int group=0;group<3;group++)for(int field=0;field<HB_POLICY_FIELDS;field++){
        if(used>=length)return used;
        used+=snprintf(buffer+used,(size_t)(length-used),",%d",group?hb_role_default(group-1,field):instance->policy_values[field]);
    }
    return used;
}
