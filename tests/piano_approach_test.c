#define main reference_suite_main
#include "follower_reference_test.c"
#undef main
static void event(Inst *i,int pitch,int on){
    uint8_t m[3]={(uint8_t)(on?0x90:0x80),(uint8_t)pitch,(uint8_t)(on?100:0)},o[64][3];int lens[64];
    API.process_midi(i,m,3,o,lens,64);
}
int main(void){
    Inst *i=fixture();i->travel_map=6;i->content_map=1;g_bus.boundary_buffer_ms=0;
    char param[80],view[1024];
    for(int scale=1;scale<=15;scale++)for(int root=0;root<12;root++){
        hb_set_shared_follower_scale(scale);hb_effective_write(chord(root,root&1,1));
        for(int target=0;target<128;target++){
            int alias=target<64?target+36:target-36,shift=target-alias;
            memset(i->movy_pad_shift,0,sizeof(i->movy_pad_shift));
            int resolution=hb_map_follower_note_now(i,target);int expected=resolution-1;if(expected<0)expected+=12;
            i->movy_pad_shift[alias]=shift;
            assert(hb_map_follower_note_now(i,alias)==expected);
            assert(hb_map_follower_note_now(i,target)==resolution);
        }
    }
    memset(i->movy_pad_shift,0,sizeof(i->movy_pad_shift));hb_set_shared_follower_scale(1);hb_effective_write(chord(5,0,0));
    int target=65,alias=29,normal=hb_map_follower_note_now(i,target);
    snprintf(param,sizeof(param),"%d,%d",alias,target-alias);API.set_param(i,"hb_movy_input_approach",param);
    event(i,alias,1);assert(i->movy_pad_shift[alias]==36);assert(hb_map_follower_note_now(i,alias)==normal-1);
    event(i,target,1);assert(!i->movy_pad_shift[target]);assert(hb_map_follower_note_now(i,target)==normal);
    event(i,alias,0);assert(i->held_now[target]||i->follower_held[target]);event(i,target,0);
    int recorded=0;for(int slot=0;slot<i->action_count;slot++){
        int at=(i->action_head+slot)%64;
        if(i->action_pitch[at]==alias){assert((i->action_queue[at][HB_MOTION_LANES]>>2)==2);recorded=1;}
    }assert(recorded);
    event(i,alias,1);assert(!i->movy_pad_shift[alias]);event(i,alias,0);
    i->recorded_action_valid[alias]=1;i->recorded_actions[alias][HB_MOTION_LANES]=8;i->movy_playback=1;
    event(i,alias,1);assert(i->movy_pad_shift[alias]==36);assert(hb_map_follower_note_now(i,alias)==normal-1);event(i,alias,0);i->movy_playback=0;
    for(int mode=0;mode<8;mode++){
        memset(i->movy_pad_shift,0,sizeof(i->movy_pad_shift));i->travel_map=mode;API.get_param(i,"pad_view",view,sizeof(view));assert(strstr(view,mode==6?"|piano1,1":"|piano1,0"));
        API.set_param(i,"hb_movy_input_approach",param);event(i,alias,1);assert((i->movy_pad_shift[alias]!=0)==(mode==6));event(i,alias,0);
    }
    i->role=3;API.set_param(i,"hb_movy_input_approach",param);event(i,alias,1);assert(!i->movy_pad_pending);
    i->role=1;i->travel_map=6;API.set_param(i,"hb_movy_input_approach",param);event(i,alias,1);
    hb_clear_instance_note_state(i);assert(!i->movy_pad_pending&&!i->movy_pad_shift[alias]);
    puts("piano approaches: all scales and MIDI targets, exact mapped resolution, independent ownership, recording and travel gating pass");
}
