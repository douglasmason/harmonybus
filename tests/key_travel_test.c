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
    Inst *defaults=API.create_instance("",NULL);assert(defaults->chromatic_map==1&&defaults->travel_map==7);API.destroy_instance(defaults);
    instance->chromatic_map=1; /* The inherited fixture explicitly disables it. */
    g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=6,
        .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(6,5)};
    return instance;
}
static void contexts_and_chromatic(void){
    Inst *instance=travel_fixture();
    API.set_param(instance,"conductor_key_travel","Closest Scale Tone");
    API.set_param(instance,"follower_key_travel","Relative");
    int live_outputs[HB_KEY_TRAVEL_COUNT]={0};
    for(int origin=0;origin<2;origin++){
        instance->movy_playback=origin;
        assert(hb_key_follower_travel(instance)==(origin?HB_KEY_TRAVEL_COUNT-1:0));
        for(int policy=0;policy<HB_KEY_TRAVEL_COUNT;policy++){
            API.set_param(instance,"follower_key_travel",HB_KEY_TRAVEL[policy]);
            int target=hb_map_follower_note_unoperated(instance,65);
            if(origin)assert(target==live_outputs[policy]);else live_outputs[policy]=target;
            if(policy==0)assert(target==72);
            else {
                if(policy<5)assert(abs(target-65)<=6);
                if(policy==5)assert(target>=65);
                if(policy==6)assert(target<=65);
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
    for(int policy=-1;policy<HB_KEY_TRAVEL_COUNT;policy++){
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
    for(int target=0;target<12;target++)for(int minor=0;minor<2;minor++)for(int policy=0;policy<HB_KEY_TRAVEL_COUNT;policy++)for(int seventh=0;seventh<2;seventh++){
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
    for(int policy=1;policy<HB_KEY_TRAVEL_COUNT;policy++)for(int form=1;form<HB_CP_FORMS;form++)for(int source=0;source<116;source+=5)for(int target=0;target<12;target++){
        hb_cp_config config={0};config.mode=1;config.size=form;
        int reference[HB_CP_VOICES],pitches[HB_CP_VOICES],before[12]={0},after[12]={0};
        int reference_count=hb_cp_voice(config,source,0,0,hb_explicit_scale_mask(0,1),reference);
        int count=hb_cp_voice(config,source+target,target,0,hb_explicit_scale_mask(target,1),pitches);
        if(reference_count!=count)continue;
        int original_cost=0,final_cost=0;
        for(int voice=0;voice<count;voice++){before[mod12(pitches[voice])]++;original_cost+=abs(pitches[voice]-reference[voice]);}
        hb_closest_cache cache={0};hb_key_travel_voicing(&cache,pitches,count,reference,reference_count,policy);
        for(int voice=0;voice<count;voice++){
            assert(pitches[voice]>=0&&pitches[voice]<=127);
            if(voice)assert(pitches[voice]>pitches[voice-1]);
            after[mod12(pitches[voice])]++;final_cost+=abs(pitches[voice]-reference[voice]);
        }
        if(policy<4)assert(final_cost<=original_cost);assert(!memcmp(before,after,sizeof(before)));
    }
}
static void common_algorithms(void){
    const char *names[]={"Relative","Nearest Octave","Closest Chord Tone","Closest Scale Tone","Closest Split","Upward","Downward"};
    const char *keys[]={"travel_map","conductor_key_travel","follower_key_travel"};
    char state[32768],label[64];
    for(int algorithm=0;algorithm<7;algorithm++){
        Inst *instance=fixture();
        for(int control=0;control<3;control++)API.set_param(instance,keys[control],names[(algorithm+control)%7]);
        API.get_param(instance,"state",state,sizeof(state));API.destroy_instance(instance);
        instance=fixture();API.set_param(instance,"state",state);
        for(int control=0;control<3;control++){
            API.get_param(instance,keys[control],label,sizeof(label));assert(!strcmp(label,names[(algorithm+control)%7]));
        }
        API.destroy_instance(instance);
    }
    Inst *instance=fixture();instance->content_map=1;hb_set_shared_follower_scale(1);
    uint8_t notes[]={66,69,73};hb_harmony_t destination=hb_infer_harmony(notes,3);
    hb_effective_write(destination);
    API.set_param(instance,"travel_map","Relative");
    int relative=hb_map_follower_note_now(instance,64);
    API.set_param(instance,"travel_map","Nearest Octave");
    int nearest=hb_map_follower_note_now(instance,64);
    assert(mod12(relative)==mod12(nearest));
    /* Mode changes can move a degree beyond six semitones. Relative retains
       the contour; Nearest Octave is allowed to wrap that degree downward. */
    for(int root=0;root<12;root++)for(int scale=1;scale<10;scale++){
        hb_key_context context={.active=1,.source_root=0,.target_root=root,
            .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(root,scale)};
        for(int pitch=0;pitch<128;pitch++)for(int policy=4;policy<7;policy++){
            int mapped=hb_key_map(context,pitch),actual=hb_travel_register(pitch,mapped,policy);
            assert(actual>=0&&actual<=127&&mod12(actual)==mod12(mapped));
            if(policy==4)assert(abs(actual-pitch)<=11); /* MIDI boundary octave fallback */
            if(policy==5&&actual<pitch)assert(actual+12>127);
            if(policy==6&&actual>pitch)assert(actual-12<0);
        }
    }
    hb_key_context altered={.active=1,.source_root=0,.target_root=6,
        .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(6,5)};
    assert(hb_key_map(altered,65)==72);
    assert(hb_travel_register(65,72,4)==60);
    API.destroy_instance(instance);
}
static void none_follows_dominant_collection(void){
    Inst *instance=fixture();instance->travel_map=7;instance->content_map=1;
    instance->player.config.mode=0;instance->boundary_buffer_ms=0;
    instance->next_anti_buffer_ms=0;instance->next_lookahead=0;instance->chromatic_map=1;
    g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=2,
        .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(2,3)};
    API.set_param(instance,"follower_key_travel","Relative");
    API.set_param(instance,"dominant_minor_scale","Minimal");
    uint8_t tonic_notes[]={60,64,67,71},dominant_notes[]={67,71,74,77};
    hb_harmony_t tonic=hb_infer_harmony(tonic_notes,4),dominant=hb_infer_harmony(dominant_notes,4);
    hb_harmony_t rendered=hb_key_harmony(instance,dominant);
    assert(rendered.root_pc==9&&(hb_harmony_chord_mask(rendered)&(1u<<1)));
    assert(hb_harmony_detected_mask(rendered)==((1u<<9)|(1u<<1)|(1u<<4)|(1u<<7)));
    hb_effective_write(tonic);
    assert(hb_map_follower_note_now(instance,71)==72); /* seventh: C in D Dorian */
    hb_effective_write(dominant);
    for(int origin=0;origin<2;origin++){
        instance->movy_playback=origin;
        assert(hb_map_follower_note_now(instance,71)==73); /* V raises C to C# */
        instance->movy_pad_shift[35]=36;
        assert(hb_map_follower_note_now(instance,35)==72); /* chromatic approach targets C# first */
        instance->movy_pad_shift[35]=0;
    }
    instance->movy_playback=0;
    assert(played(instance,71)==(1u<<1));release(instance,71);
    /* Full-next color compares this pad's current output to the future chord;
       it does not silently render the input under that future collection. */
    hb_next_update_playhead(0,48000);
    g_bus.next_model_locked=1;g_bus.next_model_count=2;g_bus.clip_loop_end=4;
    g_bus.next_model[0].phase=0;g_bus.next_model[0].harmony=tonic;
    g_bus.next_model[1].phase=2;g_bus.next_model[1].harmony=dominant;
    for(int at_v=0;at_v<2;at_v++){
        position=at_v?2.1:0.1;g_bus.observed_harmony=at_v?dominant:tonic;
        hb_next_apply_effective(position);
        char snapshot[32768];unsigned current,effective,scale,full;int ready;
        API.get_param(instance,"pad_render",snapshot,sizeof(snapshot));
        assert(sscanf(snapshot,"%u,%u,%u,%d",&current,&effective,&scale,&ready)==4);
        const char *field=strstr(snapshot,"|full1,1,");assert(field&&sscanf(field,"|full1,1,%u",&full)==1);
        assert(!(full&(1u<<11))); /* C is not in A7; C# is not in Dm7 */
        assert(effective&(1u<<11)); /* actual C in Dm7 / C# in A7 */
    }
    API.set_param(instance,"dominant_minor_scale","None");
    assert(hb_map_follower_note_now(instance,71)==72);
    API.destroy_instance(instance);
}
int main(void){none_follows_dominant_collection();common_algorithms();key_center_preserves_input_classes();conductor_progression();conductor_inversions();relative_register();contexts_and_chromatic();paired_release();defaults_and_persistence();puts("key travel: conductor progression/inversions, register, shared follower policy, chromatic targets, paired releases and persistence pass");}
