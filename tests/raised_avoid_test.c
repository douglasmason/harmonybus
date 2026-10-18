/* Production content, degree travel and state compatibility regressions. */
#define main reference_tests_main
#include "follower_reference_test.c"
#undef main

int main(void){
    Inst *instance=fixture();
    API.set_param(instance,"follower_scale","Major");
    API.set_param(instance,"dominant_scale","Off");
    API.set_param(instance,"dominant_minor_scale","None");
    API.set_param(instance,"content_map","Raised-Avoid");
    assert(instance->content_map==9);
    const int roots[]={2,4,7};
    const int minor[]={1,1,0};
    const int scale_roots[]={2,4,7};
    const int scale_ids[]={3,3,12}; /* Dorian, Dorian, Lydian dominant */
    for(int index=0;index<3;index++){
        hb_harmony_t harmony=chord(roots[index],minor[index],1);
        uint16_t expected=hb_explicit_scale_mask(scale_roots[index],scale_ids[index]);
        hb_harmony_t target=hb_follower_content_target(instance,harmony,9);
        if(target.pitch_mask!=expected){
            fprintf(stderr,"root %d: %x != %x\n",roots[index],target.pitch_mask,expected);
            abort();
        }
        assert((target.pitch_mask&hb_harmony_chord_mask(harmony))==hb_harmony_chord_mask(harmony));
        hb_effective_write(harmony);
        for(int travel=0;travel<5;travel++)for(int split=0;split<4;split++){
            instance->travel_map=travel;instance->follower_split_map=split;
            for(int pitch=60;pitch<72;pitch++){
                int mapped=hb_map_follower_note_now(instance,pitch);
                assert(expected&(1u<<mod12(mapped)));
            }
        }
        instance->travel_map=0;
        char preview[32768];unsigned current,effective,scale;
        hb_commit_observed_harmony(harmony);
        API.get_param(instance,"pad_view",preview,sizeof(preview));
        assert(sscanf(preview,"%u,%u,%u",&current,&effective,&scale)==3);
        assert(scale==0xfff); /* Every Relative pad maps into the raised scale. */
    }
    hb_harmony_t harmony=chord(4,1,1);
    hb_effective_write(harmony);
    instance->travel_map=0;
    assert(hb_map_follower_note_now(instance,62)==66); /* 9 -> F#, not E */
    assert(hb_apply_approach(instance,66,HB_APPROACH_CHROM_BELOW)==65);
    assert(hb_map_follower_note_now(instance,69)==73); /* 13 -> C# */
    assert(hb_follower_content_target(instance,harmony,7).pitch_mask==
        ((1u<<4)|(1u<<7)|(1u<<9)|(1u<<11)|(1u<<2)));
    /* Explicit chord tones, including a requested b9, cannot be raised. */
    uint16_t chord_mask=(1u<<0)|(1u<<1)|(1u<<4)|(1u<<7)|(1u<<10);
    assert(!hb_content_is_avoid(chord_mask,1));
    assert(hb_content_is_avoid(chord_mask,5));
    const char *labels[]={"Chord","Scale","Free","135","1357","12357","12356","Non-Avoid","123567","Raised-Avoid"};
    for(int index=0;index<10;index++){
        char alias[32],state[65536],value[32];
        snprintf(alias,sizeof(alias),"In %s",labels[index]);
        API.set_param(instance,"content_map",alias);
        assert(instance->content_map==index);
        API.get_param(instance,"state",state,sizeof(state));
        API.set_param(instance,"content_map","Chord");
        API.set_param(instance,"state",state);
        API.get_param(instance,"content_map",value,sizeof(value));
        assert(instance->content_map==index&&!strcmp(value,labels[index]));
    }
    API.destroy_instance(instance);
    puts("Raised avoid collections, relative degrees, chord protection and saved IDs pass");
}
