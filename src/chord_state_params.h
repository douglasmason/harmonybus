/* One editing destination for the existing chord and arp controls. Runtime
   overlays use lane snapshots; selecting a destination never activates it. */
typedef struct {const char *key;size_t offset;const char *const *options;int count,low,high;} hb_cs_parameter;
#define CS_PARAM(key,field,options,count,low,high) {key,__builtin_offsetof(hb_cp_config,field),options,count,low,high}
static const hb_cs_parameter HB_CS_PARAMETERS[]={
    CS_PARAM("chord_mode",mode,CP_CHORD_MODE,3,0,2),
    CS_PARAM("chord_form",size,CP_CHORD_FORM,HB_CP_FORMS,0,HB_CP_FORMS-1),
    CS_PARAM("chord_quality",quality,CP_CHORD_QUALITY,13,0,12),
    CS_PARAM("chromatic_quality",chromatic_quality,CP_CHROMATIC_QUALITY,7,0,6),
    CS_PARAM("chord_inversion",inversion,CP_CHORD_INVERSION,9,0,8),
    CS_PARAM("chord_voicing",voicing,CP_CHORD_VOICING,4,0,3),
    CS_PARAM("arp_playback",playback,CP_ARP_PLAYBACK,3,0,2),
    CS_PARAM("arp_hold",latch,CP_ARP_HOLD,6,0,5),
    CS_PARAM("arp_order",order,CP_ARP_ORDER,7,0,6),
    CS_PARAM("arp_rate",rate,CP_ARP_RATE,18,0,17),
    CS_PARAM("arp_gate",gate,CP_ARP_GATE,4,0,3),
    CS_PARAM("arp_start",start,CP_ARP_START,7,0,6),
    CS_PARAM("arp_phase",phase,CP_ARP_PHASE,3,0,2),
    CS_PARAM("arp_note_phase",note_phase,0,0,-256,256),
    CS_PARAM("arp_clear_harmony",clear_harmony,MO_SWITCH,2,0,1),
    CS_PARAM("strum_spread",spread,0,0,-9,1000),
};
#undef CS_PARAM
static hb_motion_lane *hb_cs_edit_lane(Inst *instance){
    int lane=instance->chord_edit_lane;
    if(lane<0||lane>=16)return 0;
    hb_motion_lane *settings=&instance->motion.lanes[lane];
    if(settings->operation!=HB_MO_CHORD_STATE){instance->chord_edit_lane=-1;return 0;}
    return settings;
}
static void hb_cs_preset(hb_motion_lane *lane,int preset){
    lane->amount=preset;
    if(!preset)return;
    lane->chord_state.mode=preset==1?1:2;lane->chord_state.playback=1;
    lane->chord_state.latch=0;lane->chord_state.order=5;lane->chord_state.rate=1;
    lane->chord_state.voicing=1;lane->chord_state.inversion=8;
    lane->chord_state.phase=2;lane->chord_state.start=5;
    lane->chord_state.note_phase=0;lane->chord_state_valid=1;
}
static int hb_cs_set(Inst *instance,const char *key,const char *value){
    if(!strcmp(key,"chord_edit_target")||(!strcmp(key,"motif_edit")&&instance->motion.lanes[instance->motion.selected].operation==HB_MO_CHORD_STATE)){
        int selected=-1;
        if(!strcmp(key,"motif_edit"))selected=instance->motion.selected;
        else if(!strcmp(value,"Track Settings")||!strcmp(value,"0"))selected=-1;
        else if(!strncmp(value,"Lane ",5))selected=parse_i(value+5,0)-1;
        else selected=parse_i(value,0)-1;
        if(selected<0){instance->chord_edit_lane=-1;return 1;}
        if(selected<16&&instance->motion.lanes[selected].operation==HB_MO_CHORD_STATE)instance->chord_edit_lane=selected;
        return 1;
    }
    int control=hb_mo_slot_key(key,"motion_control_");
    if(!strcmp(key,"motion_amount"))control=instance->motion.selected;
    if(control>=0&&instance->motion.lanes[control].operation==HB_MO_CHORD_STATE){
        hb_motion_lane *lane=&instance->motion.lanes[control];
        hb_cs_preset(lane,enum_index(value,MO_STATE_PRESETS,3,hb_cp_clamp(lane->amount,0,2)));
        hb_motion_publish_settings(instance);return 1;
    }
    hb_motion_lane *lane=hb_cs_edit_lane(instance);
    if(!strcmp(key,"chord_state_copy")){
        if(lane&&strcmp(value,"Off")){lane->chord_state=instance->player.config;lane->chord_state_valid=1;lane->amount=0;hb_motion_publish_settings(instance);}return 1;
    }
    if(!strcmp(key,"chord_input")){
        if(lane){static const char *options[]={"Single Note","Root/Bass + Top"};lane->chord_input=enum_index(value,options,2,lane->chord_input);hb_motion_publish_settings(instance);}return 1;
    }
    if(!lane&&!instance->player.state_override)return 0;
    for(unsigned index=0;index<sizeof(HB_CS_PARAMETERS)/sizeof(HB_CS_PARAMETERS[0]);index++){
        const hb_cs_parameter *spec=&HB_CS_PARAMETERS[index];if(strcmp(key,spec->key))continue;
        if(!lane){int role;if(hb_policy_key(key,&role)>=0)return 0;}
        hb_cp_config *config=lane?&lane->chord_state:&instance->player.config;
        int *field=(int*)((char*)config+spec->offset);
        if(!strcmp(value,"Role Default"))return 1;
        int next=spec->options?enum_index(value,spec->options,spec->count,*field):parse_i(value,*field);
        if(!strcmp(key,"strum_spread"))for(int division=0;division<9;division++)if(!strcmp(value,BUFFER_DIVISIONS[division]))next=-division-1;
        *field=hb_cp_clamp(next,spec->low,spec->high);
        if(lane){lane->amount=0;lane->chord_state_valid=1;hb_motion_publish_settings(instance);}return 1;
    }return 0;
}
static const char *HB_CS_NAMES[]={"Auto Chord","Form","Quality","Chromatic","Inversion","Voicing","Playback","Hold","Order","Rate","Gate","Start Note","Start Timing","Note Phase","Clear on Harmony","Spread"};
static int hb_cs_metadata(Inst *instance,char *buffer,int length){
    int used=hb_mo_get(&instance->motion,"chain_params",buffer,length);
    if(used<1||used>=length)return -1;used--;
    used+=snprintf(buffer+used,(size_t)(length-used),",{\"key\":\"chord_edit_target\",\"name\":\"Edit Target\",\"type\":\"enum\",\"options_as_string\":true,\"options\":[\"Track Settings\"");
    for(int lane=0;lane<16;lane++)if(instance->motion.lanes[lane].operation==HB_MO_CHORD_STATE){
        if(used>=length)return -1;used+=snprintf(buffer+used,(size_t)(length-used),",\"Lane %d\"",lane+1);
    }
    if(used>=length)return -1;used+=snprintf(buffer+used,(size_t)(length-used),"]}");
    if(hb_cs_edit_lane(instance))for(unsigned index=0;index<sizeof(HB_CS_PARAMETERS)/sizeof(HB_CS_PARAMETERS[0]);index++){
        const hb_cs_parameter *spec=&HB_CS_PARAMETERS[index];int role;
        if(!spec->options||hb_policy_key(spec->key,&role)<0)continue;
        if(used>=length)return -1;
        used+=snprintf(buffer+used,(size_t)(length-used),",{\"key\":\"%s\",\"name\":\"%s\",\"type\":\"enum\",\"options_as_string\":true,\"options\":[",spec->key,HB_CS_NAMES[index]);
        for(int option=0;option<spec->count;option++){
            if(used>=length)return -1;used+=snprintf(buffer+used,(size_t)(length-used),"%s\"%s\"",option?",":"",spec->options[option]);
        }
        if(used>=length)return -1;used+=snprintf(buffer+used,(size_t)(length-used),"]}");
    }
    used=hb_defaults_metadata(instance,buffer,length,used);
    if(used<0||used>=length)return -1;used+=snprintf(buffer+used,(size_t)(length-used),"]");return used>=length?-1:used;
}
static int hb_cs_get(Inst *instance,const char *key,char *buffer,int length){
    hb_motion_lane *lane=hb_cs_edit_lane(instance);
    if(!strcmp(key,"chain_params"))return hb_cs_metadata(instance,buffer,length);
    if(!strcmp(key,"chord_edit_target"))return lane?snprintf(buffer,(size_t)length,"Lane %d",instance->chord_edit_lane+1):snprintf(buffer,(size_t)length,"Track Settings");
    if(!strcmp(key,"motif_edit")&&instance->motion.lanes[instance->motion.selected].operation==HB_MO_CHORD_STATE)return snprintf(buffer,(size_t)length,"Edit State");
    if(!strcmp(key,"chord_state_copy"))return snprintf(buffer,(size_t)length,"Off");
    if(!strcmp(key,"chord_input"))return snprintf(buffer,(size_t)length,"%s",lane&&lane->chord_input?"Root/Bass + Top":"Single Note");
    if(!lane)return -1;
    for(unsigned index=0;index<sizeof(HB_CS_PARAMETERS)/sizeof(HB_CS_PARAMETERS[0]);index++){
        const hb_cs_parameter *spec=&HB_CS_PARAMETERS[index];if(strcmp(key,spec->key))continue;
        int field=*(int*)((char*)&lane->chord_state+spec->offset);
        if(spec->options)return snprintf(buffer,(size_t)length,"%s",spec->options[hb_cp_clamp(field,0,spec->count-1)]);
        if(!strcmp(key,"strum_spread"))return field<0?snprintf(buffer,(size_t)length,"%s",BUFFER_DIVISIONS[-field-1]):snprintf(buffer,(size_t)length,"%d ms",field);
        return snprintf(buffer,(size_t)length,"%d",field);
    }return -1;
}
