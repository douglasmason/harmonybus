/* Preview chord expansion must not make chromatic approaches look diatonic.
   Short MIDI onsets need a separate, bounded visual lifetime. */
#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main
static void active_key_colors(void){
    Inst *instance=fixture();
    API.set_param(instance,"dominant_minor_scale","Harmonic Minor");
    g_key_context=(hb_key_context){.active=1,.source_root=0,.target_root=9,
        .source_mask=hb_explicit_scale_mask(0,1),.target_mask=hb_explicit_scale_mask(9,2)};
    hb_harmony_t dominant=chord(7,0,1);
    unsigned e7=(1u<<4)|(1u<<8)|(1u<<11)|(1u<<2);
    /* G7 becomes E7 in A minor, never Em7. Color and pulse membership must
       preserve the same functional third as the sounding harmony. */
    assert(hb_pad_chord_mask_form(instance,dominant,0)==e7);
    g_pad_settings[0]=0;g_pad_chord_form=0;
    API.set_param(instance,"pad_next_pulse","3+7");
    assert(hb_pad_next_mask(instance,dominant)==((1u<<8)|(1u<<2)));
    hb_commit_observed_harmony(dominant);
    char view[4096];
    for(int chromatic=0;chromatic<2;chromatic++)for(int travel=0;travel<8;travel++){
        instance->chromatic_map=chromatic;instance->travel_map=travel;instance->content_map=0;
        API.get_param(instance,"pad_render",view,sizeof(view));
        unsigned current,effective,scale,expected=0,tonic=0;
        assert(sscanf(view,"%u,%u,%u",&current,&effective,&scale)==3);
        for(int pitch=0;pitch<12;pitch++){
            int rendered=mod12(hb_map_follower_note_unoperated(instance,60+pitch));
            if(e7&(1u<<rendered))expected|=1u<<pitch;
            if(rendered==4)tonic|=1u<<pitch;
        }
        assert(effective==expected);
        const char *field=strstr(view,"|tonic1,");unsigned actual_tonic;
        assert(field&&sscanf(field+8,"%u",&actual_tonic)==1);
        assert(actual_tonic==tonic);
    }
    API.destroy_instance(instance);
}
static unsigned section(const char *view,const char *key){
    const char *start=strstr(view,key);unsigned mask=0;assert(start);
    assert(sscanf(start+strlen(key),"%u",&mask)==1);return mask;
}
static void all_pulse_options(void){
    Inst *i=fixture();
    const int forms[]={1,2,3,4,8,9,15,16};
    const unsigned tones[]={0x81,0x91,0x891,0x895,0x8b5,0xab5,0x810,0x814};
    const unsigned pulses[]={0,0x10,0x800,0x810,1,0x80,0x81,4,0x20,0x200,0x224,0xfff};
    for(int root=0;root<12;root++)for(int f=0;f<8;f++){
        hb_harmony_t h=chord(root,0,0);
        hb_set_shared_follower_scale(1);globals.follower_explicit_root=root;
        API.set_param(i,"pad_chord_form",CP_CHORD_FORM[forms[f]]);API.set_param(i,"pad_next_chord_form",CP_CHORD_FORM[forms[f]]);
        for(int p=0;p<12;p++){
            API.set_param(i,"pad_next_pulse",PAD_NEXT_PULSE[p]);
            assert(hb_pad_next_mask(i,h)==hb_transpose_mask(tones[f]&pulses[p],root));
        }
    }
    API.destroy_instance(i);
}
static void next_tone_pulse(void){
    Inst *instance=fixture();char view[4096],saved[65536],value[64];
    API.get_param(instance,"pad_next_pulse",value,sizeof(value));assert(!strcmp(value,"None"));
    /* Guide-tone classification handles major, minor, half/full diminished. */
    static const int intervals[5][4]={{0,4,7,11},{0,4,7,10},{0,3,7,10},{0,3,6,10},{0,3,6,9}};
    for(int quality=0;quality<5;quality++)for(int root=0;root<12;root++){
        uint8_t notes[4];for(int i=0;i<4;i++)notes[i]=60+root+intervals[quality][i];
        hb_harmony_t harmony=hb_infer_harmony(notes,4);harmony.root_pc=root;
        API.set_param(instance,"pad_next_pulse","3+7");
        assert(hb_pad_next_mask(instance,harmony)==((1u<<mod12(root+intervals[quality][1]))|(1u<<mod12(root+intervals[quality][3]))));
        API.set_param(instance,"pad_chord_form","Rootless 7");API.set_param(instance,"pad_next_chord_form","Rootless 7");
        assert(hb_pad_next_mask(instance,harmony)==((1u<<mod12(root+intervals[quality][1]))|(1u<<mod12(root+intervals[quality][3]))));
        API.set_param(instance,"pad_chord_form","Follow Detected");API.set_param(instance,"pad_next_chord_form","Follow Detected");
        API.set_param(instance,"pad_next_pulse","9+11+13");assert(!hb_pad_next_mask(instance,harmony));
    }
    API.set_param(instance,"pad_next_pulse","3+7");
    API.set_param(instance,"travel_map","None");API.set_param(instance,"chord_mode","Off");
    API.set_param(instance,"pad_chord_form","Follow Detected");API.set_param(instance,"pad_next_chord_form","Follow Detected");
    hb_set_shared_follower_scale(1);instance->next_lookahead=0;
    g_bus.clip_loop_end=4;g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=chord(0,0,0)};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=chord(2,1,1)};
    position=1.5;g_bus.observed_harmony=g_bus.next_model[0].harmony;hb_effective_write(g_bus.observed_harmony);
    Inst unchanged=*instance;hb_harmony_t bus_before=bus_read();
    API.get_param(instance,"pad_view",view,sizeof(view));
    assert(section(view,"|nextpulse1,")==((1u<<5)|(1u<<0)));
    assert(!memcmp(&unchanged,instance,sizeof(unchanged)));assert(hb_harmony_equal_effective(bus_before,bus_read()));
    API.set_param(instance,"pad_display","Current");
    API.get_param(instance,"pad_view",view,sizeof(view));assert(!section(view,"|nextpulse1,"));
    API.set_param(instance,"pad_display","Effective");
    API.get_param(instance,"pad_view",view,sizeof(view));assert(section(view,"|nextpulse1,")==1u<<4);
    API.set_param(instance,"pad_display","Lookahead");
    API.get_param(instance,"pad_view",view,sizeof(view));assert(!section(view,"|nextpulse1,"));
    API.set_param(instance,"pad_display","Both Full Lookahead");
    /* Wrapping advances selection back to C major, without inventing a seventh. */
    position=3.5;API.get_param(instance,"pad_view",view,sizeof(view));assert(section(view,"|nextpulse1,")==1u<<4);
    /* An explicit color form adds a seventh even when detection is a triad. */
    API.set_param(instance,"pad_chord_form","Seventh");API.set_param(instance,"pad_next_chord_form","Seventh");
    API.set_param(instance,"pad_next_pulse","7");
    API.get_param(instance,"pad_view",view,sizeof(view));assert(section(view,"|nextpulse1,")==1u<<11);
    position=1.5;API.set_param(instance,"pad_display","Effective");
    API.get_param(instance,"pad_view",view,sizeof(view));assert(section(view,"|nextpulse1,")==1u<<11);
    API.set_param(instance,"pad_chord_form","Power");
    API.get_param(instance,"pad_view",view,sizeof(view));assert(!section(view,"|nextpulse1,"));
    API.set_param(instance,"pad_chord_form","Follow Detected");API.set_param(instance,"pad_next_chord_form","Follow Detected");
    API.get_param(instance,"pad_view",view,sizeof(view));assert(!section(view,"|nextpulse1,"));
    API.set_param(instance,"pad_next_pulse","3+7");
    API.set_param(instance,"pad_display","Both Full Lookahead");
    g_bus.next_model_count=0;g_bus.next_model_locked=0;
    API.get_param(instance,"pad_view",view,sizeof(view));assert(!section(view,"|nextpulse1,"));
    API.get_param(instance,"state",saved,sizeof(saved));assert(strstr(saved,";pnp1,3"));
    hb_pad_defaults();API.set_param(instance,"state",saved);assert(g_pad_next_pulse==3);
    API.set_param(instance,"state","0;pd1,6,3,3,2,0;pnp1,0");assert(g_pad_next_pulse==3);
    hb_pad_defaults();API.set_param(instance,"state","0;pd1,6,3,3,2,0");assert(!g_pad_next_pulse);
    API.destroy_instance(instance);
}
static void independent_pad_form(void){
    Inst *instance=fixture();
    API.set_param(instance,"travel_map","None");
    API.set_param(instance,"chord_mode","Scale Degree");
    API.set_param(instance,"chord_form","Ninth");
    API.set_param(instance,"pad_chord_form","Rootless 7");
    static const int intervals[5][4]={{0,4,7,11},{0,4,7,10},{0,3,7,10},{0,3,6,10},{0,3,6,9}};
    for(int quality=0;quality<5;quality++)for(int root=0;root<12;root++){
        uint8_t notes[4];for(int index=0;index<4;index++)notes[index]=60+root+intervals[quality][index];
        hb_harmony_t harmony=hb_infer_harmony(notes,4);
        unsigned expected=(1u<<mod12(root+intervals[quality][1]))|(1u<<mod12(root+intervals[quality][3]));
        /* Dim7 inversion is symmetric: test the intended root explicitly. */
        harmony.root_pc=root;
        assert(hb_pad_chord_mask(instance,harmony)==expected);
        assert(instance->player.config.size==4);
        for(int form=0;form<HB_CP_FORMS;form++){
            API.set_param(instance,"pad_chord_form",CP_CHORD_FORM[form]);
            if(!form)assert(hb_pad_chord_mask(instance,harmony)==hb_harmony_chord_mask(harmony));
            else {
                hb_cp_config config;hb_cp_defaults(&config);config.mode=2;config.size=form;config.inversion=1;
                int rendered[12];unsigned mask=0;
                int count=hb_cp_voice(config,60+root,root,hb_harmony_chord_mask(harmony),hb_follower_scale_target(instance,harmony).pitch_mask,rendered);
                for(int index=0;index<count;index++)mask|=1u<<mod12(rendered[index]);
                assert(hb_pad_chord_mask(instance,harmony)==mask);
            }
        }
        API.set_param(instance,"pad_chord_form","Rootless 7");
    }
    hb_harmony_t harmony=chord(0,0,1);hb_effective_write(harmony);g_bus.observed_harmony=harmony;
    Inst preview=*instance;
    assert(hb_pad_target_inputs(&preview,instance,harmony,hb_pad_chord_mask(instance,harmony))==((1u<<4)|(1u<<10)));
    assert(instance->player.config.size==4);
    char snapshot[4096];unsigned current=0,effective=0,scale=0;
    API.get_param(instance,"pad_render",snapshot,sizeof(snapshot));
    assert(sscanf(snapshot,"%u,%u,%u",&current,&effective,&scale)==3);
    assert(current==((1u<<4)|(1u<<10))&&effective==current);
    assert(instance->player.config.size==4);
    char state[65536],value[64];API.get_param(instance,"state",state,sizeof(state));
    assert(strstr(state,";pf1,15"));
    API.destroy_instance(instance);instance=fixture();API.set_param(instance,"state",state);
    API.get_param(instance,"pad_chord_form",value,sizeof(value));assert(!strcmp(value,"Rootless 7"));
    /* A later track's saved defaults cannot overwrite the global choice. */
    API.set_param(instance,"state","0");assert(g_pad_chord_form==15);
    API.destroy_instance(instance);
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
static void separate_color_forms(void){
    Inst *instance=fixture();char view[4096],saved[65536];
    assert(!g_pad_adjacent_shading);
    static const int qualities[5][4]={{0,4,7,11},{0,4,7,10},{0,3,7,10},{0,3,6,10},{0,3,6,9}};
    for(int quality=0;quality<5;quality++)for(int root=0;root<12;root++){
        uint8_t notes[4];for(int index=0;index<4;index++)notes[index]=60+root+qualities[quality][index];
        hb_harmony_t detected=hb_infer_harmony(notes,4);detected.root_pc=root;
        assert(hb_pad_chord_mask_form(instance,detected,18)==(1u<<root));
        assert(hb_pad_chord_mask_form(instance,detected,19)==((1u<<root)|(1u<<mod12(root+qualities[quality][1]))));
        assert(hb_pad_chord_mask_form(instance,detected,20)==((1u<<root)|(1u<<mod12(root+qualities[quality][3]))));
    }
    API.set_param(instance,"travel_map","None");API.set_param(instance,"chord_mode","Off");
    API.set_param(instance,"pad_chord_form","Root Only");API.set_param(instance,"pad_next_chord_form","Root + Seventh");
    API.set_param(instance,"pad_next_pulse","7");
    hb_harmony_t harmony=chord(0,0,1);
    assert(hb_pad_chord_mask(instance,harmony)==1);
    assert(hb_pad_chord_mask_form(instance,harmony,19)==17);
    assert(hb_pad_chord_mask_form(instance,harmony,20)==1025);
    g_bus.clip_loop_end=4;g_bus.next_model_locked=1;g_bus.next_model_count=2;
    g_bus.next_model[0]=(hb_loop_harmony_event_t){.phase=0,.harmony=harmony};
    g_bus.next_model[1]=(hb_loop_harmony_event_t){.phase=2,.harmony=harmony};
    position=1;g_bus.observed_harmony=harmony;hb_effective_write(harmony);
    API.get_param(instance,"pad_view",view,sizeof(view));
    unsigned current,effective,scale,ready,next,full_valid,full;
    assert(sscanf(view,"%u,%u,%u,%u,%u",&current,&effective,&scale,&ready,&next)==5);
    assert(current==1&&effective==1);
    assert(sscanf(strstr(view,"|full1,")+7,"%u,%u",&full_valid,&full)==2);assert(full==1025);
    assert(section(view,"|nextpulse1,")==1024);
    API.set_param(instance,"pad_display","Effective");
    API.get_param(instance,"pad_view",view,sizeof(view));assert(section(view,"|nextpulse1,")==0);
    API.get_param(instance,"state",saved,sizeof(saved));
    hb_pad_defaults();API.set_param(instance,"state",saved);assert(g_pad_chord_form==18&&g_pad_next_chord_form==20);
    char *next_suffix=strstr(saved,";pnf1,");assert(next_suffix);char *suffix_end=strchr(next_suffix+1,';');if(suffix_end)memmove(next_suffix,suffix_end,strlen(suffix_end)+1);else *next_suffix=0;
    hb_pad_defaults();API.set_param(instance,"state",saved);assert(g_pad_chord_form==18&&g_pad_next_chord_form==18);
    API.destroy_instance(instance);
}


static void harmony_off_view(void){
    Inst *instance=fixture();
    API.set_param(instance,"follower_scale","Major");API.set_param(instance,"pad_display","Harmony Off");
    char first[4096],second[4096],setting[64];
    API.get_param(instance,"pad_display",setting,sizeof(setting));assert(!strcmp(setting,"Harmony Off"));
    hb_effective_write(chord(0,0,1));API.get_param(instance,"pad_view",first,sizeof(first));
    hb_effective_write(chord(6,1,1));API.get_param(instance,"pad_view",second,sizeof(second));
    /* Harmony Off freezes pad colors, but the musical footer stays live. */
    char *first_footer=strstr(first,"|footer1,"),*second_footer=strstr(second,"|footer1,");
    assert(first_footer&&second_footer);assert(strcmp(first_footer,second_footer));
    *first_footer=*second_footer=0;
    assert(!strcmp(first,second));
    assert(strstr(first,"0,0,2741,0,0,7,0,3,"));
    assert(!strstr(first,"outputs1,")&&!strstr(first,"gapcolors1,")&&!strstr(first,"nextpulse1,"));
    assert(strstr(first,"|input1,0,1,1,2741,0"));
    API.set_param(instance,"pad_display","Effective");API.get_param(instance,"pad_view",second,sizeof(second));
    assert(strcmp(first,second));API.destroy_instance(instance);
}

int main(void){active_key_colors();harmony_off_view();
    separate_color_forms();
    all_pulse_options();next_tone_pulse();independent_pad_form();
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
