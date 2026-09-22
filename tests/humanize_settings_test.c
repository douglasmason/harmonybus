#define main reference_suite_main
#include "follower_reference_test.c"
#undef main
static void human_expect(Inst *instance,const char *key,const char *expected){
    char value[256];API.get_param(instance,key,value,sizeof(value));assert(!strcmp(value,expected));
}
int main(void){
    Inst *first=fixture(),*second=API.create_instance("",NULL);
    human_expect(first,"humanize_timing","0");
    API.set_param(first,"humanize_timing","12");
    API.set_param(first,"humanize_velocity","9");
    API.set_param(first,"humanize_gate","15");
    human_expect(second,"humanize_timing","12");
    human_expect(second,"humanize_velocity","9");
    human_expect(second,"humanize_gate","15");
    char saved[16384],context[256];API.get_param(first,"state",saved,sizeof(saved));
    assert(strstr(saved,";hu1,12,9,15"));
    API.get_param(first,"follower_input_context",context,sizeof(context));assert(!strstr(context,"|"));
    first->movy_track=0;first->role=0;second->movy_track=1;second->role=1;
    API.get_param(first,"follower_input_context_v2",context,sizeof(context));assert(strstr(context,"|hu1,12,9,15,3"));
    memset(g_humanize,0,sizeof(g_humanize));g_humanize_restored=0;
    API.set_param(second,"state",saved);
    human_expect(first,"humanize_timing","12");
    API.set_param(second,"humanize_timing","0");API.set_param(first,"state",saved);
    human_expect(first,"humanize_timing","0"); /* Later track restores cannot undo a shared edit. */
    char *suffix=strstr(saved,";hu1,");assert(suffix);*suffix=0;
    g_humanize_restored=0;API.set_param(first,"state",saved);
    human_expect(first,"humanize_velocity","0");human_expect(first,"humanize_gate","0");
    puts("Humanize: shared controls, zero defaults, backward-compatible context, first restore and persistence pass");
}
