/* Snapshot comparison probe: compile against either checkout via -I. */
#define HB_PAD_PREVIEW_TEST 1
#define main original_fixture_main
#include <follower_reference_test.c>
#undef main
int main(void){
    char snapshot[8192];
    for(int scenario=0;scenario<6;scenario++)for(int mode=0;mode<3;mode++)for(int travel=0;travel<10;travel++)for(int trails=0;trails<2;trails++)for(int layout=0;layout<4;layout++){
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
        if(scenario==1||scenario==2){
            instance->motion.lanes[0].operation=HB_MO_TRANSPOSE;
            instance->motion.lanes[0].amount=5;instance->motion.lanes[0].offset=-2;
            instance->motion.lanes[0].pattern=scenario==2?6:0;
            instance->motion.lanes[0].probability=scenario==2?65:100;
            API.set_param(instance,"motion_gesture_1","LatchOn");
        }
        if(scenario==3)API.set_param(instance,"approach_chrom_next","Down");
        if(scenario==4){
            API.set_param(instance,"approach_bank_1","Stock: ii-V-Target");
            API.set_param(instance,"approach_control_1","LatchOn");
        }
        if(scenario==5){
            instance->pad_sounding[60]=1;instance->pad_sounding[67]=2;
            instance->pad_flash_seconds[65]=0.2;instance->trail_valid[64]=1;
            instance->trail_at[64]=0.25;instance->trail_chord_at[64]=1;
            instance->movy_playback=1;instance->movy_input_degree[60]=4;
            instance->movy_input_target[60]=7;
        }
        for(int color=0;color<7;color++){
            g_pad_settings[0]=color;g_pad_next_pulse=color;
#ifdef HB_PAD_PREVIEW_CAN_COMPARE
            char reference[8192];hb_pad_preview_reuse=0;
            assert(API.get_param(instance,"pad_view",reference,sizeof(reference))>0);
            hb_pad_preview_reuse=1;
#endif
            assert(API.get_param(instance,"pad_view",snapshot,sizeof(snapshot))>0);
#ifdef HB_PAD_PREVIEW_CAN_COMPARE
            assert(!strcmp(reference,snapshot));
#endif
            printf("%d %d %d %d %d %d:%s\n",scenario,mode,travel,trails,layout,color,snapshot);
        }
        API.destroy_instance(instance);
    }
    return 0;
}
