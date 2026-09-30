#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main
static void input(Inst *instance,int source,int on){uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)source,(uint8_t)(on?100:0)},output[64][3];int lengths[64];API.process_midi(instance,message,3,output,lengths,64);}
static Inst *setup(void){Inst *instance=fixture();instance->travel_map=0;instance->content_map=1;instance->chromatic_map=1;instance->boundary_buffer_ms=0;instance->next_anti_buffer_ms=0;hb_set_shared_follower_scale(1);g_bus.observed_harmony=chord(0,0,0);hb_effective_write(g_bus.observed_harmony);return instance;}
static int schedule_has(Inst *instance,int pitch){for(int index=0;index<HB_MT_SCHEDULE;index++)if(instance->motif.events[index].used&&instance->motif.events[index].pitch==pitch)return 1;return 0;}
int main(void){
    Inst *instance=setup();char text[65536];
    API.get_param(instance,"approach_bank_1",text,sizeof(text));assert(!strcmp(text,"Stock: vi-ii-V"));
    API.set_param(instance,"approach_mode_active","1");
    API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Up,50");
    assert(instance->approach_rows.performance);
    const int expected[]={57,62,55};
    for(int step=0;step<3;step++){input(instance,60,1);assert(schedule_has(instance,expected[step]));assert(((instance->action_queue[step][HB_MOTION_LANES]>>43)&2047)==(51u|((unsigned)step<<6)));input(instance,60,0);}
    assert(!instance->approach_rows.performance);input(instance,60,1);assert(!instance->approach_rows.tokens[60]);input(instance,60,0);
    API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_2","Down");assert(instance->approach_rows.row_preset==1&&instance->approach_rows.count==2);API.set_param(instance,"approach_touch_1","Up,500");API.set_param(instance,"approach_touch_2","Up,500");assert(!instance->approach_rows.performance&&instance->approach_rows.row_preset==1);
    API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Up,500");
    API.set_param(instance,"approach_mode_active","0"); /* Row selection survives bank changes. */
    memset(instance->motif.events,0,sizeof(instance->motif.events));instance->motif.pending=0;
    for(int row=0;row<3;row++){
        int identity=(60+32*(row+1))%128,shift=60-identity;char parameter[64];snprintf(parameter,sizeof(parameter),"%d,%d,%d",identity,shift,row);API.set_param(instance,"hb_movy_input_approach",parameter);input(instance,identity,1);assert(schedule_has(instance,expected[row]));assert(instance->approach_rows.swallow[0][identity]==1);
        unsigned long long word=instance->action_queue[instance->action_count-1][HB_MOTION_LANES];assert(hb_ar_alias_shift(word)==shift&&hb_ar_alias_valid(word));assert(((word>>43)&2047)==(51u|((unsigned)row<<6)));
        Inst *preview=malloc(sizeof(*preview));assert(preview);*preview=*instance;preview->movy_pad_shift[identity]=shift;preview->approach_rows.preview_row=row;assert(hb_pad_render_mask(preview,instance,hb_render_harmony(instance),identity,1,0,0)==(1u<<mod12(expected[row])));free(preview);
    }
    for(int row=0;row<3;row++){int identity=(60+32*(row+1))%128;input(instance,identity,0);assert(!instance->approach_rows.swallow[0][identity]);}
    for(int target=0;target<128;target++)for(int row=0;row<3;row++){int identity=(target+32*(row+1))%128,shift=target-identity;assert(hb_ar_alias_valid(hb_ar_alias_word(shift)));assert(hb_ar_alias_shift(hb_ar_alias_word(shift))==shift);}
    assert(!hb_ar_alias_valid(12));assert(!hb_ar_alias_valid(1ULL<<55));
    API.set_param(instance,"approach_bank_1","Secondary LT");assert(instance->approach_rows.bank[0]==-1);assert(hb_ar_row_peek(&instance->approach_rows,0)==1&&hb_ar_row_peek(&instance->approach_rows,1)==15);
    API.set_param(instance,"approach_bank_1","Stock: vi-ii-V");
    int used=API.get_param(instance,"state",text,sizeof(text));assert(used>0&&strstr(text,";ar2,0"));Inst *restored=API.create_instance("",0);API.set_param(restored,"state",text);assert(restored->approach_rows.bank[0]==36&&restored->approach_rows.row_preset==0&&!restored->approach_rows.performance);API.destroy_instance(restored);
    API.set_param(instance,"approach_mode_active","1");API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_latch","On");API.set_param(instance,"approach_touch_1","Up,500");assert(instance->approach_rows.performance);API.set_param(instance,"approach_latch","Off");assert(!instance->approach_rows.performance);
    API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_step_touch_1","Down");API.set_param(instance,"approach_touch_1","Up,500");assert(instance->approach_rows.down==1);API.set_param(instance,"approach_step_touch_1","Up,500");assert(!instance->approach_rows.down);
    API.destroy_instance(instance);
    for(int mode=1;mode<=2;mode++){
        instance=setup();instance->player.config.mode=mode;instance->player.config.size=2;
        g_bus.clip_loop_end=4;g_bus.next_model_locked=1;g_bus.next_model_count=2;position=.25;
        g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
        g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=chord(2,1,0)};
        instance->motion.lanes[0].operation=HB_MO_HARMONY;instance->motion.lanes[0].amount=100;instance->motion.lanes[0].enabled=0;API.set_param(instance,"motion_gesture_1","LatchOn");
        API.set_param(instance,"hb_movy_input_approach","28,32,2");input(instance,28,1);assert(schedule_has(instance,57));input(instance,28,0);API.destroy_instance(instance);
    }
    puts("Shared bank: all-layout performance, independent row selection, three spatial motif steps, alias ownership, previews, latch and persistence pass");
}
