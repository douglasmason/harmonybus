#define HB_PARALLEL_FIXTURE
#include "parallel_harmony_test.c"

static void expect_value(Inst *instance,const char *key,const char *expected){
    char value[256];API.get_param(instance,key,value,sizeof(value));assert(!strcmp(value,expected));
}
int main(void){
    Inst *instance=fixture(),*other=API.create_instance("",NULL);
    API.set_param(other,"role","Follower");
    API.set_param(instance,"defaults_editor","Follower Chords");
    API.set_param(instance,"follower_default_chord_form","Ninth");
    API.set_param(instance,"defaults_control_1","Minor");
    expect_value(other,"chord_quality","Minor");
    expect_value(other,"chord_form","Ninth");
    API.set_param(instance,"defaults_editor","Conductor Chords");
    API.set_param(instance,"conductor_default_chord_form","Triad");
    expect_value(other,"chord_form","Ninth");
    API.set_param(instance,"defaults_editor","Follower Scales");
    API.set_param(instance,"defaults_control_1","Strict Local");
    API.set_param(instance,"defaults_control_5","Lyd / Aeo / Loc#2");
    expect_value(other,"gap_scale","Strict Local");
    expect_value(other,"local_major","Lydian");
    expect_value(other,"local_minor","Aeolian");
    expect_value(other,"local_halfdim","Locrian #2");
    expect_value(instance,"defaults_control_6","All followers");
    API.set_param(other,"local_palette","Ion / Dor / Loc");
    API.set_param(instance,"defaults_control_5","Lyd / Dor / Loc#2");
    expect_value(other,"local_major","Ionian");
    API.set_param(other,"local_palette","Role Default");
    expect_value(other,"local_major","Lydian");
    expect_value(other,"local_palette","Follow Role");
    char metadata[65536];int size=API.get_param(instance,"chain_params",metadata,sizeof(metadata));
    assert(size>0&&size<(int)sizeof(metadata));
    assert(strstr(metadata,"\"key\":\"defaults_control_5\",\"name\":\"Local Palette\""));
    API.set_param(instance,"motion_control_33","Current");expect_value(instance,"motion_control_33","Current");
    API.set_param(instance,"motion_control_33","Next");expect_value(instance,"motion_control_33","Next");
    API.destroy_instance(other);API.destroy_instance(instance);
    puts("Settings editor: role destinations, palette inheritance, metadata and harmony target passed");
    return 0;
}
