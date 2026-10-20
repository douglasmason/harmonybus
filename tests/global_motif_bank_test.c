#define main reference_tests_main
#include "follower_reference_test.c"
#undef main
static void expect_bank(Inst *instance,const char *expected){
    char value[128];API.get_param(instance,"approach_bank_1",value,sizeof(value));assert(!strcmp(value,expected));
}
int main(void){
    Inst *first=fixture(),*second=API.create_instance("",0);
    API.set_param(first,"approach_bank_1","Stock: ii-V-Target");
    expect_bank(second,"Stock: ii-V-Target");
    char saved[65536];API.get_param(first,"state",saved,sizeof(saved));
    second->approach_rows.cursor=3;second->approach_rows.latch_slots=4;second->approach_rows.down=8;
    API.set_param(first,"approach_bank_1","Leading Tone");expect_bank(second,"Leading Tone");
    assert(second->approach_rows.cursor==3&&second->approach_rows.latch_slots==4&&second->approach_rows.down==8);
    Inst *third=API.create_instance("",0);expect_bank(third,"Leading Tone");
    API.set_param(second,"state",saved);expect_bank(second,"Leading Tone");expect_bank(first,"Leading Tone");
    /* Operators already share their definitions without sharing holds. */
    API.set_param(first,"motion_operation_1","Play Motif");
    assert(first->motion.lanes[0].operation==HB_MO_MOTIF&&first->motion.lanes[0].operation==second->motion.lanes[0].operation);
    first->motion.held=1;assert(second->motion.held==0);
    API.destroy_instance(third);API.destroy_instance(second);API.destroy_instance(first);
    first=API.create_instance("",0);second=API.create_instance("",0);
    API.set_param(first,"state",saved);expect_bank(first,"Stock: ii-V-Target");expect_bank(second,"Stock: ii-V-Target");
    API.set_param(second,"approach_knob_1","Leading Tone");expect_bank(first,"Leading Tone");
    API.destroy_instance(second);API.destroy_instance(first);
    first=API.create_instance("",0);assert(first->approach_rows.bank[0]==HB_AR_DEFAULT_BANK[0]);API.destroy_instance(first);
    puts("global motif bank: edit propagation, new instances, local runtime, stale restores, reload, legacy edits and teardown pass");
}
