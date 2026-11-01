#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main
#include <time.h>
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
static void preview_snapshot(Inst *conductor){
    hb_chord_queue before=g_chord_queue;
    hb_harmony_t expected[HB_CQ_CAPACITY+3]={0};
    if(before.history_count){
        hb_cq_event event=before.history[before.history_count-1];
        expected[0]=hb_key_harmony(conductor,hb_selection_decode(event.word,event.note));
    }
    if(before.count){
        hb_cq_event event=before.pending[0];
        expected[1]=hb_key_harmony(conductor,hb_selection_decode(event.word,event.note));
    }
    int cycle=conductor->chord_queue_loop&&!before.waiting&&before.cycle_count;
    int count=cycle?before.cycle_count:before.count+(expected[0].valid?1:0);
    for(int index=0;index<count;index++){
        if(!cycle&&expected[0].valid&&index==0)expected[index+2]=expected[0];
        else {
            hb_cq_event event=cycle?before.cycle[index]:before.pending[index-(expected[0].valid?1:0)];
            expected[index+2]=hb_key_harmony(conductor,hb_selection_decode(event.word,event.note));
        }
    }
    hb_key_harmony_cache musical[4];memcpy(musical,conductor->key_cache,sizeof(musical));
    unsigned cursor=conductor->key_cache_cursor;
    for(int read=0;read<3;read++){
        char buffer[2048];hb_cq_status(conductor,buffer,sizeof(buffer));
        assert(!memcmp(&before,&g_chord_queue,sizeof(before)));
        assert(cursor==conductor->key_cache_cursor);
        assert(!memcmp(musical,conductor->key_cache,sizeof(musical)));
        int cell=0;
        for(char *token=strtok(buffer,"|");token;token=strtok(NULL,"|"),cell++){
            int index=cell==3?0:cell==4?1:cell>=11?cell-9:-1;
            if(index>=0){
                assert(index<count+2);
                assert(!strcmp(token,expected[index].valid?expected[index].name:"--"));
            }
        }
        assert(cell==11+count);
    }
}
static void queue_preview_check(Inst *conductor){
    hb_key_context saved=g_key_context;
    int transpose=g_bus.global_transpose,accidentals=g_bus.accidentals,range=conductor->chord_register;
    unsigned overrides=conductor->policy_overrides;
    int major=conductor->policy_values[HB_P_DOMINANT],minor=conductor->policy_values[HB_P_DOMINANT_MINOR];
    API.set_param(conductor,"hb_chord_queue","off");
    API.set_param(conductor,"chord_queue_depth","16");
    API.set_param(conductor,"chord_queue_end","Loop");
    running=0;position=0;held=-1;
    API.set_param(conductor,"hb_chord_queue","begin");
    for(int index=0;index<HB_CQ_CAPACITY;index++){
        char selection[64];snprintf(selection,sizeof(selection),"%d,%d,0,1,0,1,12",index%12,index<12?1:2);
        API.set_param(conductor,"hb_chord_queue",selection);
    }
    assert(g_chord_queue.waiting&&g_chord_queue.count==HB_CQ_CAPACITY);
    g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=9,
        .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(9,2)};
    preview_snapshot(conductor); /* Waiting: no current chord. */
    if(getenv("HB_QUEUE_STATUS_BENCH")){
        char status[2048];hb_cq_event event=g_chord_queue.pending[0];
        hb_harmony_t musical=hb_selection_decode(event.word,event.note);
        hb_key_harmony(conductor,musical);
        unsigned cursor=conductor->key_cache_cursor;
        struct timespec start,end;clock_gettime(CLOCK_MONOTONIC,&start);
        for(int read=0;read<10000;read++){
            hb_cq_status(conductor,status,sizeof(status));hb_key_harmony(conductor,musical);
        }
        clock_gettime(CLOCK_MONOTONIC,&end);
        double ns=(end.tv_sec-start.tv_sec)*1e9+end.tv_nsec-start.tv_nsec;
        printf("16-chord status + musical read: %.0f ns/read, %u musical-cache writes (desktop only)\n",ns/10000.,conductor->key_cache_cursor-cursor);
        assert(cursor==conductor->key_cache_cursor);
    }
    API.set_param(conductor,"hb_chord_queue","commit");assert(take(conductor));
    running=1;assert(!take(conductor));
    for(int change=0;change<32;change++){
        if(change){position=change*4.;assert(take(conductor));}
        g_bus.global_transpose=change%5-2;g_bus.accidentals=change%3;
        conductor->chord_register=change%7-3;
        conductor->policy_overrides|=(1u<<HB_P_DOMINANT)|(1u<<HB_P_DOMINANT_MINOR);
        conductor->policy_values[HB_P_DOMINANT]=conductor->policy_values[HB_P_DOMINANT_MINOR]=change%11;
        g_key_context.active=change%4!=0;g_key_context.target_root=change%12;
        g_key_context.target_mask=hb_explicit_scale_mask(change%12,change%19+1);
        preview_snapshot(conductor);
    }
    running=0;assert(!take(conductor));preview_snapshot(conductor);
    running=1;position=0;assert(take(conductor));preview_snapshot(conductor);
    /* Compare against a fresh mapping with unchanged key/descriptor inputs
       while spelling preferences change. */
    g_bus.global_transpose=0;
    g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=1,
        .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(1,1)};
    for(int spelling=0;spelling<3;spelling++){
        g_bus.accidentals=spelling;
        hb_cq_event event=g_chord_queue.cycle[0];
        hb_key_harmony_cache fresh={0};
        hb_harmony_t expected=hb_key_harmony_cached(conductor,hb_selection_decode(event.word,event.note),&fresh);
        char status[2048],prefix[128];hb_cq_status(conductor,status,sizeof(status));
        snprintf(prefix,sizeof(prefix),"cq1|%d|%.6f|%s|",g_chord_queue.count,g_chord_queue.pending[0].at,expected.name);
        assert(!strncmp(status,prefix,strlen(prefix)));
        hb_harmony_t musical=hb_key_harmony(conductor,hb_selection_decode(event.word,event.note));
        assert(!strcmp(expected.name,musical.name));
    }
    /* Hold can display the current chord plus sixteen future chords. */
    API.set_param(conductor,"chord_queue_end","Hold");choose(conductor,5);
    assert(g_chord_queue.count==HB_CQ_CAPACITY);preview_snapshot(conductor);
    API.set_param(conductor,"hb_chord_queue","off");
    g_key_context=saved;g_bus.global_transpose=transpose;g_bus.accidentals=accidentals;
    conductor->chord_register=range;conductor->policy_overrides=overrides;
    conductor->policy_values[HB_P_DOMINANT]=major;conductor->policy_values[HB_P_DOMINANT_MINOR]=minor;
    running=0;
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
static void selection_cache_check(Inst *conductor){
    int transpose=g_bus.global_transpose,accidentals=g_bus.accidentals,range=conductor->chord_register;
    for(int root=0;root<12;root++)for(int quality=1;quality<=12;quality++)for(int form=0;form<=16;form++){
        int fields[9]={root,quality,form,1,0,1,12,0,0},note=0;
        unsigned long long word=hb_selection_resolve(conductor,fields,&note);if(!word)continue;
        for(int shift=-12;shift<=12;shift+=6)for(int reg=-1;reg<=1;reg++){
            g_bus.global_transpose=shift;conductor->chord_register=reg;
            unsigned long long ranged=reg?hb_selection_with_register(word,&note,reg):word;
            for(int spelling=0;spelling<3;spelling++){
                g_bus.accidentals=spelling;conductor->chord_register=spelling-1;
                hb_harmony_t reference=hb_selection_decode_uncached(ranged,note);
                for(int read=0;read<2;read++){
                    hb_harmony_t cached=hb_selection_decode(ranged,note),expected=reference;
                    assert(!strcmp(cached.name,expected.name));
                    memset(cached.name,0,sizeof(cached.name));memset(expected.name,0,sizeof(expected.name));
                    assert(!memcmp(&cached,&expected,sizeof(cached)));
                }
            }
        }
    }
    g_bus.global_transpose=transpose;g_bus.accidentals=accidentals;conductor->chord_register=range;
    if(getenv("HB_QUEUE_DECODE_BENCH")){
        int fields[9]={0,1,0,1,0,1,12,0,0},note=0;
        unsigned long long word=hb_selection_resolve(conductor,fields,&note);
        volatile unsigned checksum=0;
        for(int cached=0;cached<2;cached++){
            struct timespec start,end;clock_gettime(CLOCK_MONOTONIC,&start);
            for(int index=0;index<100000;index++){
                hb_harmony_t harmony=cached?hb_selection_decode(word,note):hb_selection_decode_uncached(word,note);
                checksum+=harmony.pitch_mask;
            }
            clock_gettime(CLOCK_MONOTONIC,&end);
            double ns=(end.tv_sec-start.tv_sec)*1e9+end.tv_nsec-start.tv_nsec;
            printf("Selection decode %s: %.0f ns/read (desktop only)\n",cached?"cached":"uncached",ns/100000.0);
        }
        assert(checksum>0);
    }
}
static void late_first_cue(Inst *conductor){
    API.set_param(conductor,"hb_chord_queue","off");
    API.set_param(conductor,"chord_queue_depth","4");
    API.set_param(conductor,"chord_queue_end","Loop");
    API.set_param(conductor,"chord_timing","1 Bar");
    API.set_param(conductor,"anticipation","On Grid");
    running=1;position=0;held=-1;
    const int roots[]={0,7,5,2};
    for(int index=0;index<4;index++)choose(conductor,roots[index]);
    assert(!take(conductor));
    assert(g_chord_queue.pending[0].at==4);
    unsigned long long late=g_chord_queue_diagnostics.late;
    /* Stall across two deadlines before the first attack. Servicing a seed
       must preserve its due time just like every subsequent queue event. */
    position=9;assert(take(conductor));
    assert(g_chord_queue.history[0].at==4);
    assert(g_chord_queue_diagnostics.last_due==4&&g_chord_queue_diagnostics.last_actual==9);
    assert(g_chord_queue_diagnostics.late==late+1);
    assert(hb_live_selection(conductor).root_pc==0);
    assert(take(conductor));
    assert(g_chord_queue.history[1].at==8);
    assert(hb_live_selection(conductor).root_pc==7);
    assert(hb_cq_at(conductor,7.5,0).root_pc==0);
    assert(hb_cq_at(conductor,8.5,0).root_pc==7);
    assert(g_chord_queue.pending[0].at==12&&!take(conductor));
    position=12;assert(take(conductor));
    assert(hb_live_selection(conductor).root_pc==5);
    API.set_param(conductor,"hb_chord_queue","off");
    running=0;
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
    running=0;assert(!take(conductor));snapshot(conductor,"|0|3|C|C|F");
    memset(conductor->player.keys,0,sizeof(conductor->player.keys));held=-1;
    for(int tick=0;tick<100;tick++)assert(!take(conductor));
    assert(g_chord_queue.paused&&g_chord_queue.count==2);
    running=1;position=0;assert(take(conductor));snapshot(conductor,"|0|3|C|C|F");
    assert(g_chord_queue.pending[0].at==4&&!g_chord_queue.paused);
    position=4;assert(take(conductor));snapshot(conductor,"|1|3|C|C|F");
    position=8;assert(take(conductor));snapshot(conductor,"|2|3|C|C|F");
    /* Metadata arrives before the bus's conductor reconciliation. Queue
       servicing must see Stop/Play even when the shared clock is stale. */
    hb_movy_clip_t *metadata=&g_movy_clips[conductor-g_pool];
    metadata->present=1;metadata->ppqn=96;metadata->tick=8*96;metadata->running=0;
    assert(!take(conductor)&&g_chord_queue.paused); /* host still says running */
    memset(conductor->player.keys,0,sizeof(conductor->player.keys));held=-1;
    metadata->running=1;metadata->tick=0;running=0;
    assert(take(conductor)&&!g_chord_queue.paused); /* host still says stopped */
    snapshot(conductor,"|0|3|C|C|F");assert(g_chord_queue.pending[0].at==4);
    memset(metadata,0,sizeof(*metadata));
    running=0;assert(!take(conductor));API.set_param(conductor,"hb_chord_queue","off");
    API.get_param(conductor,"hb_chord_queue_status",buffer,sizeof(buffer));assert(!strcmp(buffer,"--"));
    sustained_loop(conductor);
    late_first_cue(conductor);
    queue_preview_check(conductor);
    selection_cache_check(conductor);
    API.destroy_instance(conductor);API.destroy_instance(follower);
    puts("Queued chords: seed, grid/anticipation, stopped preload, lookahead/late, motif arrival, empty hold, capacity, preview cache isolation, recorded isolation and lifecycle pass");
}
