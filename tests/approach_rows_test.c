#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main
static int input(Inst *i,int source,int on){uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)source,(uint8_t)(on?100:0)},output[64][3];int lengths[64],note=-1;int count=API.process_midi(i,message,3,output,lengths,64);for(int n=0;n<count;n++)if(lengths[n]==3&&(output[n][0]&0xf0)==0x90&&output[n][2])note=output[n][1];count=API.tick(i,64,48000,output,lengths,64);for(int n=0;n<count;n++)if(lengths[n]==3&&(output[n][0]&0xf0)==0x90&&output[n][2])note=output[n][1];return note;}
static Inst *setup(void){Inst *i=fixture();i->travel_map=0;i->content_map=1;i->chromatic_map=1;i->boundary_buffer_ms=0;i->next_anti_buffer_ms=0;hb_set_shared_follower_scale(1);g_bus.observed_harmony=chord(0,0,0);hb_effective_write(g_bus.observed_harmony);return i;}
static void touch(Inst *i,const char *key){API.set_param(i,key,"Down");API.set_param(i,key,"Up,50");}
int main(void){
    Inst *i=setup();char text[65536];
    API.get_param(i,"approach_bank_1",text,sizeof(text));assert(!strcmp(text,"Stock: V-Target"));
    API.set_param(i,"approach_bank_1","Secondary LT");API.set_param(i,"approach_bank_2","Secondary II");API.set_param(i,"approach_bank_3","Secondary V");
    API.set_param(i,"approach_mode_active","1");
    /* Non-overlapping, nonadjacent controls all enter the same FIFO. */
    touch(i,"approach_touch_1");touch(i,"approach_step_touch_2");touch(i,"approach_touch_3");
    assert(i->approach_rows.row_slots[2]==0&&i->approach_rows.row_slots[1]==1&&i->approach_rows.row_slots[0]==2);
    assert(!i->approach_rows.down);API.set_param(i,"approach_mode_active","0");
    const int expected[]={55,62,59};
    for(int row=2;row>=0;row--){int alias=(60+32*(row+1))%128;char param[64];snprintf(param,sizeof(param),"%d,%d,%d",alias,60-alias,row);Inst *preview=malloc(sizeof(*preview));assert(preview);*preview=*i;preview->movy_pad_shift[alias]=60-alias;preview->approach_rows.preview_row=row;assert(hb_pad_render_mask(preview,i,hb_render_harmony(i),alias,1,0,0)==(1u<<mod12(expected[row])));free(preview);API.set_param(i,"hb_movy_input_approach",param);assert(input(i,alias,1)==expected[row]);input(i,alias,0);}
    /* A fourth touch evicts only the oldest, leaving chronological top-down order. */
    touch(i,"approach_step_touch_16");assert(i->approach_rows.row_slots[2]==1&&i->approach_rows.row_slots[1]==2&&i->approach_rows.row_slots[0]==15);
    API.get_param(i,"approach_rows_view",text,sizeof(text));assert(strstr(text,"3,Secondary V")&&strstr(text,"2,Secondary II"));
    /* A motif on each row has its own cursor; singleton rows repeat the operation. */
    API.set_param(i,"approach_bank_1","Stock: vi-ii-V");touch(i,"approach_touch_1");touch(i,"approach_touch_1");
    unsigned first=hb_ar_row_peek(&i->approach_rows,0);assert(first==51);hb_ar_row_advance(&i->approach_rows,0);assert(hb_ar_row_peek(&i->approach_rows,0)==(51|(1<<6)));assert(hb_ar_row_peek(&i->approach_rows,1)==51);
    hb_ar_row_advance(&i->approach_rows,3);assert(hb_ar_row_peek(&i->approach_rows,3)==(51|(1<<6)));
    /* Save assignments, never physical holds or momentary/latch state. */
    API.get_param(i,"state",text,sizeof(text));assert(strstr(text,";ar3,0,0,15"));Inst *restored=API.create_instance("",0);API.set_param(restored,"state",text);assert(!memcmp(restored->approach_rows.row_slots,i->approach_rows.row_slots,sizeof(i->approach_rows.row_slots)));assert(!restored->approach_rows.performance&&!restored->approach_rows.down);API.destroy_instance(restored);
    API.set_param(i,"approach_mode_active","1");API.set_param(i,"approach_touch_1","Down");API.set_param(i,"approach_step_touch_1","Down");API.set_param(i,"approach_touch_1","Up,500");assert(i->approach_rows.down==1);API.set_param(i,"approach_step_touch_1","Up,500");assert(!i->approach_rows.down&&!i->approach_rows.performance);
    /* One-shot performance remains independent of the persistent row FIFO. */
    touch(i,"approach_touch_1");assert(i->approach_rows.performance);for(int n=0;n<3;n++){input(i,60,1);input(i,60,0);}assert(!i->approach_rows.performance);
    for(int target=0;target<128;target++)for(int row=0;row<3;row++){int identity=(target+32*(row+1))%128,shift=target-identity;unsigned long long word=hb_ar_alias_word(shift);assert(hb_ar_alias_valid(word)&&hb_ar_alias_shift(word)==shift);}
    API.set_param(i,"approach_touch_1","Down");API.set_param(i,"approach_latch","On");API.set_param(i,"approach_touch_1","Up,500");assert(i->approach_rows.performance);API.set_param(i,"approach_latch","Off");assert(!i->approach_rows.performance);
    API.destroy_instance(i);
    for(int mode=1;mode<=2;mode++){
        i=setup();i->player.config.mode=mode;i->player.config.size=2;API.set_param(i,"approach_bank_1","Stock: vi-ii-V");touch(i,"approach_touch_1");i->approach_rows.row_steps[0]=2;
        g_bus.clip_loop_end=4;g_bus.next_model_locked=1;g_bus.next_model_count=2;position=.25;
        g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
        g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=chord(2,1,0)};
        i->motion.lanes[0].operation=HB_MO_HARMONY;i->motion.lanes[0].amount=100;i->motion.lanes[0].enabled=0;API.set_param(i,"motion_gesture_1","LatchOn");
        API.set_param(i,"hb_movy_input_approach","92,-32,0");uint8_t message[3]={0x90,92,100},output[64][3];int lengths[64];API.process_midi(i,message,3,output,lengths,64);int found=0;for(int n=0;n<HB_MT_SCHEDULE;n++)if(i->motif.events[n].used&&i->motif.events[n].pitch==57)found=1;assert(found);input(i,92,0);API.destroy_instance(i);
    }
    puts("Approach FIFO: released touches, knobs/steps, top-down 3-2-1 rendering, eviction, independent motif cursors, persistence, ownership and one-shot pass");
}
