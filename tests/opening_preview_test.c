#define main existing_chord_tests
#include "chord_player_test.c"
#undef main
int main(void){
    Inst *conductor=fixture();API.set_param(conductor,"role","Conductor");
    Inst *follower=API.create_instance("",NULL);API.set_param(follower,"role","Follower");
    g_movy_running=0;g_bus.observed_harmony=(hb_harmony_t){0};hb_effective_write((hb_harmony_t){0});
    API.set_param(conductor,"chord_mode","Scale Degree");API.set_param(conductor,"chord_form","Triad");
    API.set_param(conductor,"hb_opening_preview","1;62,0");
    int before=render_count,recorded_before=recorded_count;unsigned seq=g_bus.seq;
    hb_harmony_t opening=hb_opening_harmony();assert(opening.valid&&opening.root_pc==2);
    assert(hb_harmony_chord_mask(opening)==((1<<2)|(1<<5)|(1<<9)));
    char view[8192];API.get_param(follower,"pad_view",view,sizeof(view));
    unsigned current,effective,scale,look;int ready;
    assert(sscanf(view,"%u,%u,%u,%d,%u",&current,&effective,&scale,&ready,&look)==5);
    assert(current&&effective&&scale&&ready&&look);assert(strstr(view,"|full1,1,"));
    assert(!g_bus.observed_harmony.valid&&g_bus.seq==seq&&!hb_cp_held(&conductor->player));
    assert(render_count==before&&recorded_count==recorded_before);
    API.set_param(conductor,"hb_opening_preview","1;60,1;63,1;67,1");
    opening=hb_opening_harmony();assert(hb_harmony_chord_mask(opening)==((1<<0)|(1<<3)|(1<<7)));
    API.set_param(conductor,"chord_quality","Min7");
    API.set_param(conductor,"track_chord_form","Power");
    API.set_param(conductor,"hb_opening_preview","1;62,0");
    opening=hb_opening_harmony();assert(opening.root_pc==2);
    assert(hb_harmony_chord_mask(opening)==0x225&&hb_harmony_detected_mask(opening)==0x204);
    API.set_param(conductor,"track_chord_form","Seventh");
    opening=hb_opening_harmony();assert(hb_harmony_chord_mask(opening)==0x225&&hb_harmony_detected_mask(opening)==0x225);
    assert(render_count==before&&recorded_count==recorded_before&&g_bus.seq==seq);
    API.set_param(conductor,"hb_opening_preview","0");assert(!hb_opening_harmony().valid);
    API.set_param(conductor,"hb_opening_preview","1;60,1;999,1");assert(!hb_opening_harmony().valid);
    API.set_param(conductor,"hb_opening_preview","1;60,1;64,1;67,1");g_movy_running=1;assert(!hb_opening_harmony().valid);
    API.destroy_instance(follower);API.destroy_instance(conductor);
    puts("Opening preview: raw auto-chords, baked notes, masks, mute/empty, malformed and silent ownership passed");
}
