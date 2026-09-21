#define main motion_regression_main
#include "chord_player_test.c"
#undef main
static Inst *motion_fixture(void){Inst *instance=fixture();API.set_param(instance,"boundary_buffer_ms","0 ms");return instance;}
static int send_note(Inst *instance,int on,int pitch){
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)pitch,(uint8_t)(on?100:0)};
    int count=API.process_midi(instance,message,3,output,lengths,64);
    return count+API.tick(instance,128,48000,output+count,lengths+count,64-count);
}
static void recorded_note(Inst *instance,int source,const char *saved,int expected){
    API.set_param(instance,"hb_movy_actions",strchr(saved,',')+1);
    API.set_param(instance,"hb_movy_playback","1");
    int count=send_note(instance,1,source);
    if(count!=1||output[0][1]!=expected){fprintf(stderr,"recorded source %d expected %d got count=%d pitch=%d\n",source,expected,count,output[0][1]);assert(0);}
    send_note(instance,0,source);API.set_param(instance,"hb_movy_playback","0");
}
int main(void){
    Inst *instance=motion_fixture();char saved[2048];
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Octave");API.set_param(instance,"motion_amount","1");
    assert(send_note(instance,1,60)==1&&output[0][1]==72);send_note(instance,0,60);
    API.get_param(instance,"hb_record_action",saved,sizeof(saved));assert(!strncmp(saved,"ra1,60,",7));
    API.set_param(instance,"motion_operation","Transpose");API.set_param(instance,"motion_amount","5");
    recorded_note(instance,60,saved,72); /* captured operation wins over reassignment */
    API.set_param(instance,"motion_enabled","Off");
    API.set_param(instance,"performance_gesture_below","Down");
    API.set_param(instance,"performance_gesture_below","Up,100");
    assert(send_note(instance,1,60)==1&&output[0][1]==59);send_note(instance,0,60);
    API.get_param(instance,"hb_record_action",saved,sizeof(saved));
    recorded_note(instance,60,saved,59);
    API.set_param(instance,"performance_gesture_above","Down");
    assert(send_note(instance,1,60)==1&&output[0][1]==62);send_note(instance,0,60);
    API.get_param(instance,"hb_record_action",saved,sizeof(saved));
    API.set_param(instance,"performance_gesture_above","Up,500");
    recorded_note(instance,60,saved,62);
    API.set_param(instance,"motion_operation","Pan");API.set_param(instance,"motion_amount","100");API.set_param(instance,"motion_enabled","On");
    send_note(instance,1,60);API.get_param(instance,"hb_record_action",saved,sizeof(saved));send_note(instance,0,60);
    API.set_param(instance,"motion_operation","Off");
    API.set_param(instance,"hb_movy_actions",strchr(saved,',')+1);API.set_param(instance,"hb_movy_playback","1");
    send_note(instance,1,60);assert(instance->motion_local.pan_dirty[0]);
    API.tick(instance,128,48000,output,lengths,64);assert(instance->motion_local.pan_dirty[0]);
    send_note(instance,0,60);API.tick(instance,128,48000,output,lengths,64);assert(!instance->motion_local.pan_dirty[0]);API.set_param(instance,"hb_movy_playback","0");
    Inst *other=API.create_instance("",NULL);API.set_param(other,"role","Follower");
    API.set_param(instance,"motion_operation","Velocity");
    assert(other->motion.lanes[0].operation==HB_MO_VELOCITY);
    API.set_param(instance,"next_lookahead","3/4");assert(instance->next_lookahead!=other->next_lookahead);
    API.set_param(instance,"quant_timing","1/4");assert(hb_quant_grid_beats_for(instance)==hb_quant_grid_beats_for(other));
    puts("recorded operations: relative replay, lane reassignment, held/tapped approaches, and global/local scopes pass");
}
