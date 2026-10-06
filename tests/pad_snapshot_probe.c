/* Snapshot comparison probe: compile against either checkout via -I. */
#define main original_fixture_main
#include <follower_reference_test.c>
#undef main
int main(void){
    char snapshot[8192];
    for(int mode=0;mode<3;mode++)for(int travel=0;travel<8;travel++)for(int trails=0;trails<2;trails++)for(int layout=0;layout<4;layout++){
        Inst *instance=fixture();instance->player.config.mode=mode;instance->travel_map=travel;instance->trail_enabled=trails;
        instance->chromatic_map=1;instance->preview_count=32;instance->approach_layout=layout>1;
        g_key_context=(hb_key_context){.active=layout&1,.source_root=0,.target_root=9,
            .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(9,2)};
        hb_commit_observed_harmony(chord(2,1,0));
        for(int slot=0;slot<32;slot++){
            instance->preview_notes[slot]=layout==1?60+slot%12:48+slot;
            instance->preview_targets[slot]=layout>1&&slot%2?60+slot/4:-1;
            instance->preview_rows[slot]=layout>1?slot%4:0;
        }
        for(int color=0;color<7;color++){
            g_pad_settings[0]=color;g_pad_next_pulse=color;
            assert(API.get_param(instance,"pad_view",snapshot,sizeof(snapshot))>0);
            printf("%d %d %d %d %d:%s\n",mode,travel,trails,layout,color,snapshot);
        }
        API.destroy_instance(instance);
    }
    return 0;
}
