#define main reference_tests_main
#include "follower_reference_test.c"
#undef main
static void expect_scale(Inst *instance,const char *expected){
    char value[2048];API.get_param(instance,"follower_scale",value,sizeof(value));assert(!strcmp(value,expected));
}
int main(void){
    Inst *first=fixture(),*second=API.create_instance("",0),*conductor=API.create_instance("",0);
    API.set_param(second,"role","Follower");API.set_param(conductor,"role","Conductor");
    API.set_param(first,"follower_scale","Phrygian");
    expect_scale(second,"Phrygian");expect_scale(conductor,"Phrygian");
    char stale[8192];API.get_param(first,"state",stale,sizeof(stale));
    API.set_param(second,"follower_scale","Major");
    API.set_param(first,"state",stale);expect_scale(first,"Major");expect_scale(second,"Major");
    API.set_param(conductor,"follower_scale","Dorian");expect_scale(first,"Dorian");expect_scale(second,"Dorian");
    char left[8192],right[8192];API.get_param(first,"base_state",left,sizeof(left));API.get_param(second,"base_state",right,sizeof(right));
    assert(strstr(left,",3,0")&&strstr(right,",3,0"));
    API.set_param(second,"follower_scale","Infer");g_bus.observed_harmony=chord(0,1,0);
    assert(hb_follower_input_scale_index(first,0)==2&&hb_follower_input_scale_index(second,0)==2);
    char view[2048];API.get_param(conductor,"pad_view",view,sizeof(view));assert(strstr(view,"|key1,0,2"));
    API.destroy_instance(second);API.destroy_instance(conductor);API.destroy_instance(first);
    first=API.create_instance("",0);API.set_param(first,"state",stale);expect_scale(first,"Phrygian");
    API.set_param(first,"follower_scale","Major");second=API.create_instance("",0);
    API.set_param(second,"state",stale);expect_scale(second,"Major");
    API.destroy_instance(first);API.destroy_instance(second);
    puts("global follower scale: cross-track edits, conductor keyboard metadata, Auto, migration and stale-restore protection pass");
}
