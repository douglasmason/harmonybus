/* Pad playback follows final emitted MIDI, including generated and recorded
 * notes. The input pool is intentionally not an output-light source. */
#define main existing_chord_tests
#include "chord_player_test.c"
#undef main
static uint16_t expected[128];
static void remember(int count){
    for(int i=0;i<count;i++)if(lengths[i]>=3){
        int type=output[i][0]&0xf0,channel=output[i][0]&15,pitch=output[i][1]&127;
        if(type==0x90&&output[i][2])expected[pitch]|=(uint16_t)(1u<<channel);
        else if(type==0x80||type==0x90)expected[pitch]&=(uint16_t)~(1u<<channel);
    }
}
static unsigned snapshot(Inst *instance,int transpose){
    char request[80]="pad_view@",view[4096];
    for(int slot=0;slot<32;slot++)snprintf(request+9+slot*2,3,"%02x",48+slot);
    API.get_param(instance,request,view,sizeof(view));
    const char *section=strstr(view,"|playpads1,");assert(section);
    unsigned mask=0,wanted=0;assert(sscanf(section,"|playpads1,%u",&mask)==1);
    for(int slot=0;slot<32;slot++)if(expected[48+slot+transpose])wanted|=1u<<slot;
    if(mask!=wanted){fprintf(stderr,"play mask %u != %u: %s\n",mask,wanted,view);assert(0);}
    return mask;
}
static Inst *play_fixture(void){
    Inst *instance=fixture();memset(expected,0,sizeof(expected));
    instance->content_map=1;instance->travel_map=7;
    API.set_param(instance,"boundary_buffer_ms","0 ms");
    API.set_param(instance,"chord_form","Triad");
    return instance;
}
static void send(Inst *instance,int on,int pitch){
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)pitch,(uint8_t)(on?100:0)};
    remember(API.process_midi(instance,message,3,output,lengths,64));
}
static void step(Inst *instance,int ms,int capacity){remember(advance(instance,ms,capacity));}
static void chord_and_recorded(void){
    for(int mode=1;mode<=2;mode++)for(int recorded=0;recorded<2;recorded++){
        Inst *instance=play_fixture();instance->player.config.mode=mode;instance->movy_playback=recorded;
        send(instance,1,60);assert(snapshot(instance,0)==0);
        for(int i=0;i<4;i++){step(instance,0,1);snapshot(instance,0);}
        assert(snapshot(instance,0)==((1u<<12)|(1u<<16)|(1u<<19)));
        send(instance,0,60);
        for(int i=0;i<4;i++){step(instance,0,1);snapshot(instance,0);}
        assert(snapshot(instance,0)==0);API.destroy_instance(instance);
    }
}
static void arp_gates_and_reinterpretation(void){
    unsigned modes[2]={0};
    for(int mode=1;mode<=2;mode++){
        Inst *instance=play_fixture();instance->player.config.mode=mode;instance->movy_playback=1;
        API.set_param(instance,"arp_playback","Repeat Arp");API.set_param(instance,"arp_hold","Latch");
        API.set_param(instance,"arp_rate","1/8");API.set_param(instance,"arp_gate","50");
        send(instance,1,62);send(instance,0,62);int hits=0,gaps=0;
        for(int i=0;i<30;i++){
            step(instance,i?25:0,64);unsigned mask=snapshot(instance,0);
            if(mask){assert(!(mask&(mask-1)));hits++;modes[mode-1]|=mask;}else gaps++;
        }
        assert(hits&&gaps);assert(instance->player.keys[0].used);
        API.set_param(instance,"chord_mode","Off");API.set_param(instance,"arp_playback","Off");
        step(instance,0,64);snapshot(instance,0);
        API.destroy_instance(instance);
    }
    assert(modes[0]!=modes[1]); /* The same recorded D has new generated pitches. */
}
static void motif_outputs_and_stop(void){
    Inst *instance=play_fixture();instance->motif.pending=2;instance->motif.cancel=0;
    instance->motif.events[0]=(hb_mt_scheduled){.used=1,.pitch=64,.velocity=100,.channel=0,.render=-1,.on=0,.off=.5};
    instance->motif.events[1]=(hb_mt_scheduled){.used=1,.pitch=67,.velocity=100,.channel=0,.render=-1,.on=.5,.off=1};
    step(instance,0,64);assert(snapshot(instance,0)==(1u<<16));
    step(instance,250,64);assert(snapshot(instance,0)==(1u<<19));
    transport=MOVE_CLOCK_STATUS_STOPPED;position=-1;step(instance,1,64);assert(snapshot(instance,0)==0);
    API.destroy_instance(instance);
}
static void transposed_output_and_channels(void){
    Inst *instance=play_fixture();instance->player.config.mode=1;
    API.set_param(instance,"transpose","12");send(instance,1,60);step(instance,0,64);
    assert(snapshot(instance,12)==((1u<<12)|(1u<<16)|(1u<<19)));
    send(instance,0,60);step(instance,0,64);assert(snapshot(instance,12)==0);
    API.set_param(instance,"role","Off");step(instance,0,64);
    uint8_t messages[][3]={{0x90,60,100},{0x91,60,100},{0x80,60,0},{0x91,60,0}};
    for(int i=0;i<4;i++){
        remember(API.process_midi(instance,messages[i],3,output,lengths,64));
        assert(snapshot(instance,0)==(i<3?1u<<12:0));
    }
    API.destroy_instance(instance);
}
static void delayed_motion_output(void){
    Inst *instance=play_fixture();
    API.set_param(instance,"motion_operation","MIDI Echo");
    API.set_param(instance,"motion_amount","2");
    API.set_param(instance,"motion_grid","1/8");
    API.set_param(instance,"motion_enabled","On");
    send(instance,1,60);step(instance,0,64);assert(snapshot(instance,0)==(1u<<12));
    send(instance,0,60);step(instance,0,64);assert(snapshot(instance,0)==0);
    int hits=0,gaps=0;
    for(int i=0;i<30;i++){step(instance,25,64);if(snapshot(instance,0))hits++;else gaps++;}
    assert(hits&&gaps);assert(snapshot(instance,0)==0);
    API.destroy_instance(instance);
}
static void stopped_opening_uses_live_mapping(void){
    const int roots[]={0,5,7,9};
    for(int running=0;running<2;running++)for(int point=0;point<4;point++){
        Inst *instance=play_fixture();instance->travel_map=0;
        Inst *conductor=API.create_instance("",NULL);
        API.set_param(conductor,"role","Conductor");
        API.set_param(conductor,"chord_mode","Scale Degree");
        API.set_param(conductor,"chord_form","Triad");
        API.set_param(conductor,"hb_opening_preview","1;62,0");
        uint8_t notes[3]={(uint8_t)(60+roots[point]),(uint8_t)(60+roots[point]+(point==3?3:4)),(uint8_t)(67+roots[point])};
        hb_commit_observed_harmony(hb_infer_harmony(notes,3));
        g_movy_running=running;position=running?0:-1;
        transport=running?MOVE_CLOCK_STATUS_RUNNING:MOVE_CLOCK_STATUS_STOPPED;
        assert(running||hb_opening_harmony().root_pc==2);
        /* The stopped Dm preview must not relabel a live D as the C pad to
           its left. Sounding C harmony still makes the D input emit D. */
        char request[80]="pad_view@",view[8192];
        for(int slot=0;slot<32;slot++)snprintf(request+9+slot*2,3,"%02x",slot<3?60+2*slot:255);
        int actual_pitch=hb_map_follower_note_now(instance,62);
        send(instance,1,62);step(instance,0,64);assert(expected[actual_pitch]);
        API.get_param(instance,request,view,sizeof(view));
        unsigned playing=0;const char *field=strstr(view,"|playpads1,");
        assert(field&&sscanf(field,"|playpads1,%u",&playing)==1);
        if(playing!=2)fprintf(stderr,"running=%d root=%d: D input lit slots %u, expected 2\n",running,roots[point],playing);
        assert(playing==2);
        send(instance,0,62);step(instance,0,64);step(instance,80,64);
        API.get_param(instance,request,view,sizeof(view));
        assert(strstr(view,"|playpads1,0|")&&strstr(view,"|playflash1,0"));
        API.destroy_instance(conductor);API.destroy_instance(instance);
    }
}
int main(void){stopped_opening_uses_live_mapping();chord_and_recorded();arp_gates_and_reinterpretation();motif_outputs_and_stop();transposed_output_and_channels();delayed_motion_output();puts("Rendered play pads: live/recorded chords, emitted capacity, arp gates/latch, reinterpretation, motifs/stop, transpose and channel ownership pass");return 0;}
