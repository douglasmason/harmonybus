#define main reference_tests_main
#include "follower_reference_test.c"
#undef main
static uint16_t collection(Inst *instance,hb_harmony_t harmony){return hb_follower_scale_target(instance,harmony).pitch_mask;}
static void expect_collection(Inst *instance,hb_harmony_t harmony,uint16_t expected){
    uint16_t actual=collection(instance,harmony);
    if(actual!=expected){fprintf(stderr,"collection %s: got %x expected %x\n",harmony.name,actual,expected);abort();}
    assert((actual&hb_harmony_chord_mask(harmony))==hb_harmony_chord_mask(harmony));
}
int main(void){
    Inst *instance=fixture();
    uint16_t major=hb_explicit_scale_mask(0,1),minor=hb_explicit_scale_mask(0,2);
    API.set_param(instance,"follower_scale","Natural Minor");
    /* Bb major belongs to both C major and C minor relatives: explicit minor wins. */
    hb_harmony_t harmony=chord(10,0,0);
    expect_collection(instance,harmony,minor);
    hb_commit_observed_harmony(harmony);instance->travel_map=7;
    char view[8192];API.get_param(instance,"pad_view",view,sizeof(view));
    unsigned current,effective,scale;
    assert(sscanf(view,"%u,%u,%u",&current,&effective,&scale)==3&&scale==minor);
    /* None leaves the pitches alone. The same rendered collection drives grey. */
    assert(hb_map_follower_note_now(instance,63)==63&&hb_map_follower_note_now(instance,64)==64);
    assert((scale&(1u<<3))&&!(scale&(1u<<4)));
    API.set_param(instance,"follower_scale","Major");
    expect_collection(instance,chord(5,1,0),(major&~(1u<<9))|(1u<<8));
    expect_collection(instance,chord(2,0,1),(major&~(1u<<5))|(1u<<6));
    expect_collection(instance,chord(8,0,0),(major&~((1u<<9)|(1u<<4)))|(1u<<8)|(1u<<3));
    API.set_param(instance,"borrowed_scale","Mixolydian b6");
    expect_collection(instance,chord(5,1,0),hb_explicit_scale_mask(0,13));
    API.set_param(instance,"borrowed_scale","Aeolian");
    expect_collection(instance,chord(5,1,0),minor);
    API.set_param(instance,"borrowed_scale","Dorian");
    expect_collection(instance,chord(3,0,0),hb_explicit_scale_mask(0,3));
    expect_collection(instance,chord(5,1,0),minor); /* chord Ab wins over Dorian A */
    expect_collection(instance,chord(0,0,0),major); /* override only borrowed family */
    API.set_param(instance,"dominant_scale","Altered V");
    uint16_t altered=hb_explicit_scale_mask(7,15);
    expect_collection(instance,chord(7,0,1),altered|hb_harmony_chord_mask(chord(7,0,1)));
    API.set_param(instance,"dominant_scale","Off");
    API.set_param(instance,"borrowed_scale","Minimal");
    /* Each melodic-minor mode is exactly its rotation; original IDs stay fixed. */
    const int ids[]={9,10,11,12,13,14,15};
    const int offsets[]={0,2,3,5,7,9,11};
    for(int mode=0;mode<7;mode++)for(int root=0;root<12;root++){
        assert(hb_explicit_scale_mask(root,ids[mode])==hb_explicit_scale_mask(mod12(root-offsets[mode]),9));
        hb_set_shared_follower_scale(ids[mode]);
        assert(hb_follower_input_scale_index(instance,root)==ids[mode]);
    }
    API.set_param(instance,"follower_scale","Mixolydian b6");
    API.set_param(instance,"borrowed_scale","Mixolydian b6");
    API.get_param(instance,"state",view,sizeof(view));assert(strstr(view,";ss1,0,3"));
    API.destroy_instance(instance);instance=API.create_instance("",0);
    API.set_param(instance,"state",view);
    assert(hb_shared_follower_scale()==13&&hb_shared_borrowed_scale()==3);
    API.destroy_instance(instance);
    instance=fixture();Inst *other=API.create_instance("",0);
    API.set_param(instance,"dominant_scale","Altered V");
    API.set_param(other,"borrowed_scale","Aeolian");
    char parameter[64],stale[8192],disabled[8192];
    API.get_param(other,"dominant_scale",parameter,sizeof(parameter));assert(!strcmp(parameter,"Altered V"));
    API.get_param(instance,"borrowed_scale",parameter,sizeof(parameter));assert(!strcmp(parameter,"Aeolian"));
    API.get_param(instance,"state",stale,sizeof(stale));
    API.set_param(other,"dominant_scale","Off");API.set_param(other,"borrowed_scale","Minimal");
    API.get_param(other,"state",disabled,sizeof(disabled));
    API.set_param(instance,"state",stale);
    assert(!hb_shared_dominant_scale()&&!hb_shared_borrowed_scale());
    API.destroy_instance(other);API.destroy_instance(instance);
    instance=API.create_instance("",0);other=API.create_instance("",0);
    API.set_param(instance,"state",disabled);API.set_param(other,"state",stale);
    assert(!hb_shared_dominant_scale()&&!hb_shared_borrowed_scale());
    API.destroy_instance(other);API.destroy_instance(instance);
    puts("Follower baseline, truthful pad scale, borrowing, dominant override and melodic-minor modes pass");
}
