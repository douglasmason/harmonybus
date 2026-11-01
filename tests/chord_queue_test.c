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
static void snapshot(Inst *conductor,const char *tail){
    char buffer[1024];hb_chord_queue before=g_chord_queue;
    hb_cq_status(conductor,buffer,sizeof(buffer));
    assert(strlen(buffer)>=strlen(tail));assert(!strcmp(buffer+strlen(buffer)-strlen(tail),tail));
    assert(!memcmp(&before,&g_chord_queue,sizeof(before)));
    char tiny[4]={0,0,0,99};int full=hb_cq_status(conductor,tiny,3);
    assert(full==(int)strlen(buffer)&&tiny[2]==0&&tiny[3]==99);
}
static void sustained_loop(Inst *conductor){
    API.set_param(conductor,"hb_chord_queue","off");
    API.set_param(conductor,"chord_queue_depth","4");
    API.set_param(conductor,"chord_queue_end","Loop");
    API.set_param(conductor,"chord_timing","1 Bar");
    API.set_param(conductor,"anticipation","On Grid");
    running=0;position=0;held=-1;
    unsigned long long activations=g_chord_queue_diagnostics.activations;
    unsigned long long recoveries=g_chord_queue_diagnostics.recoveries;
    unsigned long long late=g_chord_queue_diagnostics.late;
    unsigned long long late_expected=0;
    const int roots[]={0,7,5,2};
    for(int index=0;index<4;index++)choose(conductor,roots[index]);
    assert(take(conductor));snapshot(conductor,"|0|4|C|G|F|D");
    char bank[1024]="",status[4096];
    for(int slot=0;slot<21;slot++)strcat(bank,slot?";0,1,0,1,0,1,12,0,0":"0,1,0,1,0,1,12,0,0");
    API.set_param(conductor,"hb_chord_bank",bank);
    API.get_param(conductor,"hb_chord_bank_snapshot",status,sizeof(status));
    assert(!strcmp(strrchr(status,','),",1")); /* C is in the loop, not pending. */
    running=1;assert(!take(conductor));
    for(int change=1;change<=2048;change++){
        double scheduled=change*4.0;
        position=scheduled-1.0/96.0;
        assert(!take(conductor));
        /* Sporadic late service after two clean loops must not move the grid. */
        position=scheduled+(change>8&&change%7==0?0.375:0.0);
        if(change>8&&change%7==0)late_expected++;
        assert(take(conductor));assert(!take(conductor));
        assert(hb_live_selection(conductor).root_pc==roots[change%4]);
        assert(g_chord_queue.pending[0].at==scheduled+4.0);
        assert(g_chord_queue.cycle_count==4&&g_chord_queue.history_count<=HB_CQ_HISTORY);
        assert(hb_cq_at(conductor,scheduled,0).root_pc==roots[change%4]);
        assert(hb_cq_at(conductor,scheduled-0.5,0).root_pc==roots[(change-1)%4]);
        if(change>64)assert(hb_cq_at(conductor,scheduled-63*4.0,0).root_pc==roots[(change-63)%4]);
    }
    assert(g_chord_queue_diagnostics.activations-activations==2049);
    assert(g_chord_queue_diagnostics.late-late==late_expected);
    assert(g_chord_queue_diagnostics.recoveries==recoveries);
    assert(g_chord_queue_diagnostics.max_late>=0.375);
    /* A missing voice between boundaries is separately attributed. Reads
       cannot consume a cue, reset counters, or mutate the timeline. */
    memset(conductor->player.keys,0,sizeof(conductor->player.keys));held=-1;
    assert(take(conductor));assert(g_chord_queue_diagnostics.recoveries==recoveries+1);
    assert(g_chord_queue_diagnostics.activations-activations==2049);
    hb_chord_queue before=g_chord_queue;
    char diagnostics[512],again[512],tiny[4]={0,0,0,99};
    API.get_param(conductor,"hb_chord_queue_diagnostics",diagnostics,sizeof(diagnostics));
    API.get_param(conductor,"hb_chord_queue_diagnostics",again,sizeof(again));
    assert(!strcmp(diagnostics,again)&&!strncmp(diagnostics,"qd1|",4));
    assert(!memcmp(&before,&g_chord_queue,sizeof(before)));
    assert(hb_cq_diagnostics(tiny,3)==(int)strlen(diagnostics)&&tiny[2]==0&&tiny[3]==99);
    /* A due chord must win over restoration of a voice invalidated by another
       operation. Otherwise every reset can replay the old chord before it. */
    position+=4.0;
    memset(conductor->player.keys,0,sizeof(conductor->player.keys));
    held=-1;
    assert(take(conductor));
    assert(hb_live_selection(conductor).root_pc==7);
    assert(g_chord_queue_diagnostics.recoveries==recoveries+1);
    API.set_param(conductor,"chord_queue_end","Hold");
    API.get_param(conductor,"hb_chord_bank_snapshot",status,sizeof(status));
    assert(!strcmp(strrchr(status,','),",0")); /* Hold shows only future members. */
    unsigned long long clears=g_chord_queue_diagnostics.clears;
    running=0;assert(!take(conductor));
    assert(g_chord_queue_diagnostics.clears==clears+1);
    assert(g_chord_queue_diagnostics.activations-activations==2050);
}
int main(void){
    Inst *follower=fixture();host.get_clock_status=queue_clock;host.get_beat_position=queue_beat;position=0;running=0;
    Inst *conductor=API.create_instance("",0);API.set_param(conductor,"role","Conductor");
    assert(conductor->chord_queue_depth==2);API.set_param(conductor,"chord_queue_depth","1");
    API.set_param(conductor,"render_channel","Off");API.set_param(conductor,"chord_timing","1 Bar");
    choose(conductor,0);assert(take(conductor));choose(conductor,7);choose(conductor,5);
    assert(!take(conductor)&&g_chord_queue.count==2);assert(hb_render_harmony(follower).root_pc==0);
    running=1;assert(!take(conductor));assert(g_chord_queue.pending[0].at==4&&g_chord_queue.pending[1].at==8);
    API.set_param(follower,"next_lookahead","1/4");API.set_param(follower,"next_anti_buffer_ms","0 ms");
    position=2.9;assert(hb_render_harmony(follower).root_pc==0);
    position=3;assert(hb_render_harmony(follower).root_pc==7);
    /* Enabling or changing lookahead during the first pass uses the queue,
       without a learned clip model or another traversal. */
    API.set_param(follower,"next_predict","Off");assert(hb_render_harmony(follower).root_pc==0);
    API.set_param(follower,"next_predict","On");assert(hb_render_harmony(follower).root_pc==7);
    API.set_param(follower,"next_lookahead","Off");assert(hb_render_harmony(follower).root_pc==0);
    API.set_param(follower,"next_lookahead","1/4");assert(hb_render_harmony(follower).root_pc==7);
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
    API.set_param(conductor,"role","Conductor");
    held=-1;
    API.set_param(conductor,"chord_queue_depth","4");
    API.set_param(conductor,"chord_timing","1 Bar");API.set_param(conductor,"anticipation","On Grid");
    API.set_param(follower,"next_lookahead","1/4");
    running=0;position=0;
    /* A partial batch is visible in status but never a scheduled prediction. */
    choose(conductor,0);choose(conductor,7);choose(conductor,5);
    assert(!take(conductor)&&g_chord_queue.waiting&&!hb_cq_next().valid);
    assert(g_chord_queue.pending[0].at==-1);
    API.get_param(conductor,"hb_chord_queue_status",buffer,sizeof(buffer));assert(strstr(buffer,"|1|4"));
    choose(conductor,2);assert(take(conductor));assert(hb_live_selection(conductor).root_pc==0);
    assert(g_chord_queue.count==3&&!g_chord_queue.waiting);
    running=1;assert(!take(conductor));
    for(int index=1;index<=3;index++){position=index*4;assert(take(conductor));}
    assert(g_chord_queue.waiting&&hb_live_selection(conductor).root_pc==2);
    choose(conductor,5);choose(conductor,7);choose(conductor,0);
    position=100;assert(!take(conductor)&&hb_render_harmony(follower).root_pc==2);
    assert(!hb_cq_next().valid&&hb_mt_arrival(conductor,100)<0);
    API.get_param(follower,"pad_harmony",buffer,sizeof(buffer));
    assert(sscanf(buffer,"%u,%u,%u,%d",&current,&effective,&scale,&ready)==4&&!ready&&current==effective);
    choose(conductor,9);assert(!g_chord_queue.waiting&&g_chord_queue.pending[0].at==104);
    position=103;assert(hb_render_harmony(follower).root_pc==5);assert(!take(conductor));
    position=104;assert(take(conductor)&&hb_live_selection(conductor).root_pc==5);
    /* Raising depth does not interrupt an already armed batch. */
    API.set_param(conductor,"chord_queue_depth","16");position=108;assert(take(conductor));
    position=112;assert(take(conductor));position=116;assert(take(conductor));
    for(int index=0;index<15;index++)choose(conductor,index%12);
    assert(!take(conductor)&&g_chord_queue.waiting);choose(conductor,3);
    assert(!g_chord_queue.waiting&&g_chord_queue.count==16&&g_chord_queue.pending[0].at==120);
    choose(conductor,4);assert(g_chord_queue.result==2&&g_chord_queue.count==16);
    /* Preferences persist; queued notes do not. */
    API.get_param(conductor,"state",buffer,sizeof(buffer));assert(strstr(buffer,";cqd1,16"));
    API.set_param(conductor,"hb_chord_queue","off");API.set_param(conductor,"chord_queue_depth","2");
    API.set_param(conductor,"state",buffer);assert(conductor->chord_queue_depth==16&&!hb_cq_owner());
    API.set_param(conductor,"chord_queue_depth","2");position=120;
    choose(conductor,0);assert(!take(conductor));choose(conductor,7);assert(!take(conductor));
    assert(g_chord_queue.pending[0].at==124);position=124;assert(take(conductor));
    position=128;assert(take(conductor));choose(conductor,5);assert(!take(conductor));
    /* Lowering the refill threshold arms on the host tick, not a UI read. */
    API.set_param(conductor,"chord_queue_depth","1");assert(g_chord_queue.waiting);
    assert(!take(conductor)&&g_chord_queue.pending[0].at==132);
    API.set_param(conductor,"hb_chord_queue","off");
    /* Loop the complete armed progression, with predictable refills and Off. */
    API.set_param(conductor,"chord_queue_end","Loop");API.set_param(conductor,"chord_queue_depth","2");
    running=0;held=-1;choose(conductor,0);assert(!take(conductor));choose(conductor,7);assert(take(conductor));
    running=1;position=0;assert(!take(conductor));
    assert(hb_cq_next().root_pc==7&&hb_harmony_knowledge_ready_for(follower));
    API.get_param(follower,"pad_harmony",buffer,sizeof(buffer));
    assert(sscanf(buffer,"%u,%u,%u,%d",&current,&effective,&scale,&ready)==4&&ready);
    position=3;API.set_param(follower,"next_predict","Off");assert(hb_render_harmony(follower).root_pc==0);
    API.set_param(follower,"next_predict","On");assert(hb_render_harmony(follower).root_pc==7);
    assert(g_bus.next_model_count==0); /* Still before the loop's first wrap. */
    position=4;assert(take(conductor)&&hb_live_selection(conductor).root_pc==7);
    assert(g_chord_queue.count==2&&!g_chord_queue.waiting&&g_chord_queue.pending[0].at==8);
    position=7;assert(hb_render_harmony(follower).root_pc==0);
    position=8;assert(take(conductor)&&hb_live_selection(conductor).root_pc==0);
    choose(conductor,5);assert(g_chord_queue.cycle_count==3);
    position=12;assert(take(conductor)&&hb_live_selection(conductor).root_pc==7);
    position=16;assert(take(conductor)&&hb_live_selection(conductor).root_pc==5&&g_chord_queue.count==3);
    API.set_param(conductor,"chord_queue_end","Hold");
    position=20;assert(take(conductor));position=24;assert(take(conductor));position=28;assert(take(conductor));
    position=40;assert(!take(conductor)&&g_chord_queue.waiting&&hb_live_selection(conductor).root_pc==5);
    API.set_param(conductor,"chord_queue_end","Loop");assert(!take(conductor)&&g_chord_queue.pending[0].at==44);
    API.set_param(conductor,"hb_chord_queue","off");assert(!hb_cq_owner());
    /* Gesture depth arms only when the shared touch phrase commits. */
    API.set_param(conductor,"chord_queue_end","Hold");API.set_param(conductor,"chord_queue_depth","Touch Phrase");
    running=0;held=-1;choose(conductor,0);choose(conductor,7);assert(!take(conductor)&&g_chord_queue.waiting);
    API.set_param(conductor,"hb_chord_queue","commit");assert(take(conductor)&&g_chord_queue.count==1);
    running=1;position=0;assert(!take(conductor));position=4;assert(take(conductor));
    choose(conductor,5);position=8;assert(!take(conductor)&&g_chord_queue.waiting);
    API.set_param(conductor,"hb_chord_queue","commit");assert(!take(conductor)&&g_chord_queue.pending[0].at==12);
    position=12;assert(take(conductor));API.set_param(conductor,"hb_chord_queue","off");
    /* Separate taps obey depth; overlapping release commits below it, and
       even depth one cannot start partway through a held entry gesture. */
    API.set_param(conductor,"chord_queue_depth","4");running=0;position=0;held=-1;
    API.set_param(conductor,"hb_chord_queue","begin");choose(conductor,0);
    assert(!take(conductor));API.set_param(conductor,"hb_chord_queue","end");
    assert(!take(conductor)&&g_chord_queue.waiting);
    API.set_param(conductor,"hb_chord_queue","begin");choose(conductor,7);
    assert(!take(conductor));API.set_param(conductor,"hb_chord_queue","commit");
    assert(take(conductor)&&g_chord_queue.count==1);
    API.set_param(conductor,"hb_chord_queue","off");
    API.set_param(conductor,"chord_queue_depth","1");API.set_param(conductor,"chord_queue_end","Loop");
    API.set_param(conductor,"hb_chord_queue","begin");choose(conductor,0);choose(conductor,0);
    assert(!take(conductor)&&g_chord_queue.waiting);
    snapshot(conductor,"|-1|2|C|C");
    API.set_param(conductor,"hb_chord_queue","commit");assert(take(conductor));
    snapshot(conductor,"|0|2|C|C");running=1;assert(!take(conductor));
    position=4;assert(take(conductor));snapshot(conductor,"|1|2|C|C");
    /* Appending after the last chord keeps the real cursor until next take. */
    choose(conductor,5);snapshot(conductor,"|1|3|C|C|F");
    position=8;assert(take(conductor));snapshot(conductor,"|0|3|C|C|F");
    position=12;assert(take(conductor));snapshot(conductor,"|1|3|C|C|F");
    position=16;assert(take(conductor));snapshot(conductor,"|2|3|C|C|F");
    /* Status reads at a future boundary cannot advance either queue or cursor. */
    position=20;snapshot(conductor,"|2|3|C|C|F");assert(take(conductor));snapshot(conductor,"|0|3|C|C|F");
    running=0;assert(!take(conductor));API.get_param(conductor,"hb_chord_queue_status",buffer,sizeof(buffer));assert(!strcmp(buffer,"--"));
    sustained_loop(conductor);
    API.destroy_instance(conductor);API.destroy_instance(follower);
    puts("Queued chords: seed, grid/anticipation, stopped preload, lookahead/late, motif arrival, empty hold, capacity, recorded isolation and lifecycle pass");
}
