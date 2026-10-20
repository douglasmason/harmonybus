#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"
static void edge(Inst *instance,int lane,int down,int elapsed,int stamp){
    char key[40],value[64];snprintf(key,sizeof(key),"motion_gesture_%d",lane);
    snprintf(value,sizeof(value),down?"Touch,%d":"Up,%d,%d",down?stamp:elapsed,stamp);
    API.set_param(instance,key,value);
}
static void modern_tap(Inst *instance,int lane,int stamp){edge(instance,lane,1,0,stamp);edge(instance,lane,0,40,stamp+40);}
static void trigger_states(void){
    Inst *instance=setup();
    modern_tap(instance,1,1000);assert(instance->motion.enclosure&&!instance->motion.gesture_persistent);
    modern_tap(instance,1,1100);assert(instance->motion.gesture_persistent&1);
    for(int n=0;n<3;n++){hb_mo_input(&instance->motion,60,n,.01);assert(hb_mo_source_secondary(&instance->motion)==1&&instance->motion.enclosure);}
    edge(instance,1,1,0,2000);edge(instance,1,0,500,2500);
    assert((instance->motion.gesture_persistent&1)&&!(instance->motion.held&1));
    modern_tap(instance,1,3000);assert(!instance->motion.enclosure&&!instance->motion.gesture_persistent);
    modern_tap(instance,1,3100);assert(!instance->motion.enclosure&&!instance->motion.gesture_persistent);
    modern_tap(instance,1,4000);assert(instance->motion.enclosure);
    hb_mo_input(&instance->motion,60,4,.01);assert(!instance->motion.enclosure);
    edge(instance,1,1,0,5000);assert(instance->motion.held&1);
    edge(instance,1,0,500,5500);assert(!instance->motion.held&&!instance->motion.enclosure);
    modern_tap(instance,3,6000);modern_tap(instance,1,6100);modern_tap(instance,2,6200);
    assert(!instance->motion.gesture_persistent&&instance->motion.enclosure==13);
    API.destroy_instance(instance);
}
static void continuous_once(void){
    Inst *instance=fixture();instance->travel_map=7;
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Velocity");API.set_param(instance,"motion_amount","-50");API.set_param(instance,"motion_enabled","Off");
    modern_tap(instance,1,1000);assert(instance->motion.gesture_once==1);
    played(instance,60);assert(instance->motion.held&1);release(instance,60);assert(!(instance->motion.held&1));
    modern_tap(instance,1,2000);modern_tap(instance,1,2100);assert(instance->motion.gesture_persistent&1);
    played(instance,60);release(instance,60);assert(instance->motion.held&1);
    modern_tap(instance,1,3000);modern_tap(instance,1,3100);assert(!(instance->motion.held&1));
    modern_tap(instance,1,4000);edge(instance,1,1,0,5000);edge(instance,1,0,500,5500);
    assert(!instance->motion.gesture_once&&!instance->motion.gesture_latched&&!instance->motion.held);
    API.set_param(instance,"motion_gesture_1","Touch");assert(instance->motion.held&1);
    API.set_param(instance,"motion_gesture_1","Up,500");assert(!instance->motion.held);
    API.destroy_instance(instance);
}
static void chord_expiry_and_lights(void){
    Inst *instance=setup();
    modern_tap(instance,5,1000);assert(instance->next_touch_mask&(1u<<4));
    modern_tap(instance,5,1100);assert(instance->motion.gesture_persistent&(1u<<4));assert(!instance->next_touch_mask);
    uint8_t notes[3]={62,65,69};hb_commit_observed_harmony(hb_infer_harmony(notes,3));hb_next_touch_clear_expired(instance);
    assert(instance->motion.held&(1u<<4));
    char lights[1024];unsigned active,persistent;
    API.get_param(instance,"motion_lights",lights,sizeof(lights));assert(sscanf(lights,"%u,%u",&active,&persistent)==2);
    assert((active&(1u<<4))&&(persistent&(1u<<4)));
    modern_tap(instance,5,2000);assert(!(instance->motion.held&(1u<<4)));
    modern_tap(instance,5,3000);assert(instance->next_touch_mask);
    notes[0]=64;notes[1]=67;notes[2]=71;hb_commit_observed_harmony(hb_infer_harmony(notes,3));hb_next_touch_clear_expired(instance);
    assert(!instance->motion.held&&!instance->motion.gesture_persistent);
    edge(instance,5,1,0,4000);notes[0]=60;notes[1]=64;notes[2]=67;
    hb_commit_observed_harmony(hb_infer_harmony(notes,3));hb_next_touch_clear_expired(instance);
    assert(instance->motion.held&(1u<<4));edge(instance,5,0,500,4500);assert(!instance->motion.held);
    API.destroy_instance(instance);
}

static void repeat_once_and_replacement(void){
    Inst *instance=setup();
    modern_tap(instance,1,1000);modern_tap(instance,1,1100);
    modern_tap(instance,14,2000);
    assert(!(instance->motion.gesture_persistent&1)&&!(instance->motion.gesture_latched&1));
    API.set_param(instance,"performance_reset","1");
    API.set_param(instance,"motion_lane","4");API.set_param(instance,"motion_operation","Auto Chord Repeat");
    modern_tap(instance,4,3000);assert(instance->player.repeat_override);
    played(instance,60);release(instance,60);advance(instance,2,64);
    assert(!instance->player.repeat_override&&!instance->motion.gesture_once);
    API.destroy_instance(instance);
}
int main(void){repeat_once_and_replacement();trigger_states();continuous_once();chord_expiry_and_lights();puts("double tap: immediate arm, promotion, off guard, momentary, source-use expiry, chord immunity and LEDs pass");return 0;}
