#define main reference_suite_main
#include "follower_reference_test.c"
#undef main
static int event(Inst *i,int pitch,int on){
    uint8_t m[3]={(uint8_t)(on?0x90:0x80),(uint8_t)pitch,(uint8_t)(on?100:0)},o[64][3];int lens[64],rendered=-1;
    int count=API.process_midi(i,m,3,o,lens,64);
    for(int n=0;n<count;n++)if(lens[n]==3&&(o[n][0]&0xf0)==0x90&&o[n][2])rendered=o[n][1];
    count=API.tick(i,64,48000,o,lens,64);
    for(int n=0;n<count;n++)if(lens[n]==3&&(o[n][0]&0xf0)==0x90&&o[n][2])rendered=o[n][1];
    return rendered;
}

static void approach_pad_membership(void){
    Inst *instance=fixture();API.set_param(instance,"approach_bank_1","Connector Below");instance->chromatic_map=1;instance->content_map=1;
    instance->boundary_buffer_ms=0;instance->next_anti_buffer_ms=0;
    instance->next_lookahead=0;instance->approach_control=HB_APPROACH_OFF;
    hb_harmony_t current=chord(0,0,0),next=chord(2,1,0);
    g_bus.clip_loop_end=4;g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=current};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=next};
    g_bus.observed_harmony=current;hb_effective_write(current);position=1;
    char request[160]="pad_view@",view[4096],label[40];
    for(int slot=0;slot<32;slot++)sprintf(request+9+slot*2,"ff");
    request[73]=':';
    for(int slot=0;slot<32;slot++)sprintf(request+74+slot*2,"%02x",slot<12?61+slot:0);
    API.set_param(instance,"pad_preview_inputs",request+9);
    int scales[]={1,4,15},travels[]={0,1,2,3,4,5,7};
    for(int scale=0;scale<3;scale++)for(int travel=0;travel<7;travel++){
        hb_set_shared_follower_scale(scales[scale]);instance->travel_map=travels[travel];
        Inst before=*instance;
        assert(API.get_param(instance,"pad_view",view,sizeof(view))>0);
        before.closest_assignments=instance->closest_assignments; /* Memoization only. */
        assert(!memcmp(&before,instance,sizeof(before)));
        const char *cursor=strstr(view,"|gapcolors1");assert(cursor);cursor+=11;
        for(int slot=0;slot<32;slot++){
            int flags;assert(sscanf(cursor,",%d",&flags)==1);cursor=strchr(cursor+1,',');
            if(slot>=12){assert(flags==-1);continue;}
            int target=60+slot,identity=target<64?target+36:target-36;
            instance->movy_pad_shift[identity]=target-identity;
            instance->render_harmony_active=1;instance->render_harmony=current;
            int current_pitch=hb_map_follower_note_now(instance,identity);
            hb_harmony_t scale_target=hb_follower_scale_target(instance,current);
            instance->render_harmony=next;
            int next_pitch=hb_map_follower_note_now(instance,identity);
            instance->render_harmony_active=0;instance->movy_pad_shift[identity]=0;
            unsigned current_bit=1u<<mod12(current_pitch),next_bit=1u<<mod12(next_pitch);
            assert(!!(flags&1)==!!(current_bit&hb_harmony_chord_mask(current)));
            assert(!!(flags&2)==!!(current_bit&hb_harmony_chord_mask(current)));
            assert(!!(flags&4)==!!(current_bit&scale_target.pitch_mask));
            assert(!!(flags&16)==!!(next_bit&hb_harmony_chord_mask(next)));
        }
    }
    API.set_param(instance,"travel_map","Direct");
    API.get_param(instance,"travel_map",label,sizeof(label));assert(!strcmp(label,"None"));
    char state[8192];API.get_param(instance,"state",state,sizeof(state));
    Inst *restored=API.create_instance("",0);API.set_param(restored,"state",state);
    API.get_param(restored,"travel_map",label,sizeof(label));assert(!strcmp(label,"None"));
    API.destroy_instance(restored);API.destroy_instance(instance);
}

int main(void){
    approach_pad_membership();
    Inst *i=fixture();API.set_param(i,"approach_bank_1","Connector Below");i->travel_map=6;i->content_map=1;i->boundary_buffer_ms=0;i->next_anti_buffer_ms=0;g_bus.boundary_buffer_ms=0;
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
    assert(event(i,alias,1)==normal-1);assert(i->movy_pad_shift[alias]==36);
    event(i,target,1);assert(!i->movy_pad_shift[target]);assert(hb_map_follower_note_now(i,target)==normal);
    event(i,alias,0);assert(i->held_now[target]||i->follower_held[target]);event(i,target,0);
    int recorded=0;for(int slot=0;slot<i->action_count;slot++){
        int at=(i->action_head+slot)%64;
        if(i->action_pitch[at]==alias){assert(((i->action_queue[at][HB_MOTION_LANES]>>2)&3)==2);recorded=1;}
    }assert(recorded);
    event(i,alias,1);assert(!i->movy_pad_shift[alias]);event(i,alias,0);
    i->recorded_action_valid[alias]=1;i->recorded_actions[alias][HB_MOTION_LANES]=8;i->movy_playback=1;
    assert(event(i,alias,1)==normal-1);assert(i->movy_pad_shift[alias]==36);event(i,alias,0);i->movy_playback=0;
    for(int mode=0;mode<8;mode++){
        memset(i->movy_pad_shift,0,sizeof(i->movy_pad_shift));i->travel_map=mode;API.get_param(i,"pad_view",view,sizeof(view));assert(strstr(view,mode==6?"|piano1,1":"|piano1,0"));
        API.set_param(i,"hb_movy_input_approach",param);event(i,alias,1);assert((i->movy_pad_shift[alias]!=0)==(mode==6));event(i,alias,0);
    }
    i->role=3;API.set_param(i,"hb_movy_input_approach",param);event(i,alias,1);assert(!i->movy_pad_pending);
    i->role=1;i->travel_map=6;API.set_param(i,"hb_movy_input_approach",param);event(i,alias,1);
    hb_clear_instance_note_state(i);assert(!i->movy_pad_pending&&!i->movy_pad_shift[alias]);
    puts("piano approaches: all scales and MIDI targets, exact mapped resolution, independent ownership, recording and travel gating pass");
}
