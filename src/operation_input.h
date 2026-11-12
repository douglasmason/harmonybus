/* Operation gestures and musical inputs are independent. Reuse the physical
   note map: playback, generated chords and approach pads cannot start a live
   phrase. The catalog declares whether one note or overlapping notes starts it. */
static int hb_operation_notes_held(const Inst *instance,int channel){
    int count=0;
    for(int note=0;note<128;note++)count+=instance->physical_velocity[channel][note]!=0;
    return count;
}
static void hb_operation_note_input(Inst *instance,const uint8_t *input,int length){
    if(!input||length<3||(input[0]&240)!=0x90||!input[2]||instance->movy_playback||
       instance->synthetic_advance||instance->role>=2||instance->motif.editor.recording>=0||
       instance->movy_pad_pending==(input[1]&127)+1||!hb_source_channel_matches(instance,input[0]&15))return;
    hb_motion_config *config=&instance->motion;
    unsigned long long eligible=0;int notes=hb_operation_notes_held(instance,input[0]&15);
    for(int lane=0;lane<HB_MOTION_USER_LANES;lane++)
        if(hb_mo_note_input(config->lanes[lane].operation)&&notes>=hb_mo_note_input(config->lanes[lane].operation)&&hb_mo_lane_active(config,lane))eligible|=1ULL<<lane;
    if(!eligible)return;
    config->gesture_input_started|=eligible;
    config->gesture_used|=eligible&config->gesture_down;
}
static void hb_note_blend_sync(Inst *instance){
    hb_motion_config *config=&instance->motion;
    int mode=instance->role<2?instance->note_blend_base:0;unsigned newest=0;
    if(!config->gesture_input_started){
        hb_nb_mode(&instance->motion_local.blend,mode);hb_nb_mode(&instance->motion_render.blend,mode);return;
    }
    for(int lane=0;lane<HB_MOTION_USER_LANES;lane++){
        unsigned long long bit=1ULL<<lane;
        if(!hb_mo_input_kind(config->lanes[lane].operation)||!hb_mo_lane_active(config,lane))config->gesture_input_started&=~bit;
        if(instance->role<2&&(config->gesture_input_started&bit)&&config->lanes[lane].operation==HB_MO_NOTE_BLEND&&config->held_serial[lane]>=newest){
            newest=config->held_serial[lane];mode=hb_mo_clamp(config->lanes[lane].amount,1,5);
        }
    }
    hb_nb_mode(&instance->motion_local.blend,mode);hb_nb_mode(&instance->motion_render.blend,mode);
}
static void hb_operation_note_finish(Inst *instance){
    hb_motion_config *config=&instance->motion;
    if(!config->gesture_input_started)return;
    for(int channel=0;channel<16;channel++)if(hb_operation_notes_held(instance,channel))return;
    /* A one-shot completes at the phrase boundary. Persistent and currently
       held gestures stay enabled, ready for another overlapping phrase. */
    unsigned long long finished=0;
    for(int lane=0;lane<HB_MOTION_USER_LANES;lane++)if(hb_mo_note_input(config->lanes[lane].operation))finished|=config->gesture_input_started&(1ULL<<lane);
    config->gesture_input_started&=~finished;
    hb_mo_end_lanes(config,finished&config->gesture_once&~config->gesture_persistent);
    hb_note_blend_sync(instance);hb_motion_flush_render(instance);
}

/* The host owns cloud contacts. It reports stroke edges; activation and
   completion continue to use the same lane state as physical note inputs. */
static unsigned long long hb_operation_cloud_mask(const hb_motion_config *config){
    unsigned long long mask=0;
    for(int lane=0;lane<HB_MOTION_USER_LANES;lane++)if(hb_mo_cloud_input(config->lanes[lane].operation))mask|=1ULL<<lane;
    return mask;
}
static void hb_operation_cloud_input(hb_motion_config *config,const char *edge){
    unsigned long long mask=hb_operation_cloud_mask(config);
    if(!strcmp(edge,"Cancel")){hb_mo_end_lanes(config,mask);return;}
    if(!strcmp(edge,"Down")){
        for(int lane=0;lane<HB_MOTION_USER_LANES;lane++)if((mask&(1ULL<<lane))&&hb_mo_lane_active(config,lane)){
            config->gesture_input_started|=1ULL<<lane;config->gesture_used|=config->gesture_down&(1ULL<<lane);
        }
    }else if(!strcmp(edge,"Up")){
        unsigned long long finished=mask&config->gesture_input_started;
        config->gesture_input_started&=~mask;
        hb_mo_end_lanes(config,finished&config->gesture_once&~config->gesture_persistent&~config->gesture_down);
    }
}
static unsigned hb_operation_cloud_hold(const hb_motion_config *config){
    unsigned mask=0;
    for(int lane=0;lane<HB_MOTION_USER_LANES;lane++){
        unsigned long long bit=1ULL<<lane;
        if(!hb_mo_cloud_input(config->lanes[lane].operation)||!hb_mo_lane_active(config,lane))continue;
        if((config->gesture_once&bit)&&!(config->gesture_down&bit)&&!(config->gesture_input_started&bit))continue;
        mask|=(unsigned)bit;
    }
    return mask;
}
