#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main
static int last_output=-1;
static void input(Inst *instance,int source,int on){uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)source,(uint8_t)(on?100:0)},output[64][3];int lengths[64];API.process_midi(instance,message,3,output,lengths,64);position+=.02;int count=API.tick(instance,1024,48000,output,lengths,64);for(int index=0;index<count;index++)if((output[index][0]&0xf0)==0x90&&output[index][2])last_output=output[index][1];}
static void upper(Inst *instance){API.set_param(instance,"hb_movy_input_approach","96,-36");input(instance,96,1);}
int main(void){
    Inst *instance=fixture();instance->travel_map=0;instance->content_map=1;instance->chromatic_map=1;instance->boundary_buffer_ms=0;instance->next_anti_buffer_ms=0;
    hb_set_shared_follower_scale(1);g_bus.observed_harmony=chord(0,0,0);hb_effective_write(g_bus.observed_harmony);
    char label[64];
    API.set_param(instance,"approach_knob_8","Chromatic Below");
    API.get_param(instance,"approach_knob_8",label,sizeof(label));assert(!strcmp(label,"Secondary LT"));
    assert(instance->approach_rows.knobs[7]==0);
    API.set_param(instance,"approach_knob_8","Scale Above");
    API.get_param(instance,"approach_knob_8",label,sizeof(label));assert(!strcmp(label,"Secondary II"));
    assert(instance->approach_rows.knobs[7]==2); /* Legacy input keeps its serialized identity. */
    API.set_param(instance,"approach_knob_8","Secondary II");assert(instance->approach_rows.knobs[7]==3);
    API.set_param(instance,"motion_operation","Chrom Below");
    API.get_param(instance,"motion_operation",label,sizeof(label));assert(!strcmp(label,"Secondary LT"));
    API.set_param(instance,"motion_operation","Scale Above");
    API.get_param(instance,"motion_operation",label,sizeof(label));assert(!strcmp(label,"Secondary II"));
    assert(instance->motion.lanes[0].operation==HB_MO_ABOVE);
    API.set_param(instance,"motion_operation","Secondary II");assert(instance->motion.lanes[0].operation==HB_MO_SECONDARY_II);
    API.set_param(instance,"motion_operation","Off");
    unsigned minor=hb_explicit_scale_mask(0,2);
    hb_cp_config config=instance->player.config;config.chromatic_quality=1;
    hb_approach_result lt=hb_resolve_chord_approach(instance,60,minor,config,chord(0,1,0),0,0,-1,0);
    hb_approach_result seventh=hb_resolve_chord_approach(instance,60,minor,config,chord(0,1,0),10,0,0,0);
    assert(lt.root==59&&lt.config.quality==5);assert(seventh.root==58);
    config.chromatic_quality=3;
    lt=hb_resolve_chord_approach(instance,60,minor,config,chord(0,1,0),0,0,-1,0);
    assert(lt.root==59&&lt.config.quality==9);
    API.set_param(instance,"approach_mode_active","1");
    API.set_param(instance,"approach_knob_1","Secondary V");API.set_param(instance,"approach_knob_2","Chromatic Above");
    API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_2","Down");
    API.set_param(instance,"approach_touch_1","Up");API.set_param(instance,"approach_touch_2","Up");
    assert(instance->approach_rows.count==2&&instance->approach_rows.order[0]==5&&instance->approach_rows.order[1]==2);
    Inst *preview=malloc(sizeof(*preview));assert(preview);*preview=*instance;preview->movy_pad_shift[96]=-36;
    unsigned before=hb_ar_peek(&instance->approach_rows);
    assert(hb_pad_render_mask(preview,instance,hb_render_harmony(instance),96,1,0,0)==(1u<<7));
    assert(hb_ar_peek(&instance->approach_rows)==before);free(preview);
    upper(instance);assert(instance->approach_rows.tokens[96]==5);assert(instance->mapped[96]==55);input(instance,96,0);
    upper(instance);assert(instance->approach_rows.tokens[96]==2);assert(last_output==61);input(instance,96,0);
    input(instance,60,1);assert(instance->mapped[60]==60);input(instance,60,0);
    assert(instance->motion.lanes[0].operation!=HB_MO_MOTIF);
    unsigned long long saved[HB_MOTION_LANES+1];memcpy(saved,instance->action_queue[0],sizeof(saved));assert(((saved[HB_MOTION_LANES]>>43)&2047)==5);
    instance->movy_playback=1;memcpy(instance->recorded_actions[96],saved,sizeof(saved));instance->recorded_action_valid[96]=1;
    input(instance,96,1);assert(instance->mapped[96]==55);input(instance,96,0);instance->movy_playback=0;
    API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Down");assert(instance->approach_rows.count==1);API.set_param(instance,"approach_touch_1","Up");
    API.set_param(instance,"approach_knob_1","Motif 1");API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Up");
    upper(instance);assert(instance->motif.pending>0);assert(instance->approach_rows.event==1);input(instance,96,0);
    upper(instance);assert(instance->approach_rows.event==0);input(instance,96,0);
    int actions_before=instance->action_count;
    upper(instance);input(instance,96,0);assert(instance->action_count==actions_before+1);
    unsigned long long motif_actions[HB_MOTION_LANES+1];memcpy(motif_actions,instance->action_queue[actions_before],sizeof(motif_actions));
    assert(((motif_actions[HB_MOTION_LANES]>>43)&63)==16);
    instance->movy_playback=1;memcpy(instance->recorded_actions[96],motif_actions,sizeof(motif_actions));
    input(instance,96,1);input(instance,96,0);instance->movy_playback=0;
    API.set_param(instance,"approach_trigger","2");input(instance,60,1);assert(instance->approach_rows.bank_armed==-1);input(instance,60,0);
    char state[65536];int used=API.get_param(instance,"state",state,sizeof(state));assert(used>0&&used<sizeof(state)&&strstr(state,";ar1,"));
    Inst *restored=API.create_instance("",0);API.set_param(restored,"state",state);assert(restored->approach_rows.knobs[0]==13&&restored->approach_rows.count==1);assert(!restored->approach_rows.down&&!restored->approach_rows.enabled);
    API.destroy_instance(restored);API.destroy_instance(instance);
    for(int mode=1;mode<=2;mode++){
        instance=fixture();instance->travel_map=0;instance->chromatic_map=1;instance->boundary_buffer_ms=0;instance->next_anti_buffer_ms=0;
        hb_set_shared_follower_scale(1);g_bus.observed_harmony=chord(0,0,0);hb_effective_write(g_bus.observed_harmony);
        instance->player.config.mode=mode;instance->player.config.size=2;instance->player.config.playback=1;
        API.set_param(instance,"approach_mode_active","1");API.set_param(instance,"approach_knob_1","Secondary V");
        API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Up");
        upper(instance);int found=0;for(int voice=0;voice<HB_CP_KEYS;voice++)if(instance->player.keys[voice].used){assert(instance->player.keys[voice].root_pc==7);found++;}assert(found);
        input(instance,96,0);
        g_bus.clip_loop_end=4;g_bus.next_model_locked=1;g_bus.next_model_count=2;position=.25;
        g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
        g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=chord(2,1,0)};
        instance->motion.lanes[0].operation=HB_MO_HARMONY;instance->motion.lanes[0].amount=100;instance->motion.lanes[0].enabled=0;
        API.set_param(instance,"motion_gesture_1","LatchOn");
        upper(instance);found=0;for(int voice=0;voice<HB_CP_KEYS;voice++)if(instance->player.keys[voice].used){assert(instance->player.keys[voice].root_pc==9);found++;}assert(found);input(instance,96,0);
        API.destroy_instance(instance);
    }
    puts("Approach rows: ordered touches, transformations, isolated lower row, motif steps, direct bank, recording and persistence pass");
}
