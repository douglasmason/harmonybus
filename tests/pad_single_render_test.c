#define main existing_follower_tests
#include "follower_reference_test.c"
#undef main
int main(void){
    Inst *instance=fixture();
    Inst *single=malloc(sizeof(*single)),*expanded=malloc(sizeof(*expanded));
    assert(single&&expanded);
    const int notes[]={24,48,60,61,64,67,84,108,127};
    hb_harmony_t harmony=chord(2,1,0);
    hb_commit_observed_harmony(harmony);
    instance->player.config.mode=0;
    for(int travel=0;travel<8;travel++)for(int trails=0;trails<2;trails++){
        instance->travel_map=travel;instance->trail_enabled=trails;
        for(unsigned index=0;index<sizeof(notes)/sizeof(notes[0]);index++){
            *single=*instance;*expanded=*instance;
            unsigned long long actual_low=0,actual_high=0,expected_low=0,expected_high=0;
            unsigned actual=hb_pad_render_mask(single,instance,harmony,notes[index],1,&actual_low,&actual_high);
            unsigned expected=hb_pad_render_mask(expanded,instance,harmony,notes[index],0,&expected_low,&expected_high);
            assert(actual==expected&&actual_low==expected_low&&actual_high==expected_high);
        }
    }
    free(single);free(expanded);API.destroy_instance(instance);
    puts("Single-voice preview reuse matches ordinary output across travel modes, trails and MIDI range");
}
