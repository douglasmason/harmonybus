#define HB_NOTE_BLEND_TEST_HELPERS
#include "note_blend_test.c"
static Inst *operation_fixture(void){
    Inst *instance=setup();instance->travel_map=7;instance->content_map=2;
    API.set_param(instance,"chord_mode","Off");API.set_param(instance,"render_channel","Off");
    API.set_param(instance,"motion_operation_1","Note Blend");
    assert(instance->motion.lanes[0].operation==HB_MO_NOTE_BLEND&&!instance->motion.lanes[0].enabled);
    API.set_param(instance,"motion_control_1","Bend 12");assert(instance->motion.lanes[0].amount==3);
    clear_observation();return instance;
}
static void arm_blend(Inst *instance){
    API.set_param(instance,"motion_gesture_1","Knob,1000");
    API.set_param(instance,"motion_gesture_1","Up,40,1040");
    assert(instance->motion.gesture_once==1&&!instance->motion.gesture_persistent);
}
static void status_blend(Inst *instance,const char *expected){
    char value[64];API.get_param(instance,"motion_status_1",value,sizeof(value));assert(!strcmp(value,expected));
}
static void overlap_phrase(void){
    Inst *instance=operation_fixture();arm_blend(instance);status_blend(instance,"Armed");
    production_event(instance,0x90,60,100);production_event(instance,0x80,60,0);
    assert(instance->motion.gesture_once==1&&!instance->motion_local.blend.mode); /* No overlap: not consumed. */
    production_event(instance,0x90,60,100);production_event(instance,0x91,72,100);
    assert(!instance->motion.gesture_input_started); /* Different channels aren't a blend pair. */
    production_event(instance,0x80,60,0);production_event(instance,0x81,72,0);
    production_event(instance,0x90,60,100);production_event(instance,0x90,72,100);
    assert(instance->motion_local.blend.mode==3&&bends[0]==12288&&sounding[0][60]&&!sounding[0][72]);
    status_blend(instance,"Playing");
    production_event(instance,0x80,60,0);assert(instance->motion.gesture_once==1&&bends[0]==16383);
    production_event(instance,0x90,67,100);assert(instance->motion_local.blend.mode==3);
    production_event(instance,0x80,72,0);assert(instance->motion.gesture_input_started==1);
    production_event(instance,0x80,67,0);
    assert(!instance->motion.held&&!instance->motion.gesture_once&&!instance->motion.gesture_input_started&&!instance->motion_local.blend.mode);
    assert(bends[0]==8192);status_blend(instance,"Off");
    production_event(instance,0x90,60,100);production_event(instance,0x90,72,100);
    assert(sounding[0][60]&&sounding[0][72]);
    API.destroy_instance(instance);
}
static void gestures_and_settings(void){
    Inst *instance=operation_fixture();
    API.set_param(instance,"motion_gesture_1","Knob,2000");
    production_event(instance,0x90,60,100);production_event(instance,0x90,72,100);
    assert(instance->motion_local.blend.mode==3);
    API.set_param(instance,"motion_gesture_1","Up,500,2500");production_drain(instance);
    assert(!instance->motion.held&&!instance->motion_local.blend.mode&&sounding[0][60]&&sounding[0][72]);
    production_event(instance,0x80,60,0);production_event(instance,0x80,72,0);
    API.set_param(instance,"motion_gesture_1","LatchOn");
    for(int phrase=0;phrase<2;phrase++){
        production_event(instance,0x90,60,100);production_event(instance,0x90,72,100);
        assert(instance->motion_local.blend.mode==3&&instance->motion.gesture_persistent==1);
        production_event(instance,0x80,60,0);production_event(instance,0x80,72,0);
        assert(instance->motion.gesture_persistent==1&&!instance->motion.gesture_input_started&&!instance->motion_local.blend.mode);
    }
    API.set_param(instance,"motion_gesture_1","LatchOff");assert(!instance->motion.held);
    API.set_param(instance,"note_blend","Steps");arm_blend(instance);
    production_event(instance,0x90,60,100);production_event(instance,0x90,72,100);
    API.set_param(instance,"note_blend","Bend 2");assert(instance->motion_local.blend.mode==3);
    char saved[65536],value[128];API.get_param(instance,"state",saved,sizeof(saved));assert(strstr(saved,";nb1,2"));
    API.get_param(instance,"note_blend",value,sizeof(value));assert(!strcmp(value,"Bend 2"));
    production_event(instance,0x80,60,0);production_event(instance,0x80,72,0);assert(instance->motion_local.blend.mode==2);
    API.set_param(instance,"state",saved);production_drain(instance);
    assert(instance->motion.lanes[0].operation==HB_MO_NOTE_BLEND&&instance->motion.lanes[0].amount==3&&!instance->motion.gesture_once&&!instance->motion.gesture_input_started);
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_amount","Bend 48");
    assert(instance->motion.lanes[0].amount==5);API.get_param(instance,"motion_amount",value,sizeof(value));assert(!strcmp(value,"Bend 48"));
    API.get_param(instance,"chain_params",saved,sizeof(saved));assert(strstr(saved,"Note Blend")&&strstr(saved,"Bend 48"));
    API.destroy_instance(instance);
}
static void musical_inputs_and_catalog(void){
    Inst *instance=operation_fixture();arm_blend(instance);
    instance->movy_playback=1;production_event(instance,0x90,60,100);production_event(instance,0x90,72,100);
    assert(!instance->motion.gesture_input_started&&instance->motion.gesture_once==1);
    production_event(instance,0x80,60,0);production_event(instance,0x80,72,0);instance->movy_playback=0;
    API.set_param(instance,"chord_mode","Conductor Chord");API.set_param(instance,"chord_form","Triad");
    arm_blend(instance);production_event(instance,0x90,60,100);
    assert(!instance->motion.gesture_input_started&&!instance->motion_local.blend.mode);
    production_event(instance,0x80,60,0);assert(instance->motion.gesture_once==1);
    hb_stop_instance_note_state(instance);production_drain(instance);assert(!instance->motion.gesture_once);
    /* Normal operations retain all shared gestures regardless of saved ID. */
    const char *names[]={"Note Repeat","Repeat Divisions","Smart Below","Smart Above","Clip Weave","Note Blend"};
    for(unsigned index=0;index<sizeof(names)/sizeof(*names);index++){
        API.set_param(instance,"motion_operation_1",names[index]);
        API.set_param(instance,"motion_gesture_1","LatchOn");assert(instance->motion.gesture_persistent==1);
        API.set_param(instance,"motion_gesture_1","LatchOff");assert(!instance->motion.gesture_persistent);
    }
    assert(hb_mo_immediate_action(HB_MO_RELATIVE_KEY)&&hb_mo_immediate_action(HB_MO_KEY_BACK)&&hb_mo_immediate_action(HB_MO_KEY_RETURN));
    assert(hb_mo_overlap_input(HB_MO_NOTE_BLEND)&&!hb_mo_overlap_input(HB_MO_NOTE_REPEAT));
    API.destroy_instance(instance);
}
int main(void){
    overlap_phrase();gestures_and_settings();musical_inputs_and_catalog();
    puts("Note Blend operation: shared one-shot/latch/hold, overlap phrase continuation, ordinary-note isolation, base restore, persistence and catalog gesture permissions pass");
}
