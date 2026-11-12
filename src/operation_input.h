/* Operation gestures and musical inputs are independent. Reuse the physical
   note map: playback, generated chords and approach pads cannot start a live
   overlapping-note phrase. Catalog input traits can serve further operations. */
static int hb_operation_notes_held(const Inst *instance,int channel){
    int count=0;
    for(int note=0;note<128;note++)count+=instance->physical_velocity[channel][note]!=0;
    return count;
}
static void hb_operation_overlap_input(Inst *instance,const uint8_t *input,int length){
    if(!input||length<3||(input[0]&240)!=0x90||!input[2]||instance->movy_playback||
       instance->synthetic_advance||instance->role>=2||instance->motif.editor.recording>=0||
       instance->movy_pad_pending==(input[1]&127)+1||!hb_source_channel_matches(instance,input[0]&15))return;
    hb_motion_config *config=&instance->motion;
    unsigned long long eligible=0;
    for(int lane=0;lane<HB_MOTION_USER_LANES;lane++)
        if(hb_mo_overlap_input(config->lanes[lane].operation)&&hb_mo_lane_active(config,lane))eligible|=1ULL<<lane;
    if(!eligible||hb_operation_notes_held(instance,input[0]&15)<2)return;
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
        if(!hb_mo_overlap_input(config->lanes[lane].operation)||!hb_mo_lane_active(config,lane))config->gesture_input_started&=~bit;
        if(instance->role<2&&(config->gesture_input_started&bit)&&config->lanes[lane].operation==HB_MO_NOTE_BLEND&&config->held_serial[lane]>=newest){
            newest=config->held_serial[lane];mode=hb_mo_clamp(config->lanes[lane].amount,1,5);
        }
    }
    hb_nb_mode(&instance->motion_local.blend,mode);hb_nb_mode(&instance->motion_render.blend,mode);
}
static void hb_operation_overlap_finish(Inst *instance){
    hb_motion_config *config=&instance->motion;
    if(!config->gesture_input_started)return;
    for(int channel=0;channel<16;channel++)if(hb_operation_notes_held(instance,channel))return;
    /* A one-shot completes at the phrase boundary. Persistent and currently
       held gestures stay enabled, ready for another overlapping phrase. */
    unsigned long long finished=config->gesture_input_started;
    config->gesture_input_started=0;
    hb_mo_end_lanes(config,finished&config->gesture_once&~config->gesture_persistent);
    hb_note_blend_sync(instance);hb_motion_flush_render(instance);
}
