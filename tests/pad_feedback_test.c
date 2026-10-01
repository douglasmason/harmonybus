/* Preview chord expansion must not make chromatic approaches look diatonic.
   Short MIDI onsets need a separate, bounded visual lifetime. */
#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main
static unsigned section(const char *view,const char *key){
    const char *start=strstr(view,key);unsigned mask=0;assert(start);
    assert(sscanf(start+strlen(key),"%u",&mask)==1);return mask;
}
static int routed_notes;
static int route_probe(const uint8_t *packet,int length){if(length==4&&(packet[1]&0xf0)==0x90&&packet[3])routed_notes++;return length;}
static void route_note(Inst *instance,int on){uint8_t message[3]={(uint8_t)(on?0x90:0x80),60,(uint8_t)(on?100:0)},output[64][3];int lengths[64];API.process_midi(instance,message,3,output,lengths,64);for(int block=0;block<100;block++){position+=.006;API.tick(instance,144,48000,output,lengths,64);}}
static void movy_input_and_spatial_sequence(void){
    host.midi_inject_to_move=route_probe;
    for(int role=0;role<2;role++)for(int playback=0;playback<2;playback++)for(int destination=1;destination<=4;destination++){
        Inst *instance=fixture();API.set_param(instance,"role",role?"Follower":"Conductor");
        API.set_param(instance,"receive_channel","Off");char channel[4];snprintf(channel,sizeof(channel),"%d",destination);API.set_param(instance,"render_channel",channel);
        API.set_param(instance,"hb_movy_clip","0,384,0,1,1,0,96,0,0");
        API.set_param(instance,"hb_movy_playback",playback?"1":"0");
        API.set_param(instance,"boundary_buffer_ms","0 ms");
        assert(instance->source_channel==-1&&instance->movy_track==0);
        assert(hb_source_channel_matches(instance,0));assert(!hb_source_channel_matches(instance,1));
        routed_notes=0;route_note(instance,1);
        assert(instance->note_on_count>0&&routed_notes>0);
        route_note(instance,0);
        API.destroy_instance(instance);
    }
    Inst *instance=fixture();instance->approach_layout=1;instance->preview_count=32;
    memset(instance->preview_rows,0,sizeof(instance->preview_rows));
    API.set_param(instance,"approach_bank_1","Connector Below");API.set_param(instance,"approach_bank_2","Connector Above");
    API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_2","Down");
    API.set_param(instance,"approach_touch_1","Up,50");API.set_param(instance,"approach_touch_2","Up,50");
    hb_ar_state *state=&instance->approach_rows;
    assert(state->sequence_count==2&&!state->performance);
    assert(hb_ar_live_peek(state,3)==1&&hb_ar_live_peek(state,-1)==0);
    hb_ar_live_advance(state,3);assert(hb_ar_live_peek(state,3)==2&&hb_ar_live_peek(state,-1)==0);
    hb_ar_live_advance(state,3);assert(hb_ar_live_peek(state,3)==1);
    API.set_param(instance,"approach_step_touch_3","Down");assert(state->sequence_count==2);
    API.set_param(instance,"approach_step_touch_3","Up,50");assert(state->sequence_cursor==0);
    char saved[8192];API.get_param(instance,"state",saved,sizeof(saved));assert(strstr(saved,";ar6,2,0,1"));
    API.set_param(instance,"state",saved);assert(state->sequence_count==2&&state->sequence_cursor==0);
    API.set_param(instance,"approach_touch_2","Down");assert(state->sequence_count==1);API.set_param(instance,"approach_touch_2","Up,50");
    API.destroy_instance(instance);
}
int main(void){
    movy_input_and_spatial_sequence();
    Inst *instance=fixture();instance->travel_map=0;instance->content_map=1;instance->chromatic_map=1;
    hb_set_shared_follower_scale(1);g_bus.observed_harmony=chord(0,0,0);hb_effective_write(g_bus.observed_harmony);
    instance->approach_layout=1;API.set_param(instance,"approach_bank_1","Connector Below");
    for(int mode=0;mode<3;mode++)for(int target=60;target<72;target++){
        instance->player.config.mode=mode;
        Inst preview=*instance;int alias=target<64?target+36:target-36;
        preview.movy_pad_shift[alias]=target-alias;preview.approach_rows.preview_row=3;
        unsigned long long low=0,high=0;
        unsigned rendered=hb_pad_render_mask(&preview,instance,hb_render_harmony(instance),alias,1,&low,&high);
        unsigned scale=hb_follower_scale_target(instance,hb_render_harmony(instance)).pitch_mask;
        assert(!(rendered&~preview.preview_gap_mask)==!(rendered&~scale));
        if(target==64){assert(rendered==(1u<<3));assert(!(preview.preview_gap_mask&(1u<<3)));}
    }
    API.destroy_instance(instance);
    instance=fixture();API.set_param(instance,"approach_bank_1","Connector Below");
    API.set_param(instance,"approach_bank_2","Secondary V");
    API.set_param(instance,"approach_control_1","LatchOn");
    assert(instance->approach_rows.performance&&instance->approach_rows.latch_slots==1);
    char layout_payload[68];for(int pad=0;pad<32;pad++)sprintf(layout_payload+2*pad,"%02x",60+pad);
    strcpy(layout_payload+64,";1");API.set_param(instance,"pad_preview_inputs",layout_payload);
    assert(!instance->approach_rows.performance&&!instance->approach_rows.latch&&instance->approach_rows.latch_slots==0);
    API.set_param(instance,"approach_touch_2","Down");API.set_param(instance,"approach_touch_2","Up,50");
    assert(instance->approach_rows.row_preset==1&&!instance->approach_rows.performance);
    int saved_rows[3];memcpy(saved_rows,instance->approach_rows.row_slots,sizeof(saved_rows));
    API.set_param(instance,"approach_step_touch_1","Down");API.set_param(instance,"approach_step_touch_1","Up,50");
    assert(instance->approach_rows.performance&&!instance->approach_rows.latch);
    assert(!memcmp(saved_rows,instance->approach_rows.row_slots,sizeof(saved_rows)));
    API.set_param(instance,"approach_control_2","LatchOn");assert(instance->approach_rows.latch_slots==0);
    strcpy(layout_payload+64,";0");API.set_param(instance,"pad_preview_inputs",layout_payload);
    assert(!instance->approach_rows.performance);
    API.set_param(instance,"approach_touch_1","Down");API.set_param(instance,"approach_touch_1","Up,50");
    assert(instance->approach_rows.performance&&!instance->approach_rows.latch);
    assert(!memcmp(saved_rows,instance->approach_rows.row_slots,sizeof(saved_rows)));
    /* Factory slots migrate once; custom choices survive and new saves are stable. */
    for(int slot=0;slot<16;slot++)instance->approach_rows.bank[slot]=slot+1;
    instance->approach_rows.bank[2]=-2;
    char saved_state[65536];API.get_param(instance,"state",saved_state,sizeof(saved_state));
    char *marker=strstr(saved_state,";ar5,1");assert(marker);memmove(marker,marker+6,strlen(marker+6)+1);
    API.set_param(instance,"state",saved_state);
    assert(instance->approach_rows.bank[0]==-5&&instance->approach_rows.bank[1]==-4&&instance->approach_rows.bank[2]==-2);
    instance->approach_rows.bank[0]=1;API.get_param(instance,"state",saved_state,sizeof(saved_state));
    API.set_param(instance,"state",saved_state);assert(instance->approach_rows.bank[0]==1);
    API.destroy_instance(instance);
    instance=fixture();instance->role=3;
    char request[80]="pad_view@",view[4096];
    for(int slot=0;slot<32;slot++)snprintf(request+9+slot*2,3,"%02x",48+slot);
    uint8_t messages[2][3]={{0x90,60,100},{0x80,60,0}};int lengths[2]={3,3};
    hb_pad_observe_output(instance,messages,lengths,2);
    assert(!instance->pad_sounding[60]);
    API.get_param(instance,request,view,sizeof(view));
    assert(section(view,"|playpads1,")==0);
    assert(section(view,"|playflash1,")==1u<<12);
    uint8_t output[64][3];int sizes[64];
    API.tick(instance,2400,48000,output,sizes,64);
    API.get_param(instance,request,view,sizeof(view));assert(section(view,"|playflash1,")==1u<<12);
    API.tick(instance,1440,48000,output,sizes,64);
    API.get_param(instance,request,view,sizeof(view));assert(section(view,"|playflash1,")==0);
    hb_pad_observe_output(instance,messages,lengths,2);
    uint8_t stop[1][3]={{0xfc,0,0}};int one[1]={1};hb_pad_observe_output(instance,stop,one,1);
    API.get_param(instance,request,view,sizeof(view));assert(section(view,"|playflash1,")==0);
    API.destroy_instance(instance);
    puts("Approach membership stays chromatic across chord modes; short onsets flash without sustaining MIDI and clear on stop");
}
