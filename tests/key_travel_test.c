#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"

static void relative_register(void){
    const int melody[]={60,62,64,65,67,69,71,72};
    for(int root=0;root<12;root++)for(int scale=1;scale<=9;scale++){
        hb_key_context context={.active=1,.source_root=0,.target_root=root,
            .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(root,scale)};
        int previous=-1;
        for(int index=0;index<8;index++){
            int pitch=hb_key_map(context,melody[index]);
            assert(pitch>previous);previous=pitch;
            assert(hb_key_map(context,melody[index]+12)==pitch+12);
        }
    }
    hb_key_context context={.active=1,.source_root=0,.target_root=6,
        .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(6,5)};
    assert(hb_key_map(context,64)==70&&hb_key_map(context,65)==72);
}
static Inst *travel_fixture(void){
    Inst *instance=fixture();instance->travel_map=7;instance->content_map=1;
    instance->player.config.mode=0;instance->boundary_buffer_ms=0;
    Inst *defaults=API.create_instance("",NULL);assert(defaults->chromatic_map==1);API.destroy_instance(defaults);
    instance->chromatic_map=1; /* The inherited fixture explicitly disables it. */
    g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=6,
        .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(6,5)};
    return instance;
}
static void contexts_and_chromatic(void){
    Inst *instance=travel_fixture();
    API.set_param(instance,"conductor_key_travel","Closest Scale Tone");
    API.set_param(instance,"follower_key_travel","Relative");
    int live_outputs[4]={0};
    for(int origin=0;origin<2;origin++){
        instance->movy_playback=origin;
        assert(hb_key_follower_travel(instance)==(origin?3:0));
        for(int policy=0;policy<4;policy++){
            API.set_param(instance,"follower_key_travel",HB_KEY_TRAVEL[policy]);
            int target=hb_map_follower_note_unoperated(instance,65);
            if(origin)assert(target==live_outputs[policy]);else live_outputs[policy]=target;
            if(policy==0)assert(target==72);
            else {
                assert(abs(target-65)<=6);
                hb_harmony_t harmony=hb_key_harmony(instance,hb_render_harmony(instance));
                unsigned legal=policy==1?(g_key_context.target_mask&~hb_harmony_chord_mask(harmony)):g_key_context.target_mask;
                assert(legal&(1u<<mod12(target)));
            }
            /* E-flat approaches the relocated E, not a separately snapped note. */
            assert(hb_map_follower_note_unoperated(instance,63)==hb_map_follower_note_unoperated(instance,64)-1);
            /* The same rule applies to a physical approach-row alias. */
            instance->movy_pad_shift[61]=4;
            assert(hb_map_follower_note_unoperated(instance,61)==target-1);
            instance->movy_pad_shift[61]=0;
            instance->chromatic_map=0;
            assert(hb_map_follower_note_unoperated(instance,65)==target);
            instance->chromatic_map=1;
        }
    }
    API.set_param(instance,"follower_key_travel","Same as Conductor");
    instance->movy_playback=1;assert(hb_key_follower_travel(instance)==2);
    /* Ordinary follower travel must not receive a second key-travel pass. */
    instance->travel_map=0;
    int baseline=hb_map_follower_note_unoperated(instance,65);
    API.set_param(instance,"follower_key_travel","Closest Chord Tone");
    assert(hb_map_follower_note_unoperated(instance,65)==baseline);
    API.destroy_instance(instance);
}
static void key_center_preserves_input_classes(void){
    const int inputs[]={60,62,64,65,67,69,71};
    for(int travel=5;travel<=7;travel+=2)for(int root=0;root<12;root++)for(int minor=0;minor<2;minor++){
        Inst *instance=travel_fixture();instance->travel_map=travel;
        hb_commit_observed_harmony((hb_harmony_t){.valid=1,.root_pc=0,.chord_index=0,.pitch_mask=0x91});
        g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=root,
            .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(root,minor?2:1)};
        g_key_follower_travel=1;
        hb_harmony_t destination=hb_key_harmony(instance,hb_render_harmony(instance));
        unsigned chord_mask=hb_harmony_chord_mask(destination),seen=0;
        for(int index=0;index<7;index++){
            int output=hb_map_follower_note_unoperated(instance,inputs[index]);
            unsigned bit=1u<<mod12(output);
            assert(!(seen&bit));seen|=bit;
            assert(!!(chord_mask&bit)==(index==0||index==2||index==4));
        }
        API.destroy_instance(instance);
    }
}
static void paired_release(void){
    for(int origin=0;origin<2;origin++){
        Inst *instance=travel_fixture();instance->movy_playback=origin;
        const char *setting="follower_key_travel";
        API.set_param(instance,setting,"Relative");
        render_count=0;midi(instance,1,65);advance(instance,2,64);
        assert(instance->mapped[65]==72);
        API.set_param(instance,setting,"Closest Scale Tone");
        render_count=0;midi(instance,0,65);advance(instance,2,64);
        int paired=0;
        for(int index=0;index<render_count;index++)if((rendered[index][1]&0xf0)==0x80){assert(rendered[index][2]==72);paired++;}
        assert(paired==1);API.destroy_instance(instance);
    }
}
static void defaults_and_persistence(void){
    Inst *instance=travel_fixture();char state[32768],label[64];
    assert(g_key_conductor_travel==3&&g_key_follower_travel==0);
    API.get_param(instance,"conductor_key_travel",label,sizeof(label));assert(!strcmp(label,"Closest Split"));
    API.get_param(instance,"follower_key_travel",label,sizeof(label));assert(!strcmp(label,"Relative"));
    API.destroy_instance(instance);
    for(int policy=-1;policy<4;policy++){
        instance=travel_fixture();
        API.set_param(instance,"follower_key_travel",policy<0?"Same as Conductor":HB_KEY_TRAVEL[policy]);
        API.get_param(instance,"state",state,sizeof(state));API.destroy_instance(instance);
        instance=fixture();API.set_param(instance,"state",state);
        assert(g_key_conductor_travel==3&&g_key_follower_travel==policy);
        for(int origin=0;origin<2;origin++){
            instance->movy_playback=origin;assert(hb_key_follower_travel(instance)==(policy<0?3:policy));
        }
        API.destroy_instance(instance);
    }
    /* Conflicting old settings preserve recorded playback; equal policies and
       explicit inheritance round-trip without introducing two runtime knobs. */
    const char *legacy[]={";kc1,1,1,2", ";kc1,1,1,2;kt2,2,2,0",
        ";kc1,1,1,2;kt2,2,1,1", ";kc1,1,1,2;kt2,2,-1,-1",
        ";kc1,1,1,2;kt2,2,2,0;kt3,3,1"};
    const int conductors[]={1,2,2,2,3},followers[]={0,2,1,-1,1};
    for(int index=0;index<5;index++){
        instance=fixture();API.set_param(instance,"state",legacy[index]);
        assert(g_key_conductor_travel==conductors[index]&&g_key_follower_travel==followers[index]);
        API.destroy_instance(instance);
    }
    /* Old clients may address either alias, but both edit the shared value. */
    instance=fixture();
    API.set_param(instance,"follower_live_key_travel","Closest Chord Tone");
    API.get_param(instance,"follower_recorded_key_travel",label,sizeof(label));assert(!strcmp(label,"Closest Chord Tone"));
    API.set_param(instance,"follower_recorded_key_travel","Relative");
    API.get_param(instance,"follower_key_travel",label,sizeof(label));assert(!strcmp(label,"Relative"));
    API.destroy_instance(instance);
}
static void conductor_progression(void){
    const int inputs[4]={62,67,60,69};
    const int major_degrees[4]={2,7,0,9},minor_degrees[4]={2,7,0,8};
    const int major_thirds[4]={3,4,4,3},minor_thirds[4]={3,4,3,4};
    for(int target=0;target<12;target++)for(int minor=0;minor<2;minor++)for(int policy=0;policy<4;policy++)for(int seventh=0;seventh<2;seventh++){
        Inst *instance=fixture();
        API.set_param(instance,"role","Conductor");API.set_param(instance,"source_channel","1");
        API.set_param(instance,"chord_mode","Scale Degree");API.set_param(instance,"chord_form",seventh?"Seventh":"Triad");
        API.set_param(instance,"dominant_minor_scale","Harmonic Minor");
        g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=target,
            .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(target,minor?2:1)};
        g_key_conductor_travel=policy;instance->movy_playback=1;
        for(int step=0;step<4;step++){
            int root=mod12(target+(minor?minor_degrees[step]:major_degrees[step]));
            int third=minor?minor_thirds[step]:major_thirds[step],fifth=minor&&step==0?6:7;
            unsigned expected=(1u<<root)|(1u<<mod12(root+third))|(1u<<mod12(root+fifth));
            if(seventh)expected|=1u<<mod12(root+(minor?(step==3?11:10):(step==2?11:10)));
            unsigned actual=played(instance,inputs[step]);
            if(actual!=expected)fprintf(stderr,"conductor target%d minor%d policy%d step%d: %x != %x\n",target,minor,policy,step,actual,expected);
            assert(actual==expected);release(instance,inputs[step]);
        }
        API.destroy_instance(instance);
    }
}
static void conductor_inversions(void){
    Inst *instance=fixture();
    API.set_param(instance,"role","Conductor");API.set_param(instance,"source_channel","1");
    API.set_param(instance,"chord_mode","Scale Degree");API.set_param(instance,"chord_form","Triad");
    g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=9,
        .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(9,2)};
    g_key_conductor_travel=1;instance->movy_playback=1;
    assert(played(instance,62)==((1u<<11)|(1u<<2)|(1u<<5)));
    int notes[3],count=0;
    for(int event=0;event<render_count;event++)if((rendered[event][1]&0xf0)==0x90&&rendered[event][3]){
        assert(count<3);notes[count++]=rendered[event][2];
    }
    hb_cp_sort(notes,count);assert(count==3);
    const int expected[3]={62,65,71};expect_notes(notes,expected,3); /* Bdim/D beside recorded D-F-A. */
    /* Releases own their emitted pitches across another key/policy change. */
    g_key_context.target_root=2;g_key_context.target_mask=hb_explicit_scale_mask(2,1);g_key_conductor_travel=0;
    render_count=0;release(instance,62);unsigned released=0;
    for(int event=0;event<render_count;event++)if((rendered[event][1]&0xf0)==0x80)
        released|=1u<<mod12(rendered[event][2]);
    assert(released==((1u<<11)|(1u<<2)|(1u<<5)));API.destroy_instance(instance);
    /* Octave duplicates, extended forms and MIDI boundaries keep every voice
       and never cost more ordered motion than the original relative voicing. */
    for(int form=1;form<HB_CP_FORMS;form++)for(int source=0;source<116;source+=5)for(int target=0;target<12;target++){
        hb_cp_config config={0};config.mode=1;config.size=form;
        int reference[HB_CP_VOICES],pitches[HB_CP_VOICES],before[12]={0},after[12]={0};
        int reference_count=hb_cp_voice(config,source,0,0,hb_explicit_scale_mask(0,1),reference);
        int count=hb_cp_voice(config,source+target,target,0,hb_explicit_scale_mask(target,1),pitches);
        if(reference_count!=count)continue;
        int original_cost=0,final_cost=0;
        for(int voice=0;voice<count;voice++){before[mod12(pitches[voice])]++;original_cost+=abs(pitches[voice]-reference[voice]);}
        hb_closest_cache cache={0};hb_key_closest_voicing(&cache,pitches,count,reference,reference_count);
        for(int voice=0;voice<count;voice++){
            assert(pitches[voice]>=0&&pitches[voice]<=127);
            if(voice)assert(pitches[voice]>pitches[voice-1]);
            after[mod12(pitches[voice])]++;final_cost+=abs(pitches[voice]-reference[voice]);
        }
        assert(final_cost<=original_cost);assert(!memcmp(before,after,sizeof(before)));
    }
}
int main(void){key_center_preserves_input_classes();conductor_progression();conductor_inversions();relative_register();contexts_and_chromatic();paired_release();defaults_and_persistence();puts("key travel: conductor progression/inversions, register, shared follower policy, chromatic targets, paired releases and persistence pass");}
