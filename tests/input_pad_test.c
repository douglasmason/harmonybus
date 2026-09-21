/* Production input-scale metadata, split mapping, and current/target separation. */
#define main reference_suite_main
#include "follower_reference_test.c"
#undef main
int main(void){
    Inst *instance=fixture();
    const int roots[]={2,7,0,9},minor[]={1,0,0,1};
    char view[2048];
    for(int scale=1;scale<=9;scale++)for(int h=0;h<4;h++){
        instance->follower_scale=scale;instance->content_map=1;
        g_bus.global_transpose=0;g_bus.observed_harmony=chord(roots[h],minor[h],0);
        hb_effective_write(g_bus.observed_harmony);
        uint16_t input=hb_follower_input_scale(instance,0);
        hb_harmony_t target=hb_follower_scale_target(instance,g_bus.observed_harmony);
        unsigned chord_mask=hb_harmony_chord_mask(g_bus.observed_harmony);
        assert((target.pitch_mask&chord_mask)==chord_mask);
        API.get_param(instance,"pad_view",view,sizeof(view));
        const char *metadata=strstr(view,"|input1,");assert(metadata);
        int root,selected,resolved;unsigned mask,roles;
        assert(sscanf(metadata,"|input1,%d,%d,%d,%u,%u",&root,&selected,&resolved,&mask,&roles)==5);
        assert(root==0&&selected==scale&&resolved==scale&&mask==input);
        assert((roles&~input)==0&&(roles&1));
        for(int degree=0;degree<7;degree++){
            int pitch=60+hb_nth_scale_interval_from_root(input,0,degree);
            instance->travel_map=3;
            int ordinary=hb_map_follower_note_now(instance,pitch);
            instance->travel_map=6;
            assert(hb_map_follower_note_now(instance,pitch)==ordinary);
            if(pitch>60&&!(input&(1u<<mod12(pitch-1))))
                assert(hb_map_follower_note_now(instance,pitch-1)==ordinary-1);
        }
    }
    instance->follower_scale=4;instance->follower_split_map=0;
    const int plain_notes[]={53,50,52,52};
    const char *plain_roles[]={"3rd","5th","3rd","5th"};
    const char *approach_roles[]={"2nd-1","7th-1","4th-1","6th-1"};
    for(int h=0;h<4;h++)for(int chromatic=0;chromatic<2;chromatic++){
        g_bus.observed_harmony=chord(roots[h],minor[h],0);hb_effective_write(g_bus.observed_harmony);
        instance->travel_map=chromatic?6:3;
        instance->published_follower[52]=1;
        instance->mapped[52]=hb_map_follower_note_now(instance,52);
        instance->follower_path_harmony[52]=g_bus.observed_harmony;
        if(!chromatic)assert(instance->mapped[52]==plain_notes[h]);
        API.get_param(instance,"follower_snapshot",view,sizeof(view));
        char label[24];
        API.get_param(instance,"fpath_0_0_1",label,sizeof(label));
        assert(!strcmp(label,chromatic?"4th-1":"3rd*"));
        API.get_param(instance,"fpath_0_0_2",label,sizeof(label));
        assert(!strcmp(label,chromatic?approach_roles[h]:plain_roles[h]));
    }
    instance->follower_scale=0;g_bus.global_transpose=0;
    g_bus.observed_harmony=chord(0,1,0);hb_effective_write(chord(0,0,0));
    assert(hb_follower_input_scale_index(instance,0)==2);
    uint16_t inferred=hb_follower_input_scale(instance,0);
    hb_effective_write(chord(7,0,0));assert(hb_follower_input_scale(instance,0)==inferred);
    g_bus.global_transpose=5;g_bus.observed_harmony=hb_transpose_harmony(chord(0,1,0),5);
    assert(hb_follower_input_scale(instance,0)==inferred);
    assert(instance->follower_scale==0);
    API.destroy_instance(instance);
    puts("input pads: chord-compatible output, scale metadata, split/chromatic distinction, auto without lookahead feedback pass");
    return 0;
}
