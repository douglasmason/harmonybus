#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main
int main(void){
    Inst *instance=fixture();char buffer[65536];
    hb_harmony_t harmony=chord(0,0,0);
    for(int connector=0;connector<7;connector++){
        hb_cp_config config=instance->player.config;config.mode=1;config.chromatic_quality=connector;
        for(int role=12;role<=13;role++){
            instance->motion.render_flags=0;
            hb_approach_result result=hb_resolve_chord_approach(instance,60,0xAB5,config,harmony,role,0,0,0);
            assert(result.root==(role==12?59:61)&&result.config.quality==9);
            for(int size=2;size<=3;size++){
                int voices[HB_CP_VOICES];result.config.size=size;
                int count=hb_cp_voice(result.config,result.root,0,0x91,0xAB5,voices);
                unsigned mask=0;for(int voice=0;voice<count;voice++)mask|=1u<<mod12(voices[voice]-result.root);
                assert(mask==(size==2?0x49u:0x249u));
            }
            assert(hb_relative_approach_offset(role,60,0xAB5)==(role==12?-1:1));
        }
        instance->motion.render_flags=HB_MO_CONNECTOR_ABOVE;
        hb_approach_result above=hb_resolve_chord_approach(instance,60,0xAB5,config,harmony,0,0,2,0);
        assert(above.root==61&&above.config.quality==(connector==6?hb_cp_auto_leading_quality(60,0xAB5):hb_cp_chromatic_quality(connector)));
        instance->motion.render_flags=0;
        hb_approach_result sub=hb_resolve_chord_approach(instance,60,0xAB5,config,harmony,0,0,2,0);
        assert(sub.root==61&&sub.config.quality==6);
    }
    for(int operation=HB_MO_LEADING_TONE;operation<=HB_MO_UPPER_DIM;operation++){
        hb_motion_config motion;hb_mo_defaults(&motion);motion.lanes[0].operation=operation;
        hb_mo_set(&motion,"motion_gesture_1","Knob,0");hb_mo_set(&motion,"motion_gesture_1","Up,50");
        hb_mo_input(&motion,60,0,0);assert(hb_mo_source_secondary(&motion)==(operation==HB_MO_LEADING_TONE?12:13));
    }
    API.set_param(instance,"motion_operation","Secondary LT");assert(instance->motion.lanes[0].operation==HB_MO_BELOW);
    API.set_param(instance,"motion_operation","LT");assert(instance->motion.lanes[0].operation==HB_MO_LEADING_TONE);
    API.set_param(instance,"approach_bank_1","Leading Tone");API.set_param(instance,"approach_bank_2","Upper Dim");
    assert(hb_ar_code(&instance->approach_rows,0)==14&&hb_ar_code(&instance->approach_rows,1)==60);
    assert(hb_ar_intent(14)==hb_mo_role_word(12)&&hb_ar_intent(60)==hb_mo_role_word(13));
    for(int operation=HB_MO_LEADING_TONE;operation<=HB_MO_UPPER_DIM;operation++){
        unsigned long long words[HB_MOTION_LANES+1]={0};
        words[0]=HB_MO_RECORDED|hb_mo_operation_word(operation);
        words[HB_MOTION_LANES]=hb_mo_role_word(operation==HB_MO_LEADING_TONE?12:13)|((unsigned long long)(operation==HB_MO_LEADING_TONE?14:60)<<HB_AR_SHIFT);
        int used=snprintf(buffer,sizeof(buffer),"60");
        for(int lane=0;lane<=HB_MOTION_LANES;lane++)used+=snprintf(buffer+used,sizeof(buffer)-used,",%llu",words[lane]);
        instance->recorded_action_valid[60]=0;API.set_param(instance,"hb_movy_actions",buffer);
        assert(instance->recorded_action_valid[60]);
        assert(!memcmp(instance->recorded_actions[60],words,sizeof(words)));
    }
    API.get_param(instance,"state",buffer,sizeof(buffer));Inst *restored=API.create_instance("",0);API.set_param(restored,"state",buffer);
    assert(restored->motion.lanes[0].operation==HB_MO_LEADING_TONE);
    assert(hb_ar_code(&restored->approach_rows,0)==14&&hb_ar_code(&restored->approach_rows,1)==60);
    API.destroy_instance(restored);API.destroy_instance(instance);
    puts("Connector harmony: configurable connectors, fixed diminished triads/sevenths, fixed tritone dominant, gestures, aliases and persistence pass");
}
