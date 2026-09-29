#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"
static void knob(Inst *i,int lane,int elapsed,int stamp){
    char key[40],value[64];snprintf(key,sizeof(key),"motion_gesture_%d",lane);
    snprintf(value,sizeof(value),"Knob,%d",stamp);API.set_param(i,key,value);
    snprintf(value,sizeof(value),"Up,%d,%d",elapsed,stamp+elapsed);API.set_param(i,key,value);
}
int main(void){
    Inst *i=fixture();i->travel_map=7;
    API.set_param(i,"motion_lane","1");API.set_param(i,"motion_operation","Velocity");API.set_param(i,"motion_amount","-50");API.set_param(i,"motion_enabled","Off");
    knob(i,1,40,1000);knob(i,1,40,1100);
    assert(i->motion.gesture_once==1&&!i->motion.gesture_persistent);
    played(i,60);release(i,60);assert(!i->motion.held);
    knob(i,1,500,2000);assert(!i->motion.held&&!i->motion.gesture_once);
    API.set_param(i,"motion_gesture_1","Knob,3000");
    API.set_param(i,"motion_gesture_1","LatchOn");
    unsigned serial=i->motion.serial;
    API.set_param(i,"motion_gesture_1","LatchOn");assert(i->motion.serial==serial);
    API.set_param(i,"motion_gesture_1","Up,40,3040");assert(i->motion.gesture_persistent==1);
    knob(i,1,40,4000);knob(i,1,500,5000);assert(i->motion.gesture_persistent==1&&i->motion.held==1);
    played(i,60);release(i,60);assert(i->motion.held==1);
    API.set_param(i,"motion_gesture_1","LatchOff");assert(!i->motion.held&&!i->motion.gesture_persistent);
    API.destroy_instance(i);
    i=setup();
    for(int lane=1;lane<=5;lane++){
        if(lane==4)continue;
        char key[40];snprintf(key,sizeof(key),"motion_gesture_%d",lane);unsigned long long bit=1ULL<<(lane-1);
        API.set_param(i,"performance_reset","1");
        knob(i,lane,40,1000);knob(i,lane,40,1100);assert(!(i->motion.gesture_persistent&bit));
        API.set_param(i,key,"LatchOn");assert(i->motion.gesture_persistent&bit);
        for(int n=0;n<3;n++){played(i,60);release(i,60);assert(i->motion.gesture_persistent&bit);}
        knob(i,lane,40,2000);knob(i,lane,500,3000);assert(i->motion.gesture_persistent&bit);
        API.set_param(i,key,"LatchOff");assert(!(i->motion.gesture_persistent&bit));
        assert(!(hb_mo_pending_lanes(&i->motion)&bit));
    }
    API.destroy_instance(i);puts("knob latches: repeated taps, holds, directional turns, release ownership, persistent triggers pass");return 0;
}
