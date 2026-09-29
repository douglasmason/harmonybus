static const char *HB_DEFAULT_EDITORS[]={"Conductor Chords","Follower Chords","Conductor Scales","Follower Scales"};
static const char *HB_PALETTES[]={"Ion / Dor / Loc","Lyd / Dor / Loc","Ion / Aeo / Loc","Lyd / Aeo / Loc","Ion / Dor / Loc#2","Lyd / Dor / Loc#2","Ion / Aeo / Loc#2","Lyd / Aeo / Loc#2","Role Default"};
static int hb_palette_role(const char *key){
    if(!strcmp(key,"local_palette"))return -1;
    if(!strcmp(key,"conductor_default_local_palette"))return 0;
    if(!strcmp(key,"follower_default_local_palette"))return 1;
    return -2;
}
static const char *hb_defaults_alias(Inst *instance,const char *key,char *alias,int length){
    int control=hb_mo_slot_key(key,"defaults_control_");if(control<0||control>=6)return key;
    static const char *fields[2][6]={{"chord_form","chord_quality","chord_inversion","chord_voicing","strum_spread","chromatic_quality"},{"gap_scale","scale_context","dominant_scale","borrowed_scale","local_palette","scope"}};
    if(instance->defaults_editor>=2&&control==5)return "defaults_scope";
    snprintf(alias,(size_t)length,"%s_default_%s",instance->defaults_editor&1?"follower":"conductor",fields[instance->defaults_editor>=2][control]);return alias;
}
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
    if(!strcmp(key,"defaults_editor")){instance->defaults_editor=enum_index(parameter,HB_DEFAULT_EDITORS,4,instance->defaults_editor);return 1;}
    char alias[96];key=hb_defaults_alias(instance,key,alias,sizeof(alias));
    if(!strcmp(key,"defaults_scope"))return 1;
    int palette_role=hb_palette_role(key);
    if(palette_role!=-2){
        int previous=0;for(int bit=0;bit<3;bit++)previous|=(palette_role<0?hb_policy_value(instance,HB_P_MAJOR+bit):hb_role_default(palette_role,HB_P_MAJOR+bit))<<bit;
        int selected=enum_index(parameter,HB_PALETTES,palette_role<0?9:8,previous);
        for(int bit=0;bit<3;bit++){
            int field=HB_P_MAJOR+bit;
            if(palette_role<0){
                if(selected==8)instance->policy_overrides&=~(1u<<field);
                else {instance->policy_overrides|=1u<<field;instance->policy_values[field]=(selected>>bit)&1;}
                instance->policy_initialized=0;
            }else {hb_role_store(palette_role,field,(selected>>bit)&1);g_role_restored=1;}
        }
        hb_role_sync(instance);return 1;
    }
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
    if(!strcmp(key,"defaults_editor"))return snprintf(buffer,(size_t)length,"%s",HB_DEFAULT_EDITORS[instance->defaults_editor]);
    char alias[96];key=hb_defaults_alias(instance,key,alias,sizeof(alias));
    if(!strcmp(key,"defaults_scope"))return snprintf(buffer,(size_t)length,"All %s",instance->defaults_editor&1?"followers":"conductors");
    int palette_role=hb_palette_role(key);
    if(palette_role!=-2){
        int selected=0;for(int bit=0;bit<3;bit++)selected|=(palette_role<0?hb_policy_value(instance,HB_P_MAJOR+bit):hb_role_default(palette_role,HB_P_MAJOR+bit))<<bit;
        if(palette_role<0&&!(instance->policy_overrides&(7u<<HB_P_MAJOR)))selected=8;
        return snprintf(buffer,(size_t)length,"%s",HB_PALETTES[selected]);
    }
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

static int hb_defaults_metadata(Inst *instance,char *buffer,int length,int used){
    for(int control=1;control<=6;control++){
        char key[32],alias[96],match[128];snprintf(key,sizeof(key),"defaults_control_%d",control);
        const char *target=hb_defaults_alias(instance,key,alias,sizeof(alias));
        snprintf(match,sizeof(match),"\"key\":\"%s\"",target);
        const char *start=strstr(HB_CHAIN_PARAMS_PREFIX,match);if(!start)return -1;
        start+=strlen(match);const char *end=strchr(start,'}');if(!end||used>=length)return -1;
        used+=snprintf(buffer+used,(size_t)(length-used),",{\"key\":\"%s\"%.*s}",key,(int)(end-start),start);
        if(used>=length)return -1;
    }return used;
}
