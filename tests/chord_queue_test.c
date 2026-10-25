#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main
static int running;
static double queue_beat(void){return running?position:-1;}
static int queue_clock(void){return running?MOVE_CLOCK_STATUS_RUNNING:0;}
static int held=-1;
static int take(Inst *conductor){
    char buffer[64];API.get_param(conductor,"hb_chord_queue_take",buffer,sizeof(buffer));
    if(!strcmp(buffer,"--")||!strcmp(buffer,"off"))return 0;
    int pitch=atoi(buffer);
    if(held>=0){uint8_t off[]={0x80,(uint8_t)held,0};assert(hb_selection_input(conductor,off,3));}
    uint8_t on[]={0x90,(uint8_t)pitch,100};assert(hb_selection_input(conductor,on,3));held=pitch;return 1;
}
static void choose(Inst *conductor,int root){
    char buffer[64];snprintf(buffer,sizeof(buffer),"%d,1,0,1,0,1,12",root);
    API.set_param(conductor,"hb_chord_queue",buffer);
}
int main(void){
    Inst *follower=fixture();host.get_clock_status=queue_clock;host.get_beat_position=queue_beat;position=0;running=0;
    Inst *conductor=API.create_instance("",0);API.set_param(conductor,"role","Conductor");
    API.set_param(conductor,"render_channel","Off");API.set_param(conductor,"chord_timing","1 Bar");
    choose(conductor,0);assert(take(conductor));choose(conductor,7);choose(conductor,5);
    assert(!take(conductor)&&g_chord_queue.count==2);assert(hb_render_harmony(follower).root_pc==0);
    running=1;assert(!take(conductor));assert(g_chord_queue.pending[0].at==4&&g_chord_queue.pending[1].at==8);
    API.set_param(follower,"next_lookahead","1/4");API.set_param(follower,"next_anti_buffer_ms","0 ms");
    position=2.9;assert(hb_render_harmony(follower).root_pc==0);
    position=3;assert(hb_render_harmony(follower).root_pc==7);
    assert(hb_live_selection(conductor).root_pc==0);assert(g_bus.next_model_count==0);
    assert(hb_next_effective_boundary_for(follower,2.5)==3);
    assert(hb_mt_harmony_at(conductor,4,3).root_pc==7);
    conductor->motif.editor.arrival=3;assert(hb_mt_arrival(conductor,3)==4);
    char buffer[8192];API.get_param(follower,"pad_harmony",buffer,sizeof(buffer));
    unsigned current,effective,scale;int ready;
    assert(sscanf(buffer,"%u,%u,%u,%d",&current,&effective,&scale,&ready)==4);
    assert(current==0x91&&effective==0x884&&ready);
    conductor->movy_playback=1;uint8_t clip[]={0x90,62,100};assert(hb_selection_input(conductor,clip,3));conductor->movy_playback=0;
    assert(hb_live_selection(conductor).root_pc==0);
    position=4;assert(take(conductor));assert(!take(conductor));assert(hb_live_selection(conductor).root_pc==7);
    API.set_param(follower,"next_lookahead","-1/4");
    position=4.5;assert(hb_render_harmony(follower).root_pc==0);
    position=5;assert(hb_render_harmony(follower).root_pc==7);
    position=8;assert(take(conductor));assert(!g_chord_queue.count);
    position=100;assert(!take(conductor));assert(hb_render_harmony(follower).root_pc==5);
    assert(!hb_cq_next().valid&&hb_mt_arrival(conductor,100)<0);
    choose(conductor,2);assert(g_chord_queue.pending[0].at==104);assert(!take(conductor));
    position=104;assert(take(conductor));assert(hb_live_selection(conductor).root_pc==2);
    /* Repeated choices are distinct attacks; full queues reject atomically. */
    for(int index=0;index<HB_CQ_CAPACITY;index++)choose(conductor,2);
    choose(conductor,3);assert(g_chord_queue.count==HB_CQ_CAPACITY&&g_chord_queue.result==2);
    API.set_param(conductor,"hb_chord_queue","bad");assert(g_chord_queue.count==HB_CQ_CAPACITY);
    API.set_param(conductor,"chord_timing","1/2");API.set_param(conductor,"anticipation","1/8 Early");
    assert(!take(conductor));assert(g_chord_queue.pending[0].at==105.5);
    position=105.5;assert(take(conductor));assert(hb_live_selection(conductor).root_pc==2);
    running=0;assert(!take(conductor));assert(!hb_cq_owner());
    /* Preload after a previous run; a fresh Play resets the master position. */
    choose(conductor,0);assert(take(conductor));choose(conductor,7);
    g_chord_queue.last_beat=105.5;position=0;running=1;
    assert(!take(conductor));assert(hb_cq_owner()&&g_chord_queue.count==1);
    assert(g_chord_queue.history[0].at==0&&g_chord_queue.pending[0].at==1.5);
    running=0;assert(!take(conductor));
    choose(conductor,0);assert(take(conductor));API.set_param(conductor,"hb_chord_queue","off");assert(!hb_cq_owner());
    choose(conductor,0);assert(take(conductor));API.set_param(conductor,"role","Follower");assert(!hb_cq_owner());
    API.destroy_instance(conductor);API.destroy_instance(follower);
    puts("Queued chords: seed, grid/anticipation, stopped preload, lookahead/late, motif arrival, empty hold, capacity, recorded isolation and lifecycle pass");
}
