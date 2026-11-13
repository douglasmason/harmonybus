#define HB_NOTE_BLEND_TEST_HELPERS
#include "note_blend_test.c"
static void expect_cloud_shift(Inst *instance,unsigned hold,int horizontal,int vertical){
    char reply[64],expected[64];API.get_param(instance,"motion_cloud_transform",reply,sizeof(reply));
    snprintf(expected,sizeof(expected),"co1,%u,%d,%d",hold,horizontal,vertical);
    if(strcmp(reply,expected))fprintf(stderr,"cloud transform %s != %s\n",reply,expected);assert(!strcmp(reply,expected));
}
int main(void){
    Inst *instance=setup();API.set_param(instance,"motion_host","movy-clip-v5");
    API.set_param(instance,"motion_operation_1","Cloud Hold");API.set_param(instance,"motion_gesture_1","LatchOn");
    API.set_param(instance,"motion_operation_2","Cloud Shift X");API.set_param(instance,"motion_control_2","50");
    API.set_param(instance,"motion_operation_3","Cloud Shift Y");API.set_param(instance,"motion_control_3","-25");
    API.set_param(instance,"motion_gesture_2","LatchOn");API.set_param(instance,"motion_gesture_3","Knob,1000");expect_cloud_shift(instance,1,8192,-4096);
    API.set_param(instance,"motion_gesture_3","Up,500,1500");expect_cloud_shift(instance,1,8192,0);
    API.set_param(instance,"motion_gesture_3","Knob,2000");API.set_param(instance,"motion_gesture_3","Up,40,2040");expect_cloud_shift(instance,1,8192,0);
    API.set_param(instance,"motion_cloud_input","Down");expect_cloud_shift(instance,1,8192,-4096);
    production_event(instance,0x90,60,100);production_event(instance,0x80,60,0);expect_cloud_shift(instance,1,8192,-4096);
    API.set_param(instance,"motion_cloud_input","Up");expect_cloud_shift(instance,1,8192,0);
    /* The existing pressure mapper owns intensity and reversal; no cloud curve. */
    instance->motion.pressure_active=2;instance->motion.pressure_value[1]=.5;expect_cloud_shift(instance,1,4096,0);
    API.set_param(instance,"motion_control_2","-50");expect_cloud_shift(instance,1,-8192,0);
    instance->motion.pressure_active=2;instance->motion.pressure_value[1]=.5;expect_cloud_shift(instance,1,-4096,0);
    instance->motion.pressure_value[1]=0;expect_cloud_shift(instance,1,0,0);
    instance->motion.pressure_active=0;
    API.set_param(instance,"motion_operation_4","Cloud Shift X");API.set_param(instance,"motion_control_4","100");API.set_param(instance,"motion_gesture_4","LatchOn");expect_cloud_shift(instance,1,8192,0);
    API.set_param(instance,"motion_control_2","400");assert(instance->motion.lanes[1].amount==100);expect_cloud_shift(instance,1,16383,0);
    API.set_param(instance,"motion_host","movy-clip-v4");expect_cloud_shift(instance,1,0,0);
    API.set_param(instance,"motion_host","movy-clip-v5");API.set_param(instance,"hb_movy_block","100,480,48000,1");expect_cloud_shift(instance,0,0,0);
    API.set_param(instance,"hb_movy_block","101,480,48000,0");expect_cloud_shift(instance,1,16383,0);
    API.set_param(instance,"motion_cloud_input","Cancel");expect_cloud_shift(instance,0,0,0);
    char metadata[65536];API.get_param(instance,"chain_params",metadata,sizeof(metadata));assert(strstr(metadata,"Cloud Shift X %"));
    API.destroy_instance(instance);puts("Cloud Shift: shared gestures, contact completion, pressure, signed sums, bounded offsets, capability and bypass pass");
}
