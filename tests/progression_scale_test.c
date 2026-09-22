#define main reference_tests_main
#include "follower_reference_test.c"
#undef main
static void used(Inst *instance,const char *expected){
    char value[80];API.get_param(instance,"used_scale",value,sizeof(value));
    if(strcmp(value,expected)){fprintf(stderr,"Used Scale: %s, expected %s\n",value,expected);abort();}
}
static void learn(hb_harmony_t harmony,double phase){
    hb_commit_observed_harmony(harmony);next_pending_phase=phase;hb_next_record_observed(harmony);
}
int main(void){
    Inst *instance=fixture();hb_next_reset_knowledge();
    API.set_param(instance,"follower_scale","Infer");used(instance,"Major (default)");
    learn(chord(0,1,0),0);used(instance,"Natural Minor ?");
    learn(chord(5,1,0),1);learn(chord(10,0,0),2);learn(chord(7,1,0),3);
    used(instance,"Natural Minor");
    assert(hb_parent_scale_index(instance,chord(0,0,0))==2); /* private target cannot change baseline */
    hb_next_promote_learning();assert(g_bus.next_model_locked);
    for(int event=0;event<g_bus.next_model_count;event++){
        g_bus.observed_harmony=g_bus.next_model[event].harmony;
        hb_effective_write(chord(0,0,0));
        instance->next_lookahead=event;used(instance,"Natural Minor");
        char view[2048];API.get_param(instance,"pad_view",view,sizeof(view));assert(strstr(view,"|key1,0,2"));
    }
    API.set_param(instance,"follower_scale","Lydian Dominant");used(instance,"Lydian Dominant");
    API.set_param(instance,"follower_scale","Infer");used(instance,"Natural Minor");
    hb_next_reset_knowledge();g_bus.observed_harmony=(hb_harmony_t){0};
    learn(chord(0,0,0),0);learn(chord(5,0,0),1);learn(chord(7,0,1),2);learn(chord(5,1,0),3);
    used(instance,"Major"); /* one borrowed iv doesn't replace the established progression */
    hb_next_promote_learning();
    API.set_param(instance,"follower_explicit_root","A");used(instance,"Natural Minor");
    API.set_param(instance,"follower_explicit_root","C");used(instance,"Major");
    hb_next_reset_knowledge();g_bus.observed_harmony=(hb_harmony_t){0};
    learn(chord(0,1,0),0);learn(chord(5,0,0),1);learn(chord(10,0,0),2);
    used(instance,"Dorian");
    API.set_param(instance,"follower_root_policy","Infer Notes");used(instance,"--");
    API.destroy_instance(instance);
    puts("Progression scale: learning, locked loop, lookahead independence, borrowing, reference roots, explicit override and readout pass");
}
