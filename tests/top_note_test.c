/* Exact melody anchoring through the production chord and follower renderers. */
#define main previous_chord_player_tests
#include "chord_player_test.c"
#undef main
static void assert_top(const int *notes,int count,int top,unsigned chord){
    assert(count>0&&count<=HB_CP_VOICES&&notes[count-1]==top);
    for(int index=0;index<count-1;index++){
        assert(notes[index]>=0&&notes[index]<top);
        assert(chord&(1u<<mod12(notes[index])));
        if(index)assert(notes[index]>notes[index-1]);
    }
}
static void voicing_matrix(void){
    hb_cp_config config;hb_cp_defaults(&config);config.inversion=8;
    for(int mode=1;mode<=2;mode++)for(int form=0;form<HB_CP_FORMS;form++)
    for(int spacing=0;spacing<4;spacing++)for(int input=0;input<128;input++){
        config.mode=mode;config.size=form;config.voicing=spacing;
        int notes[12];unsigned semantic=0;
        int count=hb_cp_voice_semantic(config,input,0,0x891,0xab5,notes,&semantic);
        assert_top(notes,count,input,semantic);
    }
    config.mode=2;config.size=0;config.voicing=0;
    int notes[12];assert(hb_cp_voice(config,64,0,0x891,0xab5,notes)==4);
    const int e_top[]={55,59,60,64};expect_notes(notes,e_top,4);
    assert(hb_cp_voice(config,62,0,0x891,0xab5,notes)==5);
    const int d_top[]={52,55,59,60,62};expect_notes(notes,d_top,5);
}
static void production_mapping(void){
    for(int mode=1;mode<=2;mode++)for(int travel=0;travel<8;travel++)
    for(int transpose=-5;transpose<=5;transpose+=5)for(int spacing=0;spacing<4;spacing++){
        Inst *instance=fixture();instance->travel_map=travel;
        g_bus.global_transpose=transpose;
        instance->player.config.mode=mode;instance->player.config.inversion=8;
        instance->player.config.voicing=spacing;
        const uint8_t roots[][4]={{60,64,67,71},{62,65,69,72},{61,64,68,71}};
        for(int harmony_index=0;harmony_index<3;harmony_index++){
            instance->render_harmony=hb_infer_harmony(roots[harmony_index],4);
            hb_commit_observed_harmony(instance->render_harmony);
            instance->render_harmony_active=0;
            int source=64,expected=hb_map_follower_note_now(instance,source);
            midi(instance,1,source);advance(instance,0,64);
            hb_cp_key *key=&instance->player.keys[0];assert(key->used);
            if(key->notes[key->count-1]!=expected)fprintf(stderr,"mode %d travel %d transpose %d spacing %d harmony %d actual %d expected %d\n",mode,travel,transpose,spacing,harmony_index,key->notes[key->count-1],expected);
            assert(key->notes[key->count-1]==expected);
            for(int index=0;index<key->count;index++)assert(key->notes[index]<=expected);
            Inst *preview=malloc(sizeof(*preview));assert(preview);memcpy(preview,instance,sizeof(*preview));
            unsigned long long low=0,high=0;
            hb_pad_render_mask(preview,instance,hb_render_harmony(instance),source,0,&low,&high);
            assert(expected<64?(low&(1ULL<<expected)):(high&(1ULL<<(expected-64))));
            free(preview);
            midi(instance,0,source);advance(instance,0,64);
            assert(!hb_cp_held(&instance->player)&&!instance->player.sounding_count);
        }
        API.destroy_instance(instance);
    }
}
static void selected_harmony_and_approach(void){
    Inst *instance=fixture();instance->player.config.mode=2;instance->player.config.inversion=8;
    const uint8_t next_notes[]={62,65,69,72};
    instance->render_harmony=hb_infer_harmony(next_notes,4);instance->render_harmony_active=1;
    for(int modifier=HB_APPROACH_CHROM_BELOW;modifier<=HB_APPROACH_SCALE_ABOVE;modifier++){
        instance->approach_control=modifier;
        int expected=hb_map_follower_note_now(instance,64);
        if(modifier!=HB_APPROACH_OFF)expected=hb_apply_approach(instance,expected,modifier);
        hb_player_note_on(instance,64,0,100);
        hb_cp_key *key=&instance->player.keys[0];assert(key->used);
        assert(key->notes[key->count-1]==expected);
        memset(&instance->player.keys,0,sizeof(instance->player.keys));
    }
    API.destroy_instance(instance);
}
static void persistence_and_repeat(void){
    Inst *instance=fixture();API.set_param(instance,"chord_inversion","Top Note");
    assert(instance->player.config.inversion==8);
    char state[16384],label[64];API.get_param(instance,"state",state,sizeof(state));
    API.destroy_instance(instance);instance=API.create_instance("",0);API.set_param(instance,"state",state);
    API.get_param(instance,"chord_inversion",label,sizeof(label));assert(!strcmp(label,"Played Top Note"));
    instance->player.repeat_override=1;
    Inst *preview=malloc(sizeof(*preview));assert(preview);memcpy(preview,instance,sizeof(*preview));
    hb_pad_render_mask(preview,instance,bus_read(),64,0,0,0);
    assert(preview->player.keys[0].used&&preview->player.keys[0].count>1);
    assert(preview->player.keys[0].notes[preview->player.keys[0].count-1]==hb_map_follower_note_now(instance,64));
    free(preview);API.destroy_instance(instance);
}
int main(void){voicing_matrix();production_mapping();selected_harmony_and_approach();persistence_and_repeat();puts("top note: exact melody, chord families, spacing, travel, selected harmony, transpose, preview, release and persistence pass");return 0;}
