/* Scheduled and physical ownership share operations without sharing release state. */
#define main motion_regression_main
#include "chord_player_test.c"
#undef main
int main(void){
    Inst *instance=fixture();
    API.set_param(instance,"motion_lane","1");
    API.set_param(instance,"motion_operation","Octave");
    API.set_param(instance,"motion_enabled","Off");
    API.set_param(instance,"motion_sequence_mask","1");
    assert(hb_mo_lane_active(&instance->motion,0));
    API.set_param(instance,"motion_hold_1","On");
    API.set_param(instance,"motion_sequence_mask","0");
    assert(hb_mo_lane_active(&instance->motion,0));
    API.set_param(instance,"motion_hold_1","Off");
    assert(!hb_mo_lane_active(&instance->motion,0));
    API.set_param(instance,"motion_sequence_mask","1");
    API.set_param(instance,"motion_hold_1","On");
    API.set_param(instance,"motion_hold_1","Off");
    assert(hb_mo_lane_active(&instance->motion,0));
    API.set_param(instance,"motion_sequence_mask","0");
    assert(!hb_mo_lane_active(&instance->motion,0));
    instance->motion.lanes[0].operation=HB_MO_ENCLOSE_AB;
    API.set_param(instance,"motion_sequence_mask","1");
    assert(hb_mo_lane_active(&instance->motion,0));
    API.set_param(instance,"motion_sequence_mask","0");
    assert(!hb_mo_lane_active(&instance->motion,0));
    API.set_param(instance,"motion_sequence_mask","1");
    instance->motion.held|=1; /* A live owner retains the shared approach. */
    API.set_param(instance,"motion_sequence_mask","0");
    assert(hb_mo_lane_active(&instance->motion,0));
    instance->motion.held=0;instance->motion.enclosure=0;
    instance->motion.lanes[0].operation=HB_MO_MOTIF;instance->motion.lanes[0].amount=1;
    instance->physical_target=61;instance->physical_velocity[0][60]=100;
    API.set_param(instance,"motion_sequence_mask","1");
    assert(instance->motif.pending>0); /* Existing motif scheduler uses the held target. */
    API.destroy_instance(instance);
    puts("grid gate ownership passed");return 0;
}
