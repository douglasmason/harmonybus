/* Cached display resolutions must match music and never displace its cache. */
#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main

int main(void){
    Inst *conductor=fixture();API.set_param(conductor,"role","Conductor");
    for(int parent=1;parent<=19;parent++)for(int transpose=-2;transpose<=2;transpose++){
        hb_set_shared_follower_scale(parent);g_bus.global_transpose=transpose;
        for(int key_active=0;key_active<2;key_active++){
            g_key_context=(hb_key_context){.active=key_active,.source_root=mod12(transpose),.target_root=9,
                .source_mask=hb_explicit_scale_mask(mod12(transpose),parent),.target_mask=hb_explicit_scale_mask(9,2)};
            for(int index=0;index<21;index++){
                int fields[]={0,0,0,index>=14?1:3,0,1,12,index<7?1:index<14?12:13,index%7},note=0;
                unsigned long long word=hb_selection_resolve(conductor,fields,&note);
                hb_harmony_t expected=hb_key_harmony(conductor,hb_selection_decode(word,note));
                hb_key_harmony_cache before[4];memcpy(before,conductor->key_cache,sizeof(before));
                unsigned cursor=conductor->key_cache_cursor;
                for(int repeat=0;repeat<2;repeat++){
                    hb_harmony_t actual=hb_selection_preview(conductor,index,word,note);
                    assert(!memcmp(&expected,&actual,sizeof(expected)));
                    assert(cursor==conductor->key_cache_cursor);
                    assert(!memcmp(before,conductor->key_cache,sizeof(before)));
                }
            }
        }
    }
    /* Same slot/word with changed policy, register and spelling is not stale. */
    for(int policy=0;policy<=10;policy++)for(int octave=-3;octave<=3;octave++){
        conductor->chord_register=octave;
        conductor->policy_overrides|=(1u<<HB_P_DOMINANT)|(1u<<HB_P_DOMINANT_MINOR);
        conductor->policy_values[HB_P_DOMINANT]=conductor->policy_values[HB_P_DOMINANT_MINOR]=policy;
        int fields[]={0,0,3,3,0,1,12,2,4},note=0;
        unsigned long long word=hb_selection_resolve(conductor,fields,&note);
        hb_harmony_t expected=hb_key_harmony(conductor,hb_selection_decode(word,note));
        hb_harmony_t actual=hb_selection_preview(conductor,0,word,note);
        assert(!memcmp(&expected,&actual,sizeof(expected)));
    }
    API.destroy_instance(conductor);
    puts("Chord preview: cold/warm parity, key/scale/transpose/policy/range changes and musical cache isolation pass");
}
