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
    API.set_param(instance,"follower_recorded_key_travel","Relative");
    API.set_param(instance,"follower_live_key_travel","Closest Chord Tone");
    for(int origin=0;origin<2;origin++){
        instance->movy_playback=origin;
        assert(hb_key_follower_travel(instance)==(origin?0:1));
        for(int policy=0;policy<4;policy++){
            API.set_param(instance,origin?"follower_recorded_key_travel":"follower_live_key_travel",HB_KEY_TRAVEL[policy]);
            int target=hb_map_follower_note_unoperated(instance,65);
            if(policy==0)assert(target==72);
            else {
                assert(abs(target-65)<=6);
                hb_harmony_t harmony=hb_key_harmony(instance,hb_render_harmony(instance));
                unsigned legal=policy==1?hb_harmony_chord_mask(harmony):g_key_context.target_mask;
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
    API.set_param(instance,"follower_recorded_key_travel","Same as Conductor");
    instance->movy_playback=1;assert(hb_key_follower_travel(instance)==2);
    /* Ordinary follower travel must not receive a second key-travel pass. */
    instance->travel_map=0;
    int baseline=hb_map_follower_note_unoperated(instance,65);
    API.set_param(instance,"follower_recorded_key_travel","Closest Chord Tone");
    assert(hb_map_follower_note_unoperated(instance,65)==baseline);
    API.destroy_instance(instance);
}
static void paired_release(void){
    for(int origin=0;origin<2;origin++){
        Inst *instance=travel_fixture();instance->movy_playback=origin;
        const char *setting=origin?"follower_recorded_key_travel":"follower_live_key_travel";
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
static void persistence(void){
    Inst *instance=travel_fixture();char state[32768],label[64];
    API.set_param(instance,"conductor_key_travel","Closest Split");
    API.set_param(instance,"follower_recorded_key_travel","Closest Scale Tone");
    API.set_param(instance,"follower_live_key_travel","Relative");
    API.get_param(instance,"state",state,sizeof(state));API.destroy_instance(instance);
    instance=fixture();API.set_param(instance,"state",state);
    assert(g_key_conductor_travel==3&&g_key_follower_recorded_travel==2&&g_key_follower_live_travel==0);
    API.get_param(instance,"follower_recorded_key_travel",label,sizeof(label));assert(!strcmp(label,"Closest Scale Tone"));
    API.destroy_instance(instance);
    instance=fixture();API.set_param(instance,"state",";kc1,1,1,2");
    assert(g_key_conductor_travel==1&&g_key_follower_recorded_travel==-1&&g_key_follower_live_travel==-1);
    API.destroy_instance(instance);
}
int main(void){relative_register();contexts_and_chromatic();paired_release();persistence();puts("key travel: register, separate origins, chromatic targets, paired releases and persistence pass");}
