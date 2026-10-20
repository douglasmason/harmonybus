/* Explicit playback role survives a projected pitch becoming diatonic. */
#define main reference_suite_main
#include "follower_reference_test.c"
#undef main
int main(void){
    Inst *instance=fixture();char text[256];
    instance->movy_track=3;
    API.set_param(instance,"follower_scale","Lydian");
    API.get_param(instance,"follower_input_context",text,sizeof(text));
    assert(!strcmp(text,"fic1,0,5,8"));
    const int roots[]={0,2,7,9},minor[]={0,1,0,1};
    for(int index=0;index<4;index++){
        g_bus.observed_harmony=chord(roots[index],minor[index],0);
        hb_effective_write(g_bus.observed_harmony);
        instance->travel_map=6;instance->content_map=1;
        API.set_param(instance,"hb_movy_input_role","66,3,67");
        int target=hb_map_follower_note_now(instance,67);
        assert(hb_map_follower_note_now(instance,66)==target-1);
        instance->published_follower[66]=1;instance->mapped[66]=target-1;
        instance->follower_path_harmony[66]=g_bus.observed_harmony;
        API.get_param(instance,"fpath_0_0_1",text,sizeof(text));
        assert(!strcmp(text,"5th-1"));
        API.set_param(instance,"hb_movy_input_role","66,-1,-1");
        assert(instance->movy_input_target[66]==0);
    }
    /* Input role is also explicit for ordinary Closest Split. */
    API.set_param(instance,"follower_scale","Locrian");
    API.set_param(instance,"hb_movy_input_role","66,3,-1");
    assert(!strcmp(hb_follower_degree_role_for_note(instance,66),"4th"));
    instance->movy_playback=0;
    uint8_t input[]={0x90,66,100},output[32][3];int lengths[32];
    API.process_midi(instance,input,3,output,lengths,32);
    assert(instance->movy_input_degree[66]==0);
    assert(!strcmp(hb_follower_degree_role_for_note(instance,66),"5th"));
    puts("recorded input: global context, chromatic collision, role display and fresh live input pass");
    return 0;
}
