/* Production API and scheduler regressions, including emitted render packets. */
#include <assert.h>
#include <math.h>
#include <sys/mman.h>
#include <unistd.h>
#include "../modules/harmonybus/dsp/harmonybus.c"
static double position;
static float tempo=120;
static int transport=2,render_count;
static uint8_t rendered[512][4];
static uint8_t recorded[512][4];static int recorded_count;
static double test_beat(void){return position;}
static float test_bpm(void){return tempo;}
static int test_clock(void){return transport;}
static int test_inject(const uint8_t *packet,int length){assert(length==4);
    if((packet[0]&0xf0)==0x30){if(recorded_count<512)memcpy(recorded[recorded_count++],packet,4);}
    else if(render_count<512)memcpy(rendered[render_count++],packet,4);
    return 4;
}
static host_api_v1_t host={.sample_rate=48000,.get_beat_position=test_beat,.get_bpm=test_bpm,.get_clock_status=test_clock,.midi_inject_to_move=test_inject};
static hb_global_shared_t globals;
static uint8_t output[64][3];static int lengths[64];
static Inst *fixture(void){
    g_init=0;g_global_shared=&globals;memset(&globals,0,sizeof(globals));
    memset(g_movy_clips,0,sizeof(g_movy_clips));g_movy_present=g_movy_blocked=0;
    g_movy_revision=14695981039346656037ULL;
    position=0;tempo=120;transport=2;render_count=recorded_count=0;
    move_midi_fx_init(&host);
    Inst *instance=API.create_instance("",NULL);
    instance->content_map=0; /* Legacy tests explicitly exercise Chord content. */
    instance->next_anti_buffer_ms=0; /* Legacy timing fixture: no onset guard. */
    API.set_param(instance,"role","Follower");API.set_param(instance,"source_channel","1");
    API.set_param(instance,"render_channel","4");
    API.set_param(instance,"follower_root_policy","Explicit");API.set_param(instance,"follower_explicit_root","C");
    API.set_param(instance,"follower_scale","Major");
    uint8_t chord[4]={60,64,67,71};hb_harmony_t harmony=hb_infer_harmony(chord,4);
    hb_commit_observed_harmony(harmony);
    instance->retrigger_held=0; /* Legacy fixtures explicitly retain Off. */
    instance->player.config.phase=0; /* Legacy lifecycle fixtures request immediate start explicitly. */
    return instance;
}
static void midi(Inst *instance,int on,int note){uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)note,(uint8_t)(on?100:0)};API.process_midi(instance,message,3,output,lengths,64);}
static int advance(Inst *instance,int milliseconds,int capacity){position+=milliseconds*tempo/60000.0;return API.tick(instance,milliseconds*48,48000,output,lengths,capacity);}
static void expect_notes(const int *actual,const int *expected,int count){for(int index=0;index<count;index++)assert(actual[index]==expected[index]);}
static void voicings(void){
    hb_cp_config config;hb_cp_defaults(&config);config.mode=2;
    unsigned chord=(1u<<0)|(1u<<4)|(1u<<7)|(1u<<11),scale=0xAB5;
    int notes[12];const int first[4]={52,55,59,60},spread[4]={52,55,60,71};
    assert(hb_cp_voice(config,52,0,chord,scale,notes)==4);expect_notes(notes,first,4);
    config.voicing=1;assert(hb_cp_voice(config,52,0,chord,scale,notes)==4);expect_notes(notes,spread,4);
    config.voicing=0;assert(hb_cp_voice(config,53,0,chord,scale,notes)==4);expect_notes(notes,first,4);
    config.mode=1;const int d_minor[3]={50,53,57};
    assert(hb_cp_voice(config,50,0,chord,scale,notes)==3);expect_notes(notes,d_minor,3);
    config.size=3;const int d_minor7[4]={50,53,57,60};
    assert(hb_cp_voice(config,50,0,chord,scale,notes)==4);expect_notes(notes,d_minor7,4);
    config.inversion=2;const int inverted[4]={41,45,48,50};
    assert(hb_cp_voice(config,50,0,chord,scale,notes)==4);expect_notes(notes,inverted,4);
    /* Every inversion and spread stays sorted, unique, in range, and in chord. */
    config.mode=2;
    for(int inversion=0;inversion<5;inversion++)for(int voicing=0;voicing<3;voicing++)for(int input=0;input<128;input++){
        config.inversion=inversion;config.voicing=voicing;
        int count=hb_cp_voice(config,input,0,chord,scale,notes);assert(count==4);
        for(int index=0;index<count;index++){
            assert(notes[index]>=0&&notes[index]<128&&(chord&(1u<<(notes[index]%12))));
            if(index)assert(notes[index]>notes[index-1]);
        }
    }
}
static void forms_and_shells(void){
    hb_cp_config config;hb_cp_defaults(&config);config.mode=1;
    const int expected[12][8]={
        {60,64,67,-1},{60,67,-1},{60,64,67,-1},{60,64,67,71,-1},
        {60,62,64,67,71,-1},{60,62,64,67,-1},{60,64,67,69,-1},
        {60,62,64,67,69,-1},{60,62,64,65,67,71,-1},
        {60,62,64,65,67,69,71,-1},{60,62,67,-1},{60,65,67,-1}
    };
    int notes[12];unsigned cmaj7=(1u<<0)|(1u<<4)|(1u<<7)|(1u<<11);
    for(int form=0;form<12;form++){
        config.size=form;int count=hb_cp_voice(config,60,0,cmaj7,0xAB5,notes);
        int wanted=0;while(expected[form][wanted]>=0)wanted++;
        assert(count==wanted);expect_notes(notes,expected[form],count);
    }
    config.mode=2;config.size=4;config.voicing=3;
    const int shell[3]={60,64,71};assert(hb_cp_voice(config,60,0,cmaj7,0xAB5,notes)==3);expect_notes(notes,shell,3);
    const int ninth_bass[4]={62,64,71,72};assert(hb_cp_voice(config,62,0,cmaj7,0xAB5,notes)==4);expect_notes(notes,ninth_bass,4);
    config.size=6;const int sixth_shell[3]={60,64,69};assert(hb_cp_voice(config,60,0,cmaj7,0xAB5,notes)==3);expect_notes(notes,sixth_shell,3);
    config.size=3;unsigned cm7=(1u<<0)|(1u<<3)|(1u<<7)|(1u<<10);
    const int minor_shell[3]={60,63,70};assert(hb_cp_voice(config,60,0,cm7,0x5AD,notes)==3);expect_notes(notes,minor_shell,3);
    /* Every form/inversion/voicing and MIDI-edge anchor remains bounded,
       ordered, unique and within the selected parent collection. */
    config.mode=1;
    for(int form=0;form<12;form++)for(int inversion=0;inversion<8;inversion++)for(int voicing=0;voicing<4;voicing++){
        config.size=form;config.inversion=inversion;config.voicing=voicing;
        for(int input=0;input<128;input++){
            int count=hb_cp_voice(config,input,0,cmaj7,0xAB5,notes);assert(count>0&&count<=7);
            for(int index=0;index<count;index++){
                assert(notes[index]>=0&&notes[index]<128);
                assert(notes[index]%12==input%12||(0xAB5&(1u<<(notes[index]%12))));
                if(index)assert(notes[index]>notes[index-1]);
            }
        }
    }
}
static void ownership(void){
    Inst *instance=fixture();API.set_param(instance,"chord_mode","Conductor Chord");
    midi(instance,1,60);assert(advance(instance,0,64)==4);assert(render_count==4);
    for(int index=0;index<4;index++)assert(rendered[index][2]==output[index][1]&&rendered[index][1]==0x93);
    midi(instance,1,64);assert(advance(instance,0,64)==1); /* shared E/G/B, new high C */
    midi(instance,0,60);assert(advance(instance,0,64)==1&&output[0][1]==60&&output[0][0]==0x80);
    midi(instance,0,64);assert(advance(instance,0,64)==4);assert(instance->player.sounding_count==0);
    /* Identical retrigger produces an OFF/ON pair, without orphaning voices. */
    midi(instance,1,60);assert(advance(instance,0,64)==4);
    midi(instance,1,60);assert(advance(instance,0,64)==8);
    API.set_param(instance,"render_channel","5");
    int total=0;for(int tick=0;tick<4;tick++)total+=advance(instance,0,1);
    assert(total==4&&instance->player.sounding_count==0);
    for(int index=render_count-4;index<render_count;index++)assert(rendered[index][1]==0x83);
    API.destroy_instance(instance);
}
static void strum(void){
    Inst *instance=fixture();API.set_param(instance,"chord_mode","Conductor Chord");
    API.set_param(instance,"arp_playback","Once");API.set_param(instance,"strum_spread","150 ms");
    midi(instance,1,60);assert(advance(instance,0,64)==1&&output[0][1]==60);
    assert(advance(instance,49,64)==0);assert(advance(instance,1,64)==1&&output[0][1]==64);
    midi(instance,0,60);assert(advance(instance,0,64)==2);
    assert(advance(instance,200,64)==0); /* pending G/B cancelled */
    API.set_param(instance,"strum_spread","1/4");
    for(int trial=0;trial<2;trial++){
        tempo=trial?60:120;midi(instance,1,60);assert(advance(instance,0,64)==1);
        assert(advance(instance,trial?1000:500,64)==3);
        midi(instance,0,60);assert(advance(instance,0,64)==4);
    }
    API.destroy_instance(instance);
}
static void shuffle_cycles(void){
    for(int phase=0;phase<3;phase++)for(int count=1;count<=12;count++){
        hb_chord_player player={0};hb_cp_defaults(&player.config);
        player.config.playback=1;player.config.phase=phase;player.config.order=5;player.config.rate=15;
        for(int index=0;index<count;index++){int note=60+index;hb_cp_on(&player,note,0,100,&note,1);}
        player.beat=0;hb_cp_tick(&player,output,lengths,64);
        unsigned seen=0;int hits=0;
        if(player.running==1){seen=1u<<(player.arp_note-60);hits=1;}
        while(hits<count*5){
            if(hits%count==0)seen=0;
            player.beat=player.next_beat;hb_cp_tick(&player,output,lengths,64);
            unsigned bit=1u<<(player.arp_note-60);assert(!(seen&bit));seen|=bit;hits++;
            if(hits%count==0)assert(seen==((1u<<count)-1u));
        }
    }
    Inst *instance=fixture();API.set_param(instance,"arp_order","Shuffle");
    char saved[16384];API.get_param(instance,"state",saved,sizeof(saved));
    API.set_param(instance,"arp_order","Up");API.set_param(instance,"state",saved);
    assert(instance->player.config.order==5);API.destroy_instance(instance);
}
static void cycle_rate(void){
    for(int order=0;order<6;order++)for(int count=1;count<=5;count++){
        hb_chord_player player={0};hb_cp_defaults(&player.config);
        player.config.playback=1;player.config.phase=0;player.config.order=order;
        player.config.rate=15; /* one bar per cycle */
        for(int index=0;index<count;index++){
            int note=60+index;assert(hb_cp_on(&player,note,0,100,&note,1));
        }
        int steps=order==2&&count>1?2*count-2:count;
        for(int step=0;step<=steps;step++){
            player.beat=4.0*step/steps;
            int events=hb_cp_tick(&player,output,lengths,64),ons=0;
            for(int event=0;event<events;event++)if((output[event][0]&0xf0)==0x90)ons++;
            assert(ons==1);
            assert(fabs(player.next_beat-(4.0*(step+1)/steps))<1e-8);
        }
        // Ordinary 1 Bar is still one step per bar, regardless of pool size.
        hb_cp_clear(&player);hb_cp_tick(&player,output,lengths,64);
        player.config.rate=6;player.beat=0;
        for(int index=0;index<count;index++){int note=60+index;hb_cp_on(&player,note,0,100,&note,1);}
        hb_cp_tick(&player,output,lengths,64);assert(player.next_beat==4.0);
    }
    Inst *instance=fixture();API.set_param(instance,"arp_rate","Cycle 1 Bar");
    assert(instance->player.config.rate==15);
    char saved[16384],value[64];API.get_param(instance,"state",saved,sizeof(saved));
    API.set_param(instance,"arp_rate","1/16");API.set_param(instance,"state",saved);
    assert(instance->player.config.rate==15);
    API.get_param(instance,"arp_rate",value,sizeof(value));assert(!strcmp(value,"Cycle 1 Bar"));
    API.destroy_instance(instance);
}
static void latch_replacement_with_off(void){
    for(int phase=0;phase<3;phase++){
        Inst *instance=fixture();
        API.set_param(instance,"arp_playback","Repeat Arp");
        API.set_param(instance,"travel_map","Direct");
        API.set_param(instance,"arp_hold","Latch with Off");
        instance->player.config.phase=phase;
        char state[16384];API.get_param(instance,"state",state,sizeof(state));
        API.set_param(instance,"arp_hold","Momentary");API.set_param(instance,"state",state);
        assert(instance->player.config.latch==2);
        hb_cp_entry entries[HB_CP_KEYS*HB_CP_VOICES*4];
        midi(instance,1,60);advance(instance,0,64);
        midi(instance,1,64);advance(instance,0,64);
        assert(hb_cp_entries(&instance->player,entries,0)==2);
        midi(instance,0,60);midi(instance,0,64);advance(instance,0,64);
        if(phase==1)advance(instance,125,64);
        assert(instance->player.sounding[0][60]);
        double next=instance->player.next_beat;
        midi(instance,1,60);advance(instance,0,64);
        assert(!instance->player.sounding[0][60]);
        assert(instance->player.next_beat==next);
        assert(hb_cp_entries(&instance->player,entries,0)==1&&entries[0].pitch==64);
        midi(instance,0,60);advance(instance,0,64);
        // New gesture replaces E, rather than accumulating G beside it.
        midi(instance,1,67);advance(instance,0,64);
        assert(hb_cp_entries(&instance->player,entries,0)==1&&entries[0].pitch==67);
        // Still-held G groups the next input, independent of elapsed time.
        advance(instance,300,64);midi(instance,1,71);advance(instance,0,64);
        assert(hb_cp_entries(&instance->player,entries,0)==2);
        midi(instance,0,67);midi(instance,0,71);advance(instance,0,64);
        midi(instance,1,71);midi(instance,0,71);advance(instance,0,64);
        assert(hb_cp_entries(&instance->player,entries,0)==1&&entries[0].pitch==67);
        midi(instance,1,67);midi(instance,0,67);advance(instance,0,64);
        assert(!instance->player.sounding_count&&!instance->player.running);
        assert(hb_cp_entries(&instance->player,entries,0)==0);
        midi(instance,1,60);midi(instance,1,64);advance(instance,125,64);
        API.set_param(instance,"arp_clear","Clear");advance(instance,0,64);
        assert(hb_cp_entries(&instance->player,entries,0)==0);
        assert(!instance->player.sounding_count);
        midi(instance,0,60);midi(instance,0,64);assert(advance(instance,500,64)==0);
        API.destroy_instance(instance);
    }
}
static void latch_acc_with_off(void){
    for(int phase=0;phase<3;phase++){
        Inst *instance=fixture();
        API.set_param(instance,"arp_playback","Repeat Arp");
        API.set_param(instance,"travel_map","Direct");
        API.set_param(instance,"arp_hold","Latch Acc. with Off");
        instance->player.config.phase=phase;
        assert(instance->player.config.latch==3);
        char state[16384],value[64];
        API.get_param(instance,"state",state,sizeof(state));
        API.set_param(instance,"arp_hold","Momentary");
        API.set_param(instance,"state",state);
        assert(instance->player.config.latch==3);
        API.get_param(instance,"arp_hold",value,sizeof(value));
        assert(!strcmp(value,"Latch Acc. with Off"));
        midi(instance,1,60);midi(instance,0,60);
        advance(instance,0,64);
        midi(instance,1,64);midi(instance,0,64);
        advance(instance,0,64);
        hb_cp_entry entries[HB_CP_KEYS*HB_CP_VOICES*4];
        assert(hb_cp_entries(&instance->player,entries,0)==2);
        if(phase==1)advance(instance,125,64);
        assert(instance->player.sounding[0][60]);
        double next=instance->player.next_beat;
        midi(instance,1,60);
        assert(advance(instance,0,64)==1&&output[0][0]==0x80&&output[0][1]==60);
        assert(instance->player.next_beat==next);
        assert(hb_cp_entries(&instance->player,entries,0)==1);
        midi(instance,0,60);assert(advance(instance,0,64)==0);
        assert(hb_cp_entries(&instance->player,entries,0)==1);
        advance(instance,125,64);assert(instance->player.sounding[0][64]);
        midi(instance,1,64);advance(instance,0,64);
        assert(!instance->player.sounding_count&&!instance->player.running);
        midi(instance,0,64);assert(advance(instance,500,64)==0);
        midi(instance,1,67);midi(instance,0,67);advance(instance,0,64);
        if(phase==1)advance(instance,125,64);
        assert(instance->player.sounding[0][67]);
        API.set_param(instance,"arp_clear","Clear");advance(instance,0,64);
        assert(!instance->player.sounding_count);
        API.destroy_instance(instance);
    }
    /* Two raw keys may own the same transformed tone. Removing either key
       must retain the other owner's sound; channels remain independent. */
    hb_chord_player player={0};hb_cp_defaults(&player.config);
    player.config.playback=1;player.config.latch=3;player.config.phase=0;
    int pitch=72;
    hb_cp_on(&player,60,0,100,&pitch,1);hb_cp_off(&player,60,0);
    hb_cp_on(&player,64,0,100,&pitch,1);hb_cp_off(&player,64,0);
    assert(hb_cp_tick(&player,output,lengths,64)==1);
    hb_cp_on(&player,60,0,100,&pitch,1);
    assert(hb_cp_tick(&player,output,lengths,64)==0&&player.sounding[0][72]);
    hb_cp_on(&player,64,1,100,&pitch,1);
    hb_cp_on(&player,64,0,100,&pitch,1);
    assert(hb_cp_tick(&player,output,lengths,64)==1&&output[0][0]==0x80);
    assert(player.keys[0].used||player.keys[1].used);
    hb_cp_clear(&player);hb_cp_tick(&player,output,lengths,64);
}
static void arp_and_latch(void){
    Inst *instance=fixture();API.set_param(instance,"arp_playback","Repeat Arp");
    API.set_param(instance,"travel_map","Direct"); /* isolate scheduler/latch pitch ownership */
    API.set_param(instance,"arp_hold","Latch");
    midi(instance,1,60);midi(instance,1,64);midi(instance,1,67);
    assert(advance(instance,0,64)==1&&output[0][1]==60);
    midi(instance,0,60);midi(instance,0,64);midi(instance,0,67);
    assert(advance(instance,63,64)==1&&output[0][0]==0x80);
    assert(advance(instance,62,64)==1&&output[0][1]==64);
    midi(instance,1,69);assert(advance(instance,0,64)==0);
    assert(instance->player.sounding[0][64]); /* Complete the existing hit. */
    midi(instance,0,69);advance(instance,63,64);assert(advance(instance,62,64)==1&&output[0][1]==69);
    uint8_t stop[1]={0xFC};API.process_midi(instance,stop,1,output,lengths,64);
    assert(advance(instance,0,64)==1&&output[0][0]==0x80);
    assert(advance(instance,500,64)==0);
    API.destroy_instance(instance);
}
static hb_harmony_t infer4(int first,int second,int third,int fourth){
    uint8_t notes[4]={(uint8_t)first,(uint8_t)second,(uint8_t)third,(uint8_t)fourth};return hb_infer_harmony(notes,4);
}
static void dominant_shift(void){
    Inst *instance=fixture();API.set_param(instance,"follower_scale","Natural Minor");
    hb_harmony_t g7=infer4(55,59,62,65),gminor=infer4(55,58,62,65),bb=infer4(58,62,65,69),leading=infer4(59,62,65,68);
    assert(g7.root_pc==7&&leading.root_pc==11);
    assert(!hb_dominant_scale_mask(instance,g7,0));
    API.set_param(instance,"dominant_scale","Harmonic Minor");
    assert(hb_dominant_scale_mask(instance,g7,0)==hb_explicit_scale_mask(0,8));
    assert(hb_dominant_scale_mask(instance,leading,0)==hb_explicit_scale_mask(0,8));
    assert(!hb_dominant_scale_mask(instance,gminor,0)&&!hb_dominant_scale_mask(instance,bb,0));
    hb_harmony_t target=hb_follower_scale_target(instance,g7);
    assert(target.pitch_mask&(1u<<11));assert(!(target.pitch_mask&(1u<<10)));
    hb_commit_observed_harmony(g7);instance->content_map=1;
    /* Input D remains degree 2. Harmonic minor gives G's b9 (Ab),
       melodic minor its natural 9 (A). Source labels do not change. */
    assert(!strcmp(hb_follower_degree_role_for_note(instance,62),"2nd"));
    assert(hb_map_follower_note_now(instance,62)%12==8);
    API.set_param(instance,"dominant_scale","Melodic Minor");
    assert(hb_map_follower_note_now(instance,62)%12==9);
    API.set_param(instance,"dominant_scale","Altered V");
    unsigned altered=hb_explicit_scale_mask(8,9);
    assert(hb_dominant_scale_mask(instance,g7,0)==altered);
    target=hb_follower_scale_target(instance,g7);
    assert(target.pitch_mask==(altered|hb_harmony_chord_mask(g7)));
    assert(hb_map_follower_note_now(instance,62)%12==8);
    assert(hb_map_follower_note_now(instance,63)%12==11); /* source minor third -> chord major third, not #9 */
    assert(hb_map_follower_note_now(instance,67)%12==2); /* preserve recognized perfect fifth */
    assert(hb_dominant_scale_mask(instance,leading,0)==hb_explicit_scale_mask(0,8));
    API.set_param(instance,"chord_mode","Conductor Chord");API.set_param(instance,"chord_form","Ninth");
    midi(instance,1,55);assert(advance(instance,0,64)==5);
    int saw_flat9=0;for(int index=0;index<5;index++)if(output[index][1]%12==8)saw_flat9=1;assert(saw_flat9);
    char state[512],restored[512];API.get_param(instance,"state",state,sizeof(state));
    Inst *copy=API.create_instance("",NULL);API.set_param(copy,"state",state);API.get_param(copy,"state",restored,sizeof(restored));
    assert(!strcmp(state,restored)&&hb_shared_dominant_scale()==3);
    API.destroy_instance(copy);API.destroy_instance(instance);
}
static void release_harmony_and_state(void){
    Inst *instance=fixture();API.set_param(instance,"chord_mode","Conductor Chord");
    API.set_param(instance,"chord_voicing","Root + Fifth Low");API.set_param(instance,"quant_timing","1/4");
    position=.9;midi(instance,1,60);assert(advance(instance,0,64)==0);
    uint8_t next[3]={62,66,69};hb_commit_observed_harmony(hb_infer_harmony(next,3));
    position=1;assert(advance(instance,0,64)==3);
    for(int index=0;index<3;index++)assert((output[index][1]%12)==2||(output[index][1]%12)==6||(output[index][1]%12)==9);
    char state[512],restored[512];API.get_param(instance,"state",state,sizeof(state));assert(strstr(state,";cp1,"));
    Inst *copy=API.create_instance("",NULL);API.set_param(copy,"state",state);
    API.get_param(copy,"state",restored,sizeof(restored));assert(!strcmp(state,restored));
    assert(copy->player.config.mode==2&&copy->player.config.voicing==1);
    API.set_param(copy,"state","hb16,1,0,0,25,2,0,0,0,0,0,0,3,0,0,0,0,0,0,0,-3,60,0,0,1,0");
    assert(!hb_cp_enabled(&copy->player));
    API.set_param(instance,"role","Off");
    int count=advance(instance,0,1);while(instance->player.sounding_count)count+=advance(instance,0,1);assert(count==3);
    API.destroy_instance(copy);API.destroy_instance(instance);
}
static void arp_start_modes_and_offsets(void){
    const int notes[3]={60,64,67};
    for(int mode=0;mode<3;mode++)for(int offset=-1;offset<=1;offset++){
        hb_chord_player player={0};hb_cp_defaults(&player.config);
        assert(player.config.phase==1);
        player.config.playback=1;player.config.phase=mode;player.config.note_phase=offset;
        player.beat=.10;hb_cp_on(&player,60,0,100,notes,3);
        int count=hb_cp_tick(&player,output,lengths,64);
        if(mode==1){assert(count==0);player.beat=.251;count=hb_cp_tick(&player,output,lengths,64);}
        int ordinal=(-offset+3)%3;
        assert(count==1&&output[0][0]==0x90&&output[0][1]==notes[ordinal]);
        double target=mode==0?.35:mode==1?.5:.25;
        assert(player.next_beat>target-1e-8&&player.next_beat<target+1e-8);
        // Gate expires; the second note lands on the selected clock.
        player.beat=target-.01;hb_cp_tick(&player,output,lengths,64);
        player.beat=target+.003;count=hb_cp_tick(&player,output,lengths,64);
        assert(count==1&&output[0][1]==notes[(ordinal+1)%3]);
        assert(player.next_beat>target+.25-1e-8&&player.next_beat<target+.25+1e-8);
        // Another physical key does not move the established clock.
        hb_cp_on(&player,72,0,100,notes,3);
        player.beat=target+.01;hb_cp_tick(&player,output,lengths,64);
        assert(player.next_beat>target+.25-1e-8&&player.next_beat<target+.25+1e-8);
        // A late callback skips expired steps without drifting the anchor.
        player.beat=target+.52;hb_cp_tick(&player,output,lengths,64);
        assert(player.next_beat>target+.75-1e-8&&player.next_beat<target+.75+1e-8);
        hb_cp_off(&player,60,0);hb_cp_off(&player,72,0);
        hb_cp_tick(&player,output,lengths,64);assert(!player.sounding_count);
    }
    Inst *instance=fixture();char state[512],value[64];
    Inst *fresh=API.create_instance("",NULL);assert(fresh->player.config.phase==1);API.destroy_instance(fresh);
    API.set_param(instance,"arp_phase","1st Note Free");
    API.set_param(instance,"arp_note_phase","-16");
    API.get_param(instance,"state",state,sizeof(state));
    assert(strstr(state,";ph1,2;np1,-16"));
    API.set_param(instance,"arp_phase","Free");API.set_param(instance,"arp_note_phase","0");
    API.set_param(instance,"state",state);
    assert(instance->player.config.phase==2&&instance->player.config.note_phase==-16);
    API.set_param(instance,"arp_rate","1/8");
    assert(instance->player.config.note_phase==-8);
    API.set_param(instance,"arp_note_phase","64");
    API.get_param(instance,"arp_note_phase",value,sizeof(value));assert(!strcmp(value,"8"));
    API.set_param(instance,"chord_timing","2 Bars");
    API.set_param(instance,"arp_note_phase","64");assert(instance->player.config.note_phase==16);
    API.set_param(instance,"chord_timing","1/4");assert(instance->player.config.note_phase==2);
    API.set_param(instance,"chord_timing","Free");
    API.set_param(instance,"arp_note_phase","64");assert(instance->player.config.note_phase==8);
    API.set_param(instance,"arp_rate","2 Bars");assert(instance->player.config.note_phase==0);
    API.destroy_instance(instance);
}
static void phase_grid(void){
    hb_chord_player player={0};hb_cp_defaults(&player.config);
    player.config.playback=1;player.config.phase=1;
    int notes[3]={64,67,72};
    player.beat=.10;hb_cp_on(&player,60,0,100,notes,3);
    assert(hb_cp_tick(&player,output,lengths,64)==0);
    player.beat=.249;assert(hb_cp_tick(&player,output,lengths,64)==0);
    player.beat=.251;assert(hb_cp_tick(&player,output,lengths,64)==1&&output[0][1]==72);
    assert(player.next_beat==.5);
    player.beat=.4;hb_cp_tick(&player,output,lengths,64);
    player.beat=.501;assert(hb_cp_tick(&player,output,lengths,64)==1&&output[0][1]==64);
    assert(player.next_beat==.75);
    hb_cp_off(&player,60,0);hb_cp_tick(&player,output,lengths,64);
    player.beat=.6;hb_cp_on(&player,60,0,100,notes,3);
    assert(hb_cp_tick(&player,output,lengths,64)==0);
    hb_cp_off(&player,60,0);player.beat=.75;
    assert(hb_cp_tick(&player,output,lengths,64)==0);
    player.config.phase=0;player.beat=.81;
    hb_cp_on(&player,60,0,100,notes,3);
    assert(hb_cp_tick(&player,output,lengths,64)==1&&output[0][1]==64);
    Inst *instance=fixture();char state[512],value[64];
    API.get_param(instance,"arp_phase",value,sizeof(value));assert(!strcmp(value,"Free"));
    API.set_param(instance,"arp_phase","Auto");
    API.set_param(instance,"arp_playback","Repeat Arp");
    API.get_param(instance,"state",state,sizeof(state));assert(strstr(state,";ph1,1"));
    API.set_param(instance,"arp_phase","Free");API.set_param(instance,"state",state);
    assert(instance->player.config.phase==1);
    API.destroy_instance(instance);
}
static void conductor_chords(void){
    Inst *instance=fixture();API.set_param(instance,"role","Conductor");
    instance->movy_track=2;
    API.set_param(instance,"chord_mode","Scale Root");
    API.set_param(instance,"follower_scale","Major");
    int before=render_count;
    midi(instance,1,62);assert(render_count==before); /* no raw D echo */
    int count=advance(instance,0,64);
    assert(count==3&&output[0][1]==62&&output[1][1]==65&&output[2][1]==69);
    assert(render_count==before+3);
    assert(recorded_count==3&&recorded[0][1]==0x92&&recorded[0][2]==62);
    for(int index=0;index<3;index++)assert(rendered[before+index][2]==output[index][1]);
    assert(instance->held_count[62]&&instance->held_count[65]&&instance->held_count[69]);
    assert(bus_read().root_pc==2); /* followers see generated D minor, not raw D */
    midi(instance,0,62);assert(advance(instance,0,64)==3);
    assert(recorded_count==6&&recorded[3][1]==0x82);
    for(int index=0;index<3;index++)assert(output[index][0]==0x80);
    assert(!instance->held_count[62]&&!instance->held_count[65]&&!instance->held_count[69]);
    /* Source note chooses the bass and inversion of the recognized harmony. */
    API.set_param(instance,"chord_mode","Conductor Chord");
    midi(instance,1,65);count=advance(instance,0,64);
    assert(count==3&&output[0][1]==65&&output[1][1]==69&&output[2][1]==74);
    midi(instance,0,65);assert(advance(instance,0,64)==3);
    /* A saved chord voice is accepted directly, without doubling on replay. */
    API.set_param(instance,"hb_movy_playback","1");
    API.set_param(instance,"hb_movy_passthrough","1");
    before=render_count;int prior_recorded=recorded_count;
    midi(instance,1,72);assert(instance->held_count[72]);
    assert(advance(instance,0,64)==0);
    assert(render_count==before+1&&rendered[before][2]==72);
    midi(instance,0,72);assert(advance(instance,0,64)==0);
    assert(recorded_count==prior_recorded);
    API.set_param(instance,"hb_movy_passthrough","0");
    API.set_param(instance,"hb_movy_playback","0");
    /* The arp publishes its full harmonic gesture while sounding one note. */
    API.set_param(instance,"chord_mode","Scale Root");
    API.set_param(instance,"arp_playback","Repeat Arp");
    midi(instance,1,60);count=advance(instance,0,64);
    assert(count==1&&output[0][1]==60);
    assert(instance->held_count[60]&&instance->held_count[64]&&instance->held_count[67]);
    assert(bus_read().root_pc==0);
    midi(instance,0,60);assert(advance(instance,0,64)==1&&output[0][0]==0x80);
    API.destroy_instance(instance);
    /* Empty bus still accepts the first Conductor Chord gesture. */
    instance=fixture();API.set_param(instance,"role","Conductor");
    API.set_param(instance,"follower_scale","Major");
    API.set_param(instance,"chord_mode","Conductor Chord");
    memset(&g_bus.observed_harmony,0,sizeof(g_bus.observed_harmony));
    hb_effective_write(g_bus.observed_harmony);
    midi(instance,1,60);assert(advance(instance,0,64)==3);
    assert(output[0][1]==60&&output[1][1]==64&&output[2][1]==67);
    assert(bus_read().valid&&bus_read().root_pc==0);
    midi(instance,0,60);assert(advance(instance,0,64)==3);
    API.destroy_instance(instance);
}
static void chord_qualities(void){
    hb_cp_config config;hb_cp_defaults(&config);config.mode=1;config.size=3;
    unsigned major=0xAB5;int pitches[HB_CP_VOICES];
    config.chromatic_quality=1;
    const int major_seventh[]={61,65,68,72};
    assert(hb_cp_voice(config,61,0,0,major,pitches)==4);
    expect_notes(pitches,major_seventh,4);
    config.chromatic_quality=2;
    const int dominant_seventh[]={61,65,68,71};
    assert(hb_cp_voice(config,61,0,0,major,pitches)==4);
    expect_notes(pitches,dominant_seventh,4);
    config.chromatic_quality=3;
    const int diminished_seventh[]={61,64,67,70};
    assert(hb_cp_voice(config,61,0,0,major,pitches)==4);
    expect_notes(pitches,diminished_seventh,4);
    config.quality=5;
    assert(hb_cp_voice(config,60,0,0,major,pitches)==4);
    const int overriding_seventh[]={60,64,67,71};
    expect_notes(pitches,overriding_seventh,4);
    config.mode=2;config.size=0;config.quality=9;
    unsigned recognized=(1u<<0)|(1u<<4)|(1u<<7)|(1u<<11);
    assert(hb_cp_voice(config,60,0,recognized,major,pitches)==4);
    const int override_detected[]={60,63,66,69};
    expect_notes(pitches,override_detected,4);
    Inst *instance=fixture();API.set_param(instance,"role","Conductor");
    API.set_param(instance,"chord_mode","Scale Root");
    API.set_param(instance,"chord_form","Seventh");
    API.set_param(instance,"chromatic_quality","Dim / Dim7");
    midi(instance,1,61);assert(advance(instance,0,64)==4);
    expect_notes((int[]){output[0][1],output[1][1],output[2][1],output[3][1]},diminished_seventh,4);
    midi(instance,0,61);assert(advance(instance,0,64)==4);
    char state[512];API.get_param(instance,"state",state,sizeof(state));
    assert(strstr(state,";cq1,0,3"));
    Inst *copy=API.create_instance("",NULL);API.set_param(copy,"state",state);
    assert(copy->player.config.chromatic_quality==3);
    API.destroy_instance(copy);API.destroy_instance(instance);
    instance=fixture();API.set_param(instance,"chord_mode","Scale Root");
    API.set_param(instance,"chord_form","Seventh");
    API.set_param(instance,"approach_chrom_next","1");
    assert(instance->approach_pad_armed==HB_APPROACH_CHROM_BELOW);
    midi(instance,1,62);assert(advance(instance,0,64)==4);
    const int below_dim7[]={61,64,67,70};
    expect_notes((int[]){output[0][1],output[1][1],output[2][1],output[3][1]},below_dim7,4);
    assert(instance->approach_pad_armed==HB_APPROACH_OFF);
    midi(instance,0,62);assert(advance(instance,0,64)==4);
    midi(instance,1,62);assert(advance(instance,0,64)==4);
    const int regular_dmin7[]={62,65,69,72};
    expect_notes((int[]){output[0][1],output[1][1],output[2][1],output[3][1]},regular_dmin7,4);
    API.destroy_instance(instance);
}
static void generated_degree_progression(void){
    Inst *instance=fixture();API.set_param(instance,"role","Conductor");
    API.set_param(instance,"chord_mode","Scale Degree");
    const int roots[]={65,62,67,64};
    const int thirds[]={4,3,4,3};
    for(int inversion=0;inversion<3;inversion++)for(int pass=0;pass<2;pass++)for(int index=0;index<4;index++){
        API.set_param(instance,"chord_inversion",inversion==0?"Root":inversion==1?"First":"Second");
        midi(instance,1,roots[index]);assert(advance(instance,0,64)==3);
        int root=roots[index]%12;
        unsigned expected=(1u<<root)|(1u<<((root+thirds[index])%12))|(1u<<((root+7)%12));
        unsigned emitted=0;for(int voice=0;voice<3;voice++)emitted|=1u<<(output[voice][1]%12);
        assert(emitted==expected);
        hb_harmony_t detected=bus_read();
        assert(detected.root_pc==root);
        assert(hb_harmony_chord_mask(detected)==expected);
        char label[64],candidate[64],expected_label[64];
        API.get_param(instance,"harmony",label,sizeof(label));
        API.get_param(instance,"candidate_harmony",candidate,sizeof(candidate));
        snprintf(expected_label,sizeof(expected_label),"%s%s",hb_pc_name(root),thirds[index]==3?"m":"");
        if(inversion==0){assert(!strcmp(label,expected_label));assert(!strcmp(candidate,expected_label));}
        else {assert(!strncmp(label,expected_label,strlen(expected_label)));assert(strchr(label,'/'));}

        midi(instance,0,roots[index]);assert(advance(instance,pass?100:0,64)==3);
    }
    API.set_param(instance,"chord_mode","Off");
    API.set_param(instance,"chord_mode","Scale Root");
    char name[64];API.get_param(instance,"chord_mode",name,sizeof(name));
    assert(!strcmp(name,"Scale Degree"));
    API.destroy_instance(instance);
}
static void split2_and_master(void){
    Inst *instance=fixture();
    const char *scales[]={"Major","Natural Minor","Harmonic Minor"};
    for(int root=0;root<12;root+=5)for(int scale=0;scale<3;scale++){
        globals.follower_explicit_root=root;
        API.set_param(instance,"follower_scale",scales[scale]);
        uint8_t notes[3]={60,64,67};hb_harmony_t harmony=hb_infer_harmony(notes,3);
        hb_effective_write(harmony);
        uint16_t parent=hb_follower_input_scale(instance,root);
        for(int split=0;split<4;split++)for(int pitch=48;pitch<84;pitch++){
            instance->follower_split_map=split;instance->content_map=1;
            API.set_param(instance,"travel_map","Closest Split");
            int normal=hb_map_follower_note_now(instance,pitch);
            if(parent&(1u<<mod12(pitch))){
                API.set_param(instance,"travel_map","Closest Split Chromatic");
                assert(hb_map_follower_note_now(instance,pitch)==normal);
            }else{
                int next=pitch+1;while(!(parent&(1u<<mod12(next))))next++;
                int resolution=hb_map_follower_note_now(instance,next);
                API.set_param(instance,"travel_map","Closest Split Chromatic");
                assert(hb_map_follower_note_now(instance,pitch)==resolution-1);
                assert(hb_map_follower_note_now(instance,next)==resolution);
            }
        }
    }
    char state[8192],label[64];
    API.get_param(instance,"state",state,sizeof(state));
    API.set_param(instance,"travel_map","Relative");API.set_param(instance,"state",state);
    API.get_param(instance,"travel_map",label,sizeof(label));assert(!strcmp(label,"Closest Split Chromatic"));
    globals.follower_explicit_root=0;instance->follower_split_map=1;
    API.set_param(instance,"follower_scale","Major");
    for(int pitch=61;pitch<=62;pitch++){
        midi(instance,1,pitch);assert(advance(instance,0,64)==1&&output[0][1]==pitch);
        assert(rendered[render_count-1][2]==pitch);
        midi(instance,0,pitch);assert(advance(instance,0,64)==1&&output[0][1]==pitch);
    }
    for(int reference=0;reference<12;reference++)for(int destination=0;destination<12;destination++){
        globals.follower_explicit_root=reference;
        API.set_param(instance,"master_transpose",MASTER_ROOT_OPTS[destination+1]);
        assert(mod12(reference+g_bus.global_transpose)==destination);
        API.get_param(instance,"master_transpose",label,sizeof(label));
        assert(!strcmp(label,reference==destination?"As Played":MASTER_ROOT_OPTS[destination+1]));
    }
    API.set_param(instance,"master_transpose","As Played");assert(g_bus.global_transpose==0);
    globals.follower_explicit_root=0;instance->travel_map=0;instance->content_map=0;
    uint8_t c[3]={60,64,67};hb_effective_write(hb_infer_harmony(c,3));
    midi(instance,1,60);assert(advance(instance,0,64)==1&&output[0][1]==60);
    g_bus.next_model_locked=1;g_bus.next_model_count=1;
    g_bus.next_model[0].harmony=hb_infer_harmony(c,3);
    API.set_param(instance,"master_transpose","D");
    assert(g_bus.next_model_locked&&g_bus.next_model[0].harmony.root_pc==2);
    API.set_param(instance,"master_transpose","E"); // quick knob turns retain pending OFF
    assert(advance(instance,0,64)==1&&output[0][0]==0x80&&output[0][1]==60);
    midi(instance,1,60);assert(advance(instance,0,64)==1&&output[0][1]==64);
    midi(instance,0,60);assert(advance(instance,0,64)==1&&output[0][1]==64);
    API.destroy_instance(instance);
}
static void split_seventh(void){
    Inst *instance=fixture();
    instance->travel_map=3;instance->follower_split_map=1;
    uint8_t triad[3]={60,64,67};hb_effective_write(hb_infer_harmony(triad,3));
    instance->content_map=0;
    int seventh=hb_map_follower_note_now(instance,71);
    fprintf(stderr,"135/2467, Chord, C triad: B -> %d\n",seventh);
    assert(seventh==71);
    const int notes[7]={60,62,64,65,67,69,71};
    for(int content=0;content<=8;content++)for(int split=1;split<=2;split++){
        instance->content_map=content;instance->follower_split_map=split;
        unsigned on=(1u<<0)|(1u<<4)|(1u<<7)|(split==2?(1u<<11):0);
        unsigned off=0xAB5u&~on;
        for(int degree=0;degree<7;degree++){
            int is_on=degree==0||degree==2||degree==4||(split==2&&degree==6);
            int mapped=hb_map_follower_note_now(instance,notes[degree]);
            assert((is_on?on:off)&(1u<<(mapped%12)));
        }
    }
    API.destroy_instance(instance);
}
static void predicted_capture(void){
    for(int late=0;late<2;late++)for(int quant=0;quant<2;quant++){
        Inst *instance=fixture();
        API.set_param(instance,"boundary_buffer_ms","1/16");
        API.set_param(instance,"quant_timing",quant?"1/8":"Off");
        instance->next_lookahead=late?10:3; // -quarter or +eighth
        hb_next_update_playhead(0,48000);
        g_bus.next_model_locked=1;g_bus.next_model_count=2;
        uint8_t c[3]={60,64,67},d[3]={62,66,69};
        g_bus.next_model[0].phase=0;g_bus.next_model[0].harmony=hb_infer_harmony(c,3);
        g_bus.next_model[1].phase=2;g_bus.next_model[1].harmony=hb_infer_harmony(d,3);
        double boundary=late?3.0:1.5;
        position=boundary-0.3;hb_next_apply_effective(hb_clip_playhead());
        midi(instance,1,60);assert(advance(instance,0,64)==1&&output[0][1]==60);
        midi(instance,0,60);assert(advance(instance,0,64)==1);
        position=boundary-0.2;hb_next_apply_effective(hb_clip_playhead());
        assert(bus_read().root_pc==0);
        midi(instance,1,60);
        int count=advance(instance,0,64);
        if(quant){assert(count==0);position=boundary;count=advance(instance,0,64);}
        assert(count==1&&output[0][1]==62);
        assert(rendered[render_count-1][2]==62);
        assert(bus_read().root_pc==0&&!instance->render_harmony_active);
        position+=0.05;midi(instance,0,60);count=advance(instance,0,64);
        if(quant){assert(count==0);position+=0.2;count=advance(instance,0,64);}
        assert(count==1&&output[0][0]==0x80&&output[0][1]==62);
        API.destroy_instance(instance);
    }
    // Future harmony also feeds chord generation, without changing the bus.
    Inst *instance=fixture();
    API.set_param(instance,"chord_mode","Conductor Chord");
    instance->next_lookahead=3;hb_next_update_playhead(0,48000);g_bus.next_model_locked=1;g_bus.next_model_count=2;
    uint8_t c[3]={60,64,67},d[3]={62,66,69};
    g_bus.next_model[0].phase=0;g_bus.next_model[0].harmony=hb_infer_harmony(c,3);
    g_bus.next_model[1].phase=2;g_bus.next_model[1].harmony=hb_infer_harmony(d,3);
    position=1.3;hb_next_apply_effective(hb_clip_playhead());
    midi(instance,1,62);assert(advance(instance,0,64)==3);
    assert(output[0][1]==62&&output[1][1]==66&&output[2][1]==69);
    assert(bus_read().root_pc==0);
    midi(instance,0,62);assert(advance(instance,0,64)==3);
    // Invalidation while queued falls back to the physical boundary.
    API.set_param(instance,"chord_mode","Off");
    midi(instance,1,60);g_bus.next_model_locked=0;
    assert(advance(instance,0,64)==0);
    position=1.5;hb_effective_write(hb_infer_harmony(d,3));
    assert(advance(instance,0,64)==1&&output[0][1]==62);
    const char *labels[]={"Chord","Scale","Free","135","1357","12357","12356","Non-Avoid","123567"};
    for(int index=0;index<9;index++){
        char old[32],value[32];snprintf(old,sizeof(old),"In %s",labels[index]);
        API.set_param(instance,"content_map",old);API.get_param(instance,"content_map",value,sizeof(value));
        assert(instance->content_map==index&&!strcmp(value,labels[index]));
        API.set_param(instance,"content_map",labels[index]);assert(instance->content_map==index);
    }
    API.destroy_instance(instance);
}
static void receiver_routing(void){
    Inst *source=fixture(),*r=API.create_instance("",NULL),*other=API.create_instance("",NULL);
    API.set_param(r,"role","Receiver");API.set_param(r,"receive_channel","4");
    API.set_param(other,"role","Receiver");API.set_param(other,"receive_channel","3");
    assert(r->role==3&&r->source_channel==3);
    char state[8192],name[32];API.get_param(r,"state",state,sizeof(state));
    API.set_param(r,"role","Off");assert(r->role==2);API.set_param(r,"state",state);
    API.get_param(r,"role",name,sizeof(name));assert(!strcmp(name,"Receiver"));
    midi(source,1,60);assert(advance(source,0,64)==1);assert(render_count==1);
    assert(advance(r,0,1)==1&&output[0][1]==60);
    assert(advance(other,0,64)==0&&render_count==1); // neither rebroadcast nor cross-channel delivery
    midi(r,1,72);assert(advance(r,0,64)==0); // raw receiver pads do not re-harmonize
    midi(source,0,60);advance(source,0,64);
    assert(advance(r,0,64)==1&&output[0][0]==0x80&&output[0][1]==60);
    assert(render_count==2);
    // Same pitch from two sources: one source's release cannot end the other.
    API.set_param(other,"role","Follower");API.set_param(other,"source_channel","1");API.set_param(other,"render_channel","4");
    midi(source,1,60);advance(source,0,64);midi(other,1,60);advance(other,0,64);
    assert(advance(r,0,1)==1);assert(advance(r,0,1)==1);
    midi(source,0,60);advance(source,0,64);assert(advance(r,0,64)==0);
    API.destroy_instance(other);assert(advance(r,0,64)==1&&output[0][0]==0x80);
    midi(source,1,60);advance(source,0,64);assert(advance(r,0,64)==1);
    API.set_param(source,"render_channel","2");assert(advance(r,0,64)==1&&output[0][0]==0x80);
    API.set_param(source,"render_channel","4");midi(source,1,64);advance(source,0,64);advance(r,0,64);
    API.set_param(r,"receive_channel","2");assert(advance(r,0,64)==1&&output[0][0]==0x80);
    API.set_param(r,"receive_channel","4");
    midi(source,1,67);advance(source,0,64);advance(r,0,64);
    position=-1;assert(advance(r,0,64)==1&&output[0][0]==0x80);position=0;
    // Overflow fails closed; pending local OFF survives small output capacity.
    uint8_t packet[4]={0x29,0x93,60,100};hb_send_render(source,packet,0);advance(r,0,64);
    for(int n=0;n<257;n++)hb_send_render(source,packet,0);
    assert(advance(r,0,1)==1&&output[0][0]==0x80);
    assert(advance(r,0,64)==0);
    // Restore a different route, then change role before ticking: preserve the OFF.
    hb_send_render(source,packet,0);assert(advance(r,0,64)==1);
    API.set_param(r,"receive_channel","2");
    API.set_param(r,"state",state);
    API.set_param(r,"role","Off");
    assert(advance(r,0,64)==1&&output[0][0]==0x80);
    API.set_param(r,"role","Receiver");
    hb_send_render(source,packet,0);assert(advance(r,0,64)==1);
    API.set_param(r,"receive_channel","2");char changed[8192];API.get_param(r,"state",changed,sizeof(changed));advance(r,0,64);
    API.set_param(r,"state",state);hb_send_render(source,packet,0);assert(advance(r,0,64)==1);
    API.set_param(r,"state",changed);assert(advance(r,0,64)==1&&output[0][0]==0x80);
    // A chord-playing source becoming a receiver must release its existing voices.
    API.destroy_instance(r);API.destroy_instance(source);source=fixture();
    API.set_param(source,"chord_mode","Scale Degree");API.set_param(source,"chord_form","Triad");midi(source,1,60);assert(advance(source,0,64)==3);
    API.set_param(source,"role","Receiver");assert(advance(source,0,64)==3);
    for(int i=0;i<3;i++)assert(output[i][0]==0x80);
    API.destroy_instance(source);
}
static void four_bar_timing(void){
    Inst *instance=fixture();
    const char *keys[]={"quant_timing","chord_timing","boundary_buffer_ms","arp_rate","strum_spread","next_lookahead"};
    char value[32],state[8192],restored[8192];
    for(int index=0;index<6;index++){
        API.set_param(instance,keys[index],"4 Bars");
        API.get_param(instance,keys[index],value,sizeof(value));assert(!strcmp(value,"4 Bars"));
    }
    assert(hb_quant_grid_beats_for(instance)==16.0&&hb_chord_grid_beats()==16.0);
    assert(hb_next_lookahead_beats_for(instance)==16.0);
    assert(hb_cp_division(instance->player.config.rate)==16.0);
    assert(hb_cp_division(-instance->player.config.spread-1)==16.0);
    assert(instance->boundary_buffer_ms==-9);
    API.set_param(instance,"next_lookahead","-4 Bars");assert(hb_next_lookahead_beats_for(instance)==-16.0);
    API.get_param(instance,"next_lookahead",value,sizeof(value));assert(!strcmp(value,"-4 Bars"));
    // Legacy lookahead IDs and two-bar durations remain unchanged.
    API.set_param(instance,"next_lookahead","19");assert(hb_next_lookahead_beats_for(instance)==8.0);
    API.set_param(instance,"next_lookahead","21");assert(hb_next_lookahead_beats_for(instance)==-8.0);
    API.set_param(instance,"next_lookahead","Off");
    API.get_param(instance,"state",state,sizeof(state));
    Inst *copy=API.create_instance("",NULL);API.set_param(copy,"state",state);
    API.get_param(copy,"state",restored,sizeof(restored));assert(!strcmp(state,restored));
    assert(g_bus.quant_timing==7&&copy->player.config.rate==8&&copy->player.config.spread==-9);
    // A press one beat into the cycle captures to beat 16, not the old beat 8.
    position=1.0;midi(instance,1,60);
    assert(instance->follower_queue_count==1&&instance->follower_queue_target_beat[0]==16.0);
    API.destroy_instance(copy);API.destroy_instance(instance);
}
static void recorded_master_transpose(void){
    Inst *conductor=fixture();API.set_param(conductor,"role","Conductor");
    API.set_param(conductor,"chord_mode","Scale Degree");API.set_param(conductor,"chord_form","Triad");
    conductor->movy_track=0;API.set_param(conductor,"master_transpose","D");
    midi(conductor,1,60);assert(advance(conductor,0,64)==3);
    const int expected[3]={62,66,69};
    for(int index=0;index<3;index++){
        assert(output[index][1]==expected[index]);
        assert(recorded[index][2]==expected[index]-2);
    }
    advance(conductor,100,64);assert(bus_read().root_pc==2);
    // Changing transpose before release still closes the original recorded pitches.
    API.set_param(conductor,"master_transpose","F");advance(conductor,0,64);
    assert(recorded_count==6);
    for(int index=0;index<3;index++)assert(recorded[index+3][2]==recorded[index][2]);
    API.set_param(conductor,"hb_movy_playback","1");API.set_param(conductor,"hb_movy_passthrough","1");
    int prior_recorded=recorded_count;
    for(int mode=0;mode<2;mode++){
        API.set_param(conductor,"chord_mode",mode?"Scale Degree":"Off");
        advance(conductor,0,64);
        int before=render_count;
        for(int index=0;index<3;index++){
            uint8_t message[3]={0x90,(uint8_t)(expected[index]-2),100};
            assert(API.process_midi(conductor,message,3,output,lengths,64)==1);
            assert(output[0][1]==expected[index]+3);
            assert(rendered[before+index][2]==output[0][1]);
        }
        advance(conductor,100,64);assert(bus_read().root_pc==5);
        // Recorded/raw follower 1-3-5 and live pads share the same reference key.
        Inst *follower=API.create_instance("",NULL);
        API.set_param(follower,"role","Follower");API.set_param(follower,"render_channel","2");
        API.set_param(follower,"follower_root_policy","Explicit");API.set_param(follower,"follower_explicit_root","C");
        for(int playback=0;playback<2;playback++)for(int index=0;index<3;index++){
            API.set_param(follower,"hb_movy_playback",playback?"1":"0");
            midi(follower,1,expected[index]-2);assert(advance(follower,0,64)==1);
            assert(output[0][1]==expected[index]+3);
            assert(rendered[render_count-1][2]==output[0][1]);
            midi(follower,0,expected[index]-2);advance(follower,0,64);
        }
        API.destroy_instance(follower);
        // Master changes flush old local and broadcast pitches for replayed voices.
        API.set_param(conductor,"master_transpose","D");
        assert(advance(conductor,0,64)==3);
        for(int index=0;index<3;index++)assert(output[index][0]==0x80&&output[index][1]==expected[index]+3);
        API.set_param(conductor,"master_transpose","F");
    }
    assert(recorded_count==prior_recorded);
    API.destroy_instance(conductor);
}
static void held_conductor_chords(void){
    const uint8_t minor[3]={62,65,69};
    for(int enabled=0;enabled<2;enabled++)for(int latch=0;latch<4;latch++){
        Inst *instance=fixture();
        API.set_param(instance,"chord_mode","Conductor Chord");API.set_param(instance,"chord_form","Triad");
        instance->player.config.latch=latch;instance->retrigger_held=enabled;
        midi(instance,1,60);assert(advance(instance,0,64)==3);
        assert(output[0][1]==60&&output[1][1]==64&&output[2][1]==67);
        if(latch){midi(instance,0,60);assert(advance(instance,0,64)==0);}
        int before=render_count;
        hb_commit_observed_harmony(hb_infer_harmony(minor,3));
        if(enabled){
            // Capacity one forces the complete OFF-then-ON transition over ticks.
            for(int index=0;index<6;index++){
                assert(advance(instance,0,1)==1);
                assert(output[0][0]==(index<3?0x80:0x90));
                if(index>=3)assert(output[0][1]==minor[index-3]);
                assert(rendered[before+index][2]==output[0][1]);
            }
            assert(render_count==before+6);
        }else assert(advance(instance,0,64)==0&&render_count==before);
        assert(advance(instance,0,64)==0);
        hb_commit_observed_harmony(hb_infer_harmony(minor,3));
        assert(advance(instance,0,64)==0); // identical harmony cannot repeat the chord
        if(latch)API.set_param(instance,"arp_clear","Clear");else midi(instance,0,60);
        assert(advance(instance,0,64)==3);
        for(int index=0;index<3;index++)assert(output[index][0]==0x80);
        assert(advance(instance,0,64)==0&&instance->player.sounding_count==0);
        API.destroy_instance(instance);
    }
    // Enabling after a change catches an already-held old chord up immediately.
    Inst *late=fixture();API.set_param(late,"chord_mode","Conductor Chord");
    API.set_param(late,"chord_form","Triad");midi(late,1,60);advance(late,0,64);
    hb_commit_observed_harmony(hb_infer_harmony(minor,3));assert(advance(late,0,64)==0);
    API.set_param(late,"retrigger_held","On");assert(advance(late,0,64)==6);
    assert(output[3][1]==62&&output[4][1]==65&&output[5][1]==69);
    midi(late,0,60);assert(advance(late,0,64)==3);API.destroy_instance(late);
    // Repeat arp and trigger strum must replace their pitch pool too.
    for(int playback=1;playback<=2;playback++){
        Inst *instance=fixture();instance->retrigger_held=1;
        API.set_param(instance,"chord_mode","Conductor Chord");API.set_param(instance,"chord_form","Triad");
        instance->player.config.playback=playback;instance->player.config.spread=0;
        midi(instance,1,60);assert(advance(instance,0,64)>0);
        hb_commit_observed_harmony(hb_infer_harmony(minor,3));
        int count=advance(instance,0,64);assert(count>0);
        for(int index=0;index<HB_CP_KEYS;index++)if(instance->player.keys[index].used){
            hb_cp_key *key=&instance->player.keys[index];
            assert(key->count==3);
            for(int voice=0;voice<3;voice++)assert(key->notes[voice]==minor[voice]);
        }
        midi(instance,0,60);advance(instance,0,64);
        assert(instance->player.sounding_count==0);
        API.destroy_instance(instance);
    }
}
static void arp_buffer_bypass(void){
    for(int chord_mode=0;chord_mode<2;chord_mode++)for(int phase=0;phase<2;phase++){
        Inst *instance=fixture();
        API.set_param(instance,"chord_mode",chord_mode?"Conductor Chord":"Off");
        API.set_param(instance,"arp_playback","Repeat Arp");
        API.set_param(instance,"boundary_buffer_ms","350");
        API.set_param(instance,"quant_timing","1 Bar");
        instance->player.config.phase=phase;
        position=3.9;midi(instance,1,60);
        assert(instance->follower_queue_count==1);
        assert(instance->follower_queue_target_beat[0]<0.0);
        int count=advance(instance,0,64);
        assert(instance->follower_queue_count==0);
        if(!phase)assert(count>0&&output[0][0]==0x90);
        else{
            assert(count==0);
            position=4.0;count=advance(instance,0,64);
            assert(count>0&&output[0][0]==0x90);
        }
        position+=0.05;midi(instance,0,60);
        assert(instance->follower_queue_target_beat[0]<0.0);
        count=advance(instance,0,64);
        assert(count>0&&output[0][0]==0x80);
        assert(instance->follower_queue_count==0);
        position=8.0;assert(advance(instance,0,64)==0);
        API.destroy_instance(instance);
    }
}
static void arp_harmony_boundary(void){
    const int old_chord[3]={60,64,67},new_chord[3]={62,65,69};
    for(int chord_mode=0;chord_mode<2;chord_mode++)for(int bridge=0;bridge<2;bridge++)for(int phase=0;phase<3;phase++)for(int pending=0;pending<2;pending++){
        Inst *follower=fixture(),*conductor=API.create_instance("",NULL);
        API.set_param(conductor,"role","Conductor");API.set_param(conductor,"source_channel","1");
        API.set_param(conductor,"analysis_release_ms","0");
        API.set_param(follower,"chord_mode",chord_mode?"Conductor Chord":"Off");API.set_param(follower,"chord_form","Triad");
        API.set_param(follower,"arp_playback","Repeat Arp");API.set_param(follower,"retrigger_held","On");
        follower->player.config.phase=phase;
        API.set_param(follower,"travel_map","Relative");
        for(int voice=0;voice<3;voice++)midi(conductor,1,old_chord[voice]);
        midi(follower,1,60);
        for(int step=0;step<16;step++){position=step*0.25;advance(follower,0,64);}
        if(pending){
            // Simulate an event captured before switching to Repeat Arp.
            // Legacy/in-flight queue entries must not block held-note revoicing.
            API.set_param(follower,"quant_timing","4 Bars");API.set_param(follower,"boundary_buffer_ms","4 Bars");
            position=3.9;follower->player.config.playback=0;
            midi(follower,1,72);
            follower->player.config.playback=1;
            advance(follower,0,64);
            assert(follower->follower_queue_count==1);
        }
        position=4.0;
        // Actual conductor MIDI arrives at the same beat as the arp step.
        for(int voice=0;voice<3;voice++)midi(conductor,0,old_chord[voice]);
        for(int voice=0;voice<3;voice++)midi(conductor,1,new_chord[voice]);
        if(bridge)API.set_param(conductor,"hb_movy_block","100,64,48000");
        int before=render_count,ons=0;
        for(int step=0;step<2;step++){
            position=4.0+step*0.25;
            int count=advance(follower,0,64);
            assert(bus_read().root_pc==2);
            for(int index=0;index<count;index++)if(output[index][0]==0x90){
                if(output[index][1]!=62&&output[index][1]!=65&&output[index][1]!=69)fprintf(stderr,"stale arp: phase=%d queued=%d beat=%.2f pitch=%d harmony=%d\n",phase,pending,position,output[index][1],bus_read().root_pc);
                assert(output[index][1]==62||output[index][1]==65||output[index][1]==69);
                ons++;
            }
            /* Each due grid hit must survive the harmony change, including Auto. */
            if(ons!=step+1)fprintf(stderr,"missing mode=%d bridge=%d phase=%d pending=%d step=%d ons=%d count=%d\n",chord_mode,bridge,phase,pending,step,ons,count);
            assert(ons==step+1);
            position+=0.001;
            assert(advance(follower,0,64)==0);
        }
        assert(ons>0);
        for(int index=before;index<render_count;index++)if((rendered[index][1]&0xf0)==0x90)
            assert(rendered[index][2]==62||rendered[index][2]==65||rendered[index][2]==69);
        if(pending)assert(follower->follower_queue_count==1&&follower->follower_queue_target_beat[0]==16.0);
        API.destroy_instance(conductor);API.destroy_instance(follower);
    }
}
static void raw_arp_relative_mapping(void){
    Inst *instance=fixture();
    const uint8_t f_chord[3]={65,69,72},g_chord[3]={67,71,74};
    hb_commit_observed_harmony(hb_infer_harmony(f_chord,3));
    API.set_param(instance,"arp_playback","Repeat Arp");API.set_param(instance,"travel_map","Relative");
    midi(instance,1,60);assert(advance(instance,0,64)==1&&output[0][1]==65);
    hb_commit_observed_harmony(hb_infer_harmony(g_chord,3));
    assert(advance(instance,0,64)==0); // retrigger Off freezes the original interpretation
    API.set_param(instance,"retrigger_held","On");
    assert(advance(instance,0,64)==1&&output[0][0]==0x80&&output[0][1]==65);
    position=0.25;
    assert(advance(instance,0,64)==1&&output[0][0]==0x90&&output[0][1]==55);
    assert(rendered[render_count-1][2]==55);
    midi(instance,0,60);assert(advance(instance,0,64)==1&&output[0][0]==0x80&&output[0][1]==55);
    API.set_param(instance,"master_transpose","D");advance(instance,0,64);
    midi(instance,1,60);assert(advance(instance,0,64)==1&&output[0][1]==57);
    midi(instance,0,60);assert(advance(instance,0,64)==1&&output[0][1]==57);
    API.destroy_instance(instance);
}
static void arp_pressure(void){
    for(int chord=0;chord<2;chord++)for(int queued=0;queued<2;queued++){
        Inst *instance=fixture();
        API.set_param(instance,"arp_playback","Repeat Arp");
        API.set_param(instance,"retrigger_held","On");
        if(chord)API.set_param(instance,"chord_mode","Conductor Chord");
        midi(instance,1,60);
        if(!queued)assert(advance(instance,0,64)==1&&output[0][2]==100);
        uint8_t pressure[3]={0xA0,60,37};
        assert(API.process_midi(instance,pressure,3,output,lengths,64)==0);
        int count=advance(instance,0,64);
        if(queued)assert(count==1&&output[0][2]==37);else assert(count==0);
        double next=instance->player.next_beat;
        position=next;count=advance(instance,0,64);
        assert(count==2&&output[1][0]==0x90&&output[1][2]==37);
        assert(instance->player.next_beat==next+0.25);
        uint8_t notes[3]={62,65,69};hb_commit_observed_harmony(hb_infer_harmony(notes,3));
        position=instance->player.next_beat;count=advance(instance,0,64);
        assert(count==2&&output[1][0]==0x90&&output[1][2]==37);
        pressure[2]=0;API.process_midi(instance,pressure,3,output,lengths,64);
        position=instance->player.next_beat;count=advance(instance,0,64);
        assert(count==2&&output[1][0]==0x90&&output[1][2]==1);
        midi(instance,0,60);advance(instance,0,64);
        pressure[2]=127;API.process_midi(instance,pressure,3,output,lengths,64);
        position+=0.25;assert(advance(instance,0,64)==0);
        API.destroy_instance(instance);
    }
}
static void pressure_recording_and_replay(void){
    Inst *instance=fixture();
    API.set_param(instance,"role","Conductor");instance->movy_track=0;
    API.set_param(instance,"chord_mode","Scale Degree");
    API.set_param(instance,"arp_playback","Repeat Arp");
    midi(instance,1,60);advance(instance,0,64);
    assert(recorded_count==1&&recorded[0][3]==100);
    uint8_t pressure[3]={0xA0,60,43};
    API.process_midi(instance,pressure,3,output,lengths,64);
    position=0.25;advance(instance,0,64);
    assert(recorded[recorded_count-1][1]==0x90&&recorded[recorded_count-1][3]==43);
    API.destroy_instance(instance);
    instance=fixture();API.set_param(instance,"arp_playback","Repeat Arp");
    for(int loop=0;loop<2;loop++){
        midi(instance,1,60);
        /* Sequencer emits ON before same-tick stored pressure, before HB ticks. */
        API.process_midi(instance,pressure,3,output,lengths,64);
        int count=advance(instance,0,64);
        assert(count==1&&output[0][0]==0x90&&output[0][2]==43);
        position+=0.25;count=advance(instance,0,64);
        assert(count==2&&output[1][0]==0x90&&output[1][2]==43);
        midi(instance,0,60);advance(instance,0,64);position+=0.25;
    }
    API.destroy_instance(instance);
}
static void retrigger_default(void){
    Inst *instance=fixture();
    API.destroy_instance(instance);
    instance=API.create_instance("",NULL);
    assert(instance->retrigger_held==1);
    API.set_param(instance,"retrigger_held","Off");
    char state[8192];API.get_param(instance,"state",state,sizeof(state));
    API.destroy_instance(instance);
    instance=API.create_instance("",NULL);
    API.set_param(instance,"state",state);
    assert(instance->retrigger_held==0);
    API.destroy_instance(instance);
}
static void follower_play_transform(void){
    unsigned triad=(1u<<0)|(1u<<4)|(1u<<7);
    hb_fp_config config={.rotate=1};
    assert(hb_fp_note(config,60,0,triad)==64);
    assert(hb_fp_note(config,64,0,triad)==67);
    assert(hb_fp_note(config,67,0,triad)==72);
    config.wrap=1;assert(hb_fp_note(config,67,0,triad)==60);
    config.wrap=0;config.rotate=-1;assert(hb_fp_note(config,60,0,triad)==55);
    config.rotate=0;config.mirror=1;assert(hb_fp_note(config,64,0,triad)==55);
    unsigned outside=(1u<<2)|(1u<<5)|(1u<<9)|(1u<<11);
    assert(hb_fp_note(config,62,0,outside)==59);
    assert(hb_fp_note(config,65,0,outside)==57);
    config.bypass=1;assert(hb_fp_note(config,64,0,triad)==64);
    for(int root=0;root<12;root++)for(int note=0;note<128;note++)for(int rotate=-24;rotate<=24;rotate++){
        config=(hb_fp_config){.rotate=rotate,.octave=rotate%4};
        int mapped=hb_fp_note(config,note,root,triad);
        assert(mapped>=0&&mapped<=127);
        if(rotate)assert(triad&(1u<<(mapped%12)));
    }
    Inst *instance=fixture();
    API.set_param(instance,"content_map","Chord");
    int baseline=hb_map_follower_note_now(instance,60);
    API.set_param(instance,"play_rotate","1");
    assert(hb_map_follower_note_now(instance,60)!=baseline);
    API.set_param(instance,"play_scope","Clip");
    assert(hb_map_follower_note_now(instance,60)==baseline);
    instance->movy_playback=1;
    assert(hb_map_follower_note_now(instance,60)!=baseline);
    API.set_param(instance,"play_bypass","On");
    assert(hb_map_follower_note_now(instance,60)==baseline);
    API.set_param(instance,"play_bypass","Off");
    API.set_param(instance,"travel_map","Closest Split Chromatic");
    API.set_param(instance,"split_map","135 / 2467");
    for(int note=48;note<84;note++)if(!(0xAB5u&(1u<<(note%12)))){
        int next=note+1;while(!(0xAB5u&(1u<<(next%12))))next++;
        assert(hb_map_follower_note_now(instance,note)+1==hb_map_follower_note_now(instance,next));
    }
    API.set_param(instance,"play_range","4 Oct");API.set_param(instance,"play_octave","-2");
    char state[8192];API.get_param(instance,"state",state,sizeof(state));
    API.set_param(instance,"play_reset","Reset");assert(instance->play.rotate==0&&instance->play.range==0);
    API.set_param(instance,"state",state);
    assert(instance->play.rotate==1&&instance->play.range==3&&instance->play.octave==-2&&instance->play.scope==1);
    API.get_param(instance,"base_state",state,sizeof(state));API.set_param(instance,"state",state);
    assert(instance->play.rotate==0&&instance->play.range==0);
    API.destroy_instance(instance);
}
static void follower_play_ownership(void){
    Inst *instance=fixture();
    API.set_param(instance,"arp_playback","Repeat Arp");
    API.set_param(instance,"play_scope","Clip");
    API.set_param(instance,"play_range","2 Oct");
    API.set_param(instance,"play_rotate","1");
    instance->movy_playback=1;midi(instance,1,60);instance->movy_playback=0;
    advance(instance,1,64);
    hb_cp_key *owner=NULL;
    for(int index=0;index<HB_CP_KEYS;index++)if(instance->player.keys[index].used)owner=&instance->player.keys[index];
    assert(owner&&owner->playback_origin==1&&owner->range==2&&owner->notes[0]==64);
    double next=instance->player.next_beat;
    API.set_param(instance,"play_rotate","2");advance(instance,1,64);
    assert(owner->notes[0]==67&&owner->playback_origin==1&&instance->player.next_beat==next);
    hb_cp_entry entries[HB_CP_KEYS*HB_CP_VOICES*4];
    int count=hb_cp_entries(&instance->player,entries,0);
    assert(count==2&&entries[0].pitch==67&&entries[1].pitch==79);
    uint8_t pressure[3]={0xA0,60,47};API.process_midi(instance,pressure,3,output,lengths,64);
    assert(owner->velocity==47);
    position=next;API.tick(instance,48,48000,output,lengths,64);
    assert(instance->player.next_beat>next);
    midi(instance,0,60);advance(instance,1,64);
    assert(!owner->used&&instance->player.sounding_count==0);
    API.destroy_instance(instance);
    /* Ordinary delayed notes retain clip scope through their queue as well. */
    instance=fixture();API.set_param(instance,"play_scope","Clip");API.set_param(instance,"play_rotate","1");
    instance->movy_playback=1;midi(instance,1,60);instance->movy_playback=0;advance(instance,1,64);
    assert(instance->mapped[60]==64&&instance->follower_origin[60]==1);
    API.set_param(instance,"retrigger_held","On");API.set_param(instance,"play_rotate","2");advance(instance,1,64);
    assert(instance->mapped[60]==67);
    midi(instance,0,60);advance(instance,1,64);assert(instance->mapped[60]==-1);
    API.destroy_instance(instance);
}

static void conductor_arp_range_recording(void){
    Inst *instance=fixture();
    API.set_param(instance,"role","Conductor");instance->movy_track=0;
    API.set_param(instance,"chord_mode","Scale Degree");
    API.set_param(instance,"arp_playback","Repeat Arp");
    API.set_param(instance,"play_range","2 Oct");
    midi(instance,1,60);
    for(int tick=0;tick<8;tick++)advance(instance,125,64);
    int upper=0;
    for(int index=0;index<recorded_count;index++)
        if((recorded[index][1]&0xF0)==0x90&&recorded[index][2]>=72)upper++;
    assert(upper>0);
    double next=instance->player.next_beat;
    API.set_param(instance,"play_range","1 Oct");advance(instance,1,64);
    assert(instance->player.next_beat==next);
    for(int owner=0;owner<HB_CP_KEYS;owner++)if(instance->player.keys[owner].used)
        assert(instance->player.keys[owner].range==1);
    midi(instance,0,60);advance(instance,1,64);
    assert(instance->player.sounding_count==0);
    API.destroy_instance(instance);
}

static void rapid_latched_arp_api(void){
    for(int phase=0;phase<3;phase++)for(int chords=0;chords<2;chords++){
        Inst *instance=fixture();
        API.set_param(instance,"arp_playback","Repeat Arp");
        API.set_param(instance,"arp_hold","Latch");
        API.set_param(instance,"chord_mode",chords?"Conductor Chord":"Off");
        instance->player.config.phase=phase;
        midi(instance,1,60);midi(instance,0,60);
        int total=0;
        for(int milliseconds=0;milliseconds<=500;milliseconds++){
            if(milliseconds&&(milliseconds%17==0||milliseconds%125==0)){
                int source=60+2*((milliseconds/17)%3);
                midi(instance,1,source);midi(instance,0,source);
            }
            int count=advance(instance,milliseconds?1:0,64),hits=0;
            for(int event=0;event<count;event++)if((output[event][0]&0xF0)==0x90)hits++;
            assert(hits==(milliseconds%125==0&&(milliseconds>0||phase!=1)));
            total+=hits;
        }
        assert(total==(phase==1?4:5));
        uint8_t stop[1]={0xFC};API.process_midi(instance,stop,1,output,lengths,64);
        advance(instance,0,64);assert(instance->player.sounding_count==0);
        API.destroy_instance(instance);
    }
}
static void rapid_latched_arp(void){
    for(int phase=0;phase<3;phase++)for(int same=0;same<2;same++){
        hb_chord_player player={0};hb_cp_defaults(&player.config);
        player.config.playback=1;player.config.latch=1;player.config.phase=phase;
        int note=60;hb_cp_on(&player,60,0,100,&note,1);hb_cp_off(&player,60,0);
        int last_on=-1000,total_on=0;
        for(int milliseconds=0;milliseconds<=1000;milliseconds++){
            player.beat=milliseconds/500.0;
            /* Replace released gestures faster than the arp, including exactly
               on a scheduled boundary. This must not re-arm Auto or cut a hit. */
            if(milliseconds>0&&(milliseconds%17==0||milliseconds%125==0)){
                note=same?60:60+2*((milliseconds/17)%3);
                hb_cp_on(&player,note,0,100,&note,1);hb_cp_off(&player,note,0);
            }
            int count=hb_cp_tick(&player,output,lengths,64),hits=0;
            for(int event=0;event<count;event++){
                if((output[event][0]&0xF0)==0x90){hits++;total_on++;last_on=milliseconds;}
                else if((output[event][0]&0xF0)==0x80)assert(milliseconds-last_on>=62);
            }
            int expected=(milliseconds%125==0&&(milliseconds>0||phase!=1));
            assert(hits==expected);
        }
        assert(total_on==(phase==1?8:9));
        hb_cp_clear(&player);hb_cp_tick(&player,output,lengths,64);
        assert(player.sounding_count==0);
    }
}

static void pad_global_settings(void){
    Inst *first=fixture(),*second=API.create_instance("",NULL);
    char state[2048],value[128];
    API.get_param(first,"pad_tonic_color",value,sizeof(value));assert(!strcmp(value,"Grey"));
    API.get_param(first,"pad_play_color",value,sizeof(value));assert(!strcmp(value,"Track"));
    API.get_param(first,"pad_pulse_shape",value,sizeof(value));assert(!strcmp(value,"None"));
    API.set_param(first,"pad_tonic_color","Grey");
    API.get_param(second,"pad_tonic_color",value,sizeof(value));assert(!strcmp(value,"Grey"));
    API.set_param(first,"pad_display","Lookahead");API.set_param(first,"pad_pulse_rate","2 Bars");
    API.get_param(second,"pad_display",value,sizeof(value));assert(!strcmp(value,"Lookahead"));
    API.get_param(second,"state",state,sizeof(state));assert(strstr(state,";pd1,4,6,3,4,2"));
    assert(strstr(state,";pp1,8"));
    API.set_param(second,"pad_tonic_color","Purple");
    API.set_param(second,"pad_display","Current");API.set_param(first,"state",state);
    API.get_param(first,"pad_tonic_color",value,sizeof(value));assert(!strcmp(value,"Purple"));
    API.get_param(second,"pad_display",value,sizeof(value));assert(!strcmp(value,"Current")); /* stale state cannot overwrite live global */
    API.destroy_instance(first);API.destroy_instance(second);
    first=API.create_instance("",NULL);API.set_param(first,"state",state);
    API.get_param(first,"pad_tonic_color",value,sizeof(value));assert(!strcmp(value,"Grey"));
    API.get_param(first,"pad_display",value,sizeof(value));assert(!strcmp(value,"Lookahead"));
    API.get_param(first,"pad_pulse_rate",value,sizeof(value));assert(!strcmp(value,"2 Bars"));
    API.destroy_instance(first);
}

static void pad_render_mapping(void){
    Inst *instance=fixture();API.set_param(instance,"pad_display","Both");
    uint8_t chord[3]={67,71,74};hb_commit_observed_harmony(hb_infer_harmony(chord,3));
    API.set_param(instance,"content_map","Scale");
    instance->content_map=1;instance->travel_map=0;
    instance->boundary_buffer_ms=0;instance->next_predict=0;
    char snapshot[128];unsigned current,effective,scale,lookahead;int ready;
    Inst before=*instance;unsigned sequence=g_bus.seq;
    API.get_param(instance,"pad_render",snapshot,sizeof(snapshot));
    assert(sscanf(snapshot,"%u,%u,%u,%d,%u",&current,&effective,&scale,&ready,&lookahead)==5);
    assert(!memcmp(&before,instance,sizeof(before))&&g_bus.seq==sequence);
    assert(current&(1u<<0)); /* input C renders G, a G-major chord tone */
    assert(!(current&(1u<<2))); /* input D renders A, despite D being in G major chord */
    assert(current==effective&&!ready&&!lookahead);
    for(int mode=0;mode<3;mode++)for(int travel=0;travel<7;travel++){
        instance->player.config.mode=mode;instance->travel_map=travel;
        API.get_param(instance,"pad_render",snapshot,sizeof(snapshot));
        sscanf(snapshot,"%u,%u,%u,%d,%u",&current,&effective,&scale,&ready,&lookahead);
        for(int pitch_class=0;pitch_class<12;pitch_class++){
            render_count=0;midi(instance,1,60+pitch_class);
            advance(instance,1,64);
            unsigned actual=0;
            for(int event=0;event<render_count;event++)if((rendered[event][1]&0xf0)==0x90&&rendered[event][3])actual|=1u<<mod12(rendered[event][2]);
            assert(actual);
            unsigned chord_mask=(1u<<7)|(1u<<11)|(1u<<2);
            unsigned representative=mode==1?(1u<<pitch_class):mode==2?(1u<<7):actual;
            assert(!!(current&(1u<<pitch_class))==!(representative&~chord_mask));
            midi(instance,0,60+pitch_class);advance(instance,1,64);
        }
    }
    API.destroy_instance(instance);
}

static void pad_harmony_snapshot(void){
    for(int late=0;late<2;late++){
        Inst *instance=fixture();
        instance->next_lookahead=late?10:3;hb_next_update_playhead(0,48000);
        g_bus.next_model_locked=1;g_bus.next_model_count=2;
        uint8_t c[3]={60,64,67},d[3]={62,66,69};
        g_bus.next_model[0].phase=0;g_bus.next_model[0].harmony=hb_infer_harmony(c,3);
        g_bus.next_model[1].phase=2;g_bus.next_model[1].harmony=hb_infer_harmony(d,3);
        position=late?2.6:1.3;hb_next_apply_effective(hb_clip_playhead());
        instance->approach_pad_armed=HB_APPROACH_CHROM_BELOW;
        Inst before=*instance;unsigned sequence=g_bus.seq;
        char snapshot[128];unsigned current,effective,scale;int ready;
        assert(API.get_param(instance,"pad_harmony",snapshot,sizeof(snapshot))>0);
        assert(sscanf(snapshot,"%u,%u,%u,%d",&current,&effective,&scale,&ready)==4);
        unsigned cmask=(1u<<0)|(1u<<4)|(1u<<7),dmask=(1u<<2)|(1u<<6)|(1u<<9);
        assert(current==(late?dmask:cmask));assert(effective==(late?cmask:dmask));
        assert(ready&&scale);assert(!memcmp(&before,instance,sizeof(before))&&g_bus.seq==sequence);
        unsigned lookahead;
        API.set_param(instance,"pad_display","Both");
        API.get_param(instance,"pad_render",snapshot,sizeof(snapshot));
        assert(sscanf(snapshot,"%u,%u,%u,%d,%u",&current,&effective,&scale,&ready,&lookahead)==5);
        assert(!memcmp(&before,instance,sizeof(before))&&g_bus.seq==sequence);
        instance->approach_pad_armed=HB_APPROACH_OFF;
        API.get_param(instance,"pad_render",snapshot,sizeof(snapshot));
        sscanf(snapshot,"%u,%u,%u,%d,%u",&current,&effective,&scale,&ready,&lookahead);
        if(!late)assert(lookahead!=effective); /* buffer advances beyond plain lookahead */
        g_bus.next_model_locked=0;
        API.get_param(instance,"pad_harmony",snapshot,sizeof(snapshot));
        sscanf(snapshot,"%u,%u,%u,%d",&current,&effective,&scale,&ready);assert(!ready);
        API.destroy_instance(instance);
    }
}

static void chromatic_minor_families(void){
    const char *families[]={"Minor / Min7","Dim / Min7b5"};
    for(int family=0;family<2;family++){
        Inst *instance=fixture();API.set_param(instance,"chord_mode","Scale Degree");
        /* Exercise the DSP setter, voicer and persisted enum indices. */
        API.set_param(instance,"chromatic_quality",families[family]);
        hb_cp_config config=instance->player.config;config.mode=1;config.size=3;
        int notes[HB_CP_VOICES];int count=hb_cp_voice(config,61,0,0,0xAB5,notes);
        assert(count==4&&mod12(notes[0])==1&&mod12(notes[1])==4);
        assert(mod12(notes[2])==(family?7:8)&&mod12(notes[3])==11);
        char state[8192],value[64];API.get_param(instance,"state",state,sizeof(state));
        API.set_param(instance,"chromatic_quality","Scale");API.set_param(instance,"state",state);
        API.get_param(instance,"chromatic_quality",value,sizeof(value));assert(!strcmp(value,families[family]));
        API.destroy_instance(instance);
    }
}
static void input_latch_controls(void){
    for(int mode=4;mode<=5;mode++){
        hb_chord_player player={0};hb_cp_defaults(&player.config);player.config.latch=mode;
        int tone=72;hb_cp_on(&player,60,0,100,&tone,1);
        tone=77;hb_cp_on(&player,64,0,100,&tone,1);
        assert(hb_cp_held(&player)==1);
        for(int key=0;key<HB_CP_KEYS;key++)if(player.keys[key].used)assert(player.keys[key].source==64);
        if(mode==5){assert(hb_cp_toggle_off(&player,77,0)==0);assert(hb_cp_toggle_off(&player,64,1)==0);assert(hb_cp_toggle_off(&player,64,0)==1);}
    }
    Inst *instance=fixture();API.set_param(instance,"boundary_buffer_ms","0 ms");
    API.set_param(instance,"arp_playback","Repeat Arp");API.set_param(instance,"arp_hold","Latch Acc. with Off");
    API.set_param(instance,"arp_clear_harmony","On");
    midi(instance,1,60);advance(instance,0,64);midi(instance,0,60);advance(instance,0,64);
    midi(instance,1,64);advance(instance,0,64);
    char buffer[4096];API.get_param(instance,"pad_view",buffer,sizeof(buffer));assert(strstr(buffer,"|arp1,1,60,64"));
    /* Repeat commits and transpose preserve ownership. */
    hb_commit_observed_harmony(g_bus.observed_harmony);assert(instance->player.keys[0].used);
    API.set_param(instance,"transpose","2");assert(instance->player.keys[0].used);
    const uint8_t minor[]={62,65,69};hb_commit_observed_harmony(hb_infer_harmony(minor,3));
    API.get_param(instance,"pad_view",buffer,sizeof(buffer));assert(strstr(buffer,"|arp1,1,64")&&!strstr(buffer,",60"));
    assert(hb_cp_held(&instance->player)==1);
    API.set_param(instance,"arp_hold","Latch with Off - Single");
    API.get_param(instance,"state",buffer,sizeof(buffer));API.set_param(instance,"arp_hold","Momentary");API.set_param(instance,"state",buffer);
    assert(instance->player.config.latch==5&&instance->player.config.clear_harmony==1);
    API.destroy_instance(instance);
}
int main(void){chromatic_minor_families();input_latch_controls();shuffle_cycles();cycle_rate();latch_replacement_with_off();latch_acc_with_off();pad_global_settings();pad_render_mapping();pad_harmony_snapshot();rapid_latched_arp_api();rapid_latched_arp();conductor_arp_range_recording();follower_play_transform();follower_play_ownership();pressure_recording_and_replay();arp_pressure();retrigger_default();arp_start_modes_and_offsets();arp_buffer_bypass();raw_arp_relative_mapping();arp_harmony_boundary();held_conductor_chords();recorded_master_transpose();four_bar_timing();receiver_routing();split2_and_master();split_seventh();predicted_capture();generated_degree_progression();phase_grid();voicings();forms_and_shells();ownership();strum();arp_and_latch();dominant_shift();release_harmony_and_state();conductor_chords();chord_qualities();puts("chord player: follower and conductor voicings, quality, harmony, ownership, rendering, strum, arp/latch, state and panic pass");}
