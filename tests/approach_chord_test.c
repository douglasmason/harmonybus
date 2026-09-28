#define main previous_chord_player_tests
#include "chord_player_test.c"
#undef main
static hb_cp_key *played_key(Inst *instance,int source){
    for(int index=0;index<HB_CP_KEYS;index++)if(instance->player.keys[index].used&&instance->player.keys[index].source==source)return &instance->player.keys[index];
    assert(0);return 0;
}
static void approach_families(void){
    static const int thirds[]={0,4,4,3,3,3},fifths[]={0,7,7,6,7,6},sevenths[]={0,11,10,9,10,10};
    for(int mode=1;mode<=2;mode++)for(int family=1;family<6;family++)for(int seventh=0;seventh<2;seventh++)for(int top=0;top<2;top++){
        Inst *instance=fixture();instance->chromatic_map=1;instance->travel_map=7;
        instance->player.config.mode=mode;instance->player.config.size=seventh?3:2;
        instance->player.config.chromatic_quality=family;instance->player.config.inversion=top?8:0;
        instance->player.config.quality=1; /* Approach family overrides regular quality. */
        API.set_param(instance,"hb_movy_input_approach","29,36");midi(instance,1,29);advance(instance,0,64);
        hb_cp_key *key=played_key(instance,29);
        unsigned expected=(1u<<4)|(1u<<mod12(4+thirds[family]))|(1u<<mod12(4+fifths[family]));
        if(seventh)expected|=1u<<mod12(4+sevenths[family]);
        unsigned actual=0;for(int index=0;index<key->count;index++)actual|=1u<<mod12(key->notes[index]);
        assert(key->root_pc==4&&actual==expected&&key->count==(seventh?4:3));
        assert(top?key->notes[key->count-1]==64:key->notes[0]==64);
        assert(instance->player.config.mode==mode&&instance->player.config.quality==1);
        assert(key->onset_config.mode==mode&&key->onset_config.quality==1);
        midi(instance,0,29);advance(instance,0,64);assert(!instance->player.sounding_count);
        /* Recorded alias carries the same role independently of physical input. */
        instance->recorded_action_valid[29]=1;instance->recorded_actions[29][HB_MOTION_LANES]=8;instance->movy_playback=1;
        midi(instance,1,29);advance(instance,0,64);key=played_key(instance,29);
        actual=0;for(int index=0;index<key->count;index++)actual|=1u<<mod12(key->notes[index]);assert(actual==expected);
        midi(instance,0,29);advance(instance,0,64);API.destroy_instance(instance);
    }
}
static void same_root_and_preview(void){
    Inst *instance=fixture();instance->chromatic_map=1;instance->travel_map=7;
    instance->player.config.mode=1;instance->player.config.size=3;
    assert(instance->player.config.chromatic_quality==3);
    API.set_param(instance,"hb_movy_input_approach","29,36");midi(instance,1,29);midi(instance,1,64);advance(instance,0,64);
    const int diminished[]={64,67,70,73},diatonic[]={64,67,71,74};
    expect_notes(played_key(instance,29)->notes,diminished,4);expect_notes(played_key(instance,64)->notes,diatonic,4);
    Inst *preview=malloc(sizeof(*preview));assert(preview);memcpy(preview,instance,sizeof(*preview));
    unsigned long long low=0,high=0;hb_pad_render_mask(preview,instance,bus_read(),29,0,&low,&high);
    assert(!low&&high==((1ULL<<0)|(1ULL<<3)|(1ULL<<6)|(1ULL<<9)));free(preview);
    midi(instance,0,29);advance(instance,0,64);assert(played_key(instance,64)->held);
    midi(instance,0,64);advance(instance,0,64);assert(!instance->player.sounding_count);
    API.destroy_instance(instance);
}
static void mapped_chromatic_inputs(void){
    Inst *instance=fixture();instance->chromatic_map=1;instance->player.config.mode=1;
    instance->content_map=0;instance->travel_map=0;
    /* Explicit recorded target remains chromatic even if raw/root is diatonic. */
    instance->movy_input_target[64]=66;instance->movy_input_degree[64]=3;
    instance->render_harmony=bus_read();instance->render_harmony_active=1;
    int root=hb_map_follower_note_now(instance,64);hb_player_note_on(instance,64,0,100);
    hb_cp_key *key=played_key(instance,64);assert(key->root_pc==mod12(root));
    unsigned expected=(1u<<mod12(root))|(1u<<mod12(root+3))|(1u<<mod12(root+6));assert(key->semantic_mask==expected);
    API.destroy_instance(instance);
}
int main(void){approach_families();same_root_and_preview();mapped_chromatic_inputs();puts("approach chords: role-based families, same-root diatonic distinction, top note, recorded aliases, preview and ownership pass");return 0;}
