static const char *HB_GAP_OPTIONS[]={"Parent","Strict Local","Auto Local"};
static const char *HB_CONTEXT_OPTIONS[]={"Current Harm","Current + Next","Full Loop"};
static const char *HB_MAJOR_OPTIONS[]={"Ionian","Lydian"};
static const char *HB_MINOR_OPTIONS[]={"Dorian","Aeolian"};
static const char *HB_HALFDIM_OPTIONS[]={"Locrian","Locrian #2"};
static const char **HB_POLICY_OPTIONS[]={CP_CHORD_FORM,CP_CHORD_QUALITY,CP_CHORD_INVERSION,CP_CHORD_VOICING,0,CP_CHROMATIC_QUALITY,HB_GAP_OPTIONS,HB_CONTEXT_OPTIONS,HB_MAJOR_OPTIONS,HB_MINOR_OPTIONS,HB_HALFDIM_OPTIONS,DOMINANT_SCALE_OPTS,BORROWED_SCALE_OPTS};
static int hb_policy_key(const char *key,int *role){
    *role=-1;
    if(!strncmp(key,"track_",6))key+=6;
    if(!strncmp(key,"conductor_default_",18)){*role=0;key+=18;}
    else if(!strncmp(key,"follower_default_",17)){*role=1;key+=17;}
    for(int field=0;field<HB_POLICY_FIELDS;field++)if(!strcmp(key,HB_POLICY_KEYS[field]))return field;
    return -1;
}
static int hb_policy_set(Inst *instance,const char *key,const char *parameter){
    if(!strcmp(key,"dominant_scale")||!strcmp(key,"borrowed_scale"))return 0;
    if(!strcmp(key,"chord_reset_overrides")){
        if(!strcmp(parameter,"Reset")){instance->policy_overrides=0;instance->policy_initialized=0;hb_role_sync(instance);}return 1;
    }
    int role,field=hb_policy_key(key,&role);if(field<0)return 0;
    if(role<0&&!strcmp(parameter,"Role Default")){
        instance->policy_overrides&=~(1u<<field);instance->policy_initialized=0;hb_role_sync(instance);return 1;
    }
    int previous=role<0?hb_policy_value(instance,field):hb_role_default(role,field);
    int selected=HB_POLICY_OPTIONS[field]?enum_index(parameter,HB_POLICY_OPTIONS[field],HB_POLICY_MAX[field]+1,previous):parse_i(parameter,previous);
    if(field==HB_P_SPREAD)for(int division=0;division<9;division++)if(!strcmp(parameter,BUFFER_DIVISIONS[division]))selected=-division-1;
    selected=hb_cp_clamp(selected,field==HB_P_SPREAD?-9:0,HB_POLICY_MAX[field]);
    if(role<0){instance->policy_overrides|=1u<<field;instance->policy_values[field]=selected;instance->policy_initialized=0;}
    else {hb_role_store(role,field,selected);g_role_restored=1;}
    hb_role_sync(instance);return 1;
}
static int hb_policy_get(Inst *instance,const char *key,char *buffer,int length){
    if(!strcmp(key,"dominant_scale")||!strcmp(key,"borrowed_scale"))return -1;
    if(!strcmp(key,"chord_reset_overrides"))return snprintf(buffer,(size_t)length,"Off");
    if(!strcmp(key,"chord_scope")){
        if(!instance->policy_overrides)return snprintf(buffer,(size_t)length,"%s defaults",instance->role==0?"Conductor":"Follower");
        int used=snprintf(buffer,(size_t)length,"Track:");
        static const char *labels[]={"Form","Quality","Inversion","Voicing","Spread","Chromatic","Gap","Context","Major","Minor","HalfDim","Dominant","Borrowed"};
        for(int field=0;field<HB_POLICY_FIELDS&&used<length;field++)if(instance->policy_overrides&(1u<<field))used+=snprintf(buffer+used,(size_t)(length-used)," %s",labels[field]);
        return used;
    }
    int role,field=hb_policy_key(key,&role);if(field<0)return -1;
    int selected=role<0?hb_policy_value(instance,field):hb_role_default(role,field);
    if(HB_POLICY_OPTIONS[field])return snprintf(buffer,(size_t)length,"%s",HB_POLICY_OPTIONS[field][selected]);
    return selected<0?snprintf(buffer,(size_t)length,"%s",BUFFER_DIVISIONS[-selected-1]):snprintf(buffer,(size_t)length,"%d ms",selected);
}
