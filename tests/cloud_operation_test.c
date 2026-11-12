#define HB_NOTE_BLEND_TEST_HELPERS
#include "note_blend_test.c"
static unsigned cloud_hold(Inst *instance){char value[16];assert(API.get_param(instance,"motion_cloud_hold",value,sizeof(value))>0);return (unsigned)strtoul(value,0,10);}
static void cloud_gesture(Inst *instance,const char *gesture){API.set_param(instance,"motion_gesture_1",gesture);}
int main(void){
    Inst *instance=setup();API.set_param(instance,"motion_host","movy-clip-v4");
    API.set_param(instance,"motion_operation_1","Cloud Hold");
    cloud_gesture(instance,"Knob,1000");assert(cloud_hold(instance)==1);
    cloud_gesture(instance,"Up,40,1040");assert(!cloud_hold(instance)&&instance->motion.gesture_once==1);
    production_event(instance,0x90,60,100);assert(!cloud_hold(instance));
    API.set_param(instance,"motion_cloud_input","Down");assert(cloud_hold(instance)==1);
    production_event(instance,0x80,60,0);assert(cloud_hold(instance)==1&&instance->motion.gesture_input_started==1);
    API.set_param(instance,"motion_cloud_input","Up");assert(!cloud_hold(instance)&&!instance->motion.gesture_once);
    cloud_gesture(instance,"LatchOn");assert(cloud_hold(instance)==1);
    API.set_param(instance,"role","Off");assert(cloud_hold(instance)==1);API.set_param(instance,"role","Follower");
    API.set_param(instance,"motion_cloud_input","Down");API.set_param(instance,"motion_cloud_input","Up");assert(cloud_hold(instance)==1);
    API.set_param(instance,"motion_cloud_input","Cancel");assert(!cloud_hold(instance));
    cloud_gesture(instance,"Knob,2000");API.set_param(instance,"motion_cloud_input","Down");API.set_param(instance,"motion_cloud_input","Up");assert(cloud_hold(instance)==1);
    cloud_gesture(instance,"Up,40,2040");assert(!cloud_hold(instance)&&!instance->motion.gesture_once);
    cloud_gesture(instance,"LatchOn");API.set_param(instance,"motion_host","movy-clip-v3");assert(!cloud_hold(instance));
    API.set_param(instance,"motion_host","movy-clip-v4");assert(cloud_hold(instance)==1);
    API.set_param(instance,"hb_movy_block","100,480,48000,1");assert(!cloud_hold(instance));
    API.set_param(instance,"hb_movy_block","101,480,48000,0");assert(cloud_hold(instance)==1);
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_enabled","On");assert(!instance->motion.lanes[0].enabled);
    char metadata[65536];API.get_param(instance,"chain_params",metadata,sizeof(metadata));assert(strstr(metadata,"Cloud Hold"));
    hb_stop_instance_note_state(instance);assert(!cloud_hold(instance));API.destroy_instance(instance);
    puts("Cloud operation: shared gestures, contact completion, note isolation, capability, bypass, cancellation");
}
