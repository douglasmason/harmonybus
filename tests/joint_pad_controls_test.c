#define main reference_tests_main
#include "follower_reference_test.c"
#undef main
int main(void){
    Inst *root=fixture(),*bass=API.create_instance("",0),*comp=API.create_instance("",0),*melody=API.create_instance("",0),*other=API.create_instance("",0);
    root->movy_track=0;bass->movy_track=1;comp->movy_track=2;melody->movy_track=3;other->movy_track=4;
    API.set_param(root,"approach_bank_1","Connector Below");
    API.set_param(root,"joint_pad_approach_mode_active","9|1");
    API.set_param(melody,"joint_pad_approach_mode_active","9|1");
    API.set_param(root,"joint_pad_approach_step_touch_1","9|Down");
    API.set_param(melody,"joint_pad_approach_step_touch_1","9|Down");
    assert(root->approach_rows.step_down&&melody->approach_rows.step_down);
    assert(!bass->approach_rows.step_down&&!comp->approach_rows.step_down);
    assert(!root->physical_target&&!melody->physical_target); /* arming produces no notes */
    uint8_t press[3]={0x90,60,100},release[3]={0x80,60,0},output[64][3];int lengths[64];
    API.process_midi(root,press,3,output,lengths,64);assert(root->approach_rows.tokens[60]);
    API.process_midi(root,release,3,output,lengths,64);
    assert(!melody->physical_target); /* a root pad does not play melody */
    API.set_param(root,"joint_pad_key_center","9|Down");
    assert(g_key_armed&&hb_joint_key_zone(root)&&!hb_joint_key_zone(bass)&&hb_joint_key_zone(melody));
    assert(!hb_joint_key_zone(comp)&&!hb_joint_key_zone(other));
    API.set_param(melody,"key_center","Down");assert(!g_sc_pad_mask&&g_sc_arm_owner==melody-g_pool);
    API.set_param(melody,"key_center","Off");
    API.set_param(other,"role","Conductor");
    API.set_param(other,"motion_operation_1","Live Key Center");
    g_sc_pad_mask=9;API.set_param(other,"motion_hold_1","On");hb_key_operations_sync();
    assert(g_key_armed&&!g_sc_pad_mask&&g_sc_arm_owner==other-g_pool);
    API.set_param(other,"motion_hold_1","Off");API.set_param(other,"key_center","Off");
    API.set_param(root,"motion_operation_1","Relative Key Center");
    hb_key_context before=g_key_context;
    API.set_param(root,"joint_pad_motion_hold_1","9|On");
    assert(g_key_armed&&g_sc_pad_mask==9&&!memcmp(&before,&g_key_context,sizeof(before)));
    API.set_param(root,"joint_pad_motion_hold_1","9|Off");API.set_param(root,"key_center","Off");
    API.set_param(root,"joint_pad_approach_step_touch_1","9|Up,500");
    API.set_param(melody,"joint_pad_approach_step_touch_1","9|Up,500");
    assert(!root->approach_rows.step_down&&!melody->approach_rows.step_down);
    API.destroy_instance(other);API.destroy_instance(melody);API.destroy_instance(comp);API.destroy_instance(bass);API.destroy_instance(root);
    puts("Joint pad controls: shared intent, pad-owned sound, comp exclusion, scoped key center and release pass");
}
