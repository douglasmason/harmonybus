/* Production API and scheduler regressions, including emitted render packets. */
#include <assert.h>
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
    position=0;tempo=120;transport=2;render_count=recorded_count=0;
    move_midi_fx_init(&host);
    Inst *instance=API.create_instance("",NULL);
    API.set_param(instance,"role","Follower");API.set_param(instance,"source_channel","1");
    API.set_param(instance,"render_channel","4");
    API.set_param(instance,"follower_root_policy","Explicit");API.set_param(instance,"follower_explicit_root","C");
    API.set_param(instance,"follower_scale","Major");
    uint8_t chord[4]={60,64,67,71};hb_harmony_t harmony=hb_infer_harmony(chord,4);
    hb_commit_observed_harmony(harmony);
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
static void arp_and_latch(void){
    Inst *instance=fixture();API.set_param(instance,"arp_playback","Repeat Arp");
    API.set_param(instance,"arp_hold","Latch");
    midi(instance,1,60);midi(instance,1,64);midi(instance,1,67);
    assert(advance(instance,0,64)==1&&output[0][1]==60);
    midi(instance,0,60);midi(instance,0,64);midi(instance,0,67);
    assert(advance(instance,63,64)==1&&output[0][0]==0x80);
    assert(advance(instance,62,64)==1&&output[0][1]==64);
    midi(instance,1,69);assert(advance(instance,0,64)==2&&output[1][1]==69);
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
    assert(!strcmp(state,restored)&&copy->dominant_scale==3);
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
    API.set_param(instance,"chord_dim_next","Arm");
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
int main(void){voicings();forms_and_shells();ownership();strum();arp_and_latch();dominant_shift();release_harmony_and_state();conductor_chords();chord_qualities();puts("chord player: follower and conductor voicings, quality, harmony, ownership, rendering, strum, arp/latch, state and panic pass");}
