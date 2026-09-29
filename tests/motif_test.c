#define main existing_chord_tests
#include "chord_player_test.c"
#undef main
static void record_note(Inst *instance,int pitch){midi(instance,1,pitch);midi(instance,0,pitch);}
static void recording(void){
    Inst *instance=fixture();API.set_param(instance,"motif_record","Edit");
    assert(instance->motif.editor.recording==0);
    record_note(instance,65);advance(instance,1000,64);
    API.set_param(instance,"motif_arrow","1");record_note(instance,63);
    midi(instance,1,64);API.set_param(instance,"motif_arrow","1");midi(instance,0,64);
    assert(instance->motif.editor.draft.count==5);
    assert(instance->motif.editor.draft.events[1].kind==1);
    assert(instance->motif.editor.draft.events[4].kind==2);
    API.set_param(instance,"motif_record","Done");
    assert(g_motifs[0].count==5&&g_motifs[0].anchor==3);
    assert(g_motifs[0].events[0].duration==6);
    API.set_param(instance,"motif_record","Edit");API.set_param(instance,"motif_record","Done");assert(g_motifs[0].count==5);
    advance(instance,500,64);API.set_param(instance,"motif_arrival","Next Beat");position=0;render_count=0;
    API.set_param(instance,"motif_arm","1");record_note(instance,67);
    assert(instance->motif.editor.armed==-1);
    position=0.25;API.tick(instance,1,48000,output,lengths,64);
    assert(render_count==1&&rendered[0][2]==69);
    position=0.50;API.tick(instance,1,48000,output,lengths,64);
    position=0.75;API.tick(instance,1,48000,output,lengths,64);
    assert(rendered[render_count-1][2]==66);
    position=1.0;API.tick(instance,1,48000,output,lengths,64);
    assert(rendered[render_count-1][2]==67&&(rendered[render_count-1][1]&0xF0)==0x90);
    int before=render_count;position=1.25;API.tick(instance,1,48000,output,lengths,64);assert(render_count==before);
    position=1.5;API.tick(instance,1,48000,output,lengths,64);assert((rendered[render_count-1][1]&0xF0)==0x80);
    char state[131072];int length=API.get_param(instance,"state",state,sizeof(state));assert(length>0&&length<(int)sizeof(state));
    API.destroy_instance(instance);assert(!g_motifs[0].count);
    instance=API.create_instance("",NULL);API.set_param(instance,"state",state);assert(g_motifs[0].count==5&&g_motifs[0].anchor==3);
    char truncated[131072];strcpy(truncated,state);truncated[strlen(truncated)-2]=0;g_motifs_restored=0;
    API.set_param(instance,"state",truncated);assert(g_motifs[0].count==5);
    API.destroy_instance(instance);
}
static void explicit_intent(void){
    Inst *instance=fixture();API.set_param(instance,"motif_record","Edit");
    API.set_param(instance,"mod_scale_above","On");record_note(instance,64);API.set_param(instance,"mod_scale_above","Off");
    API.set_param(instance,"mod_chrom_below","On");record_note(instance,64);API.set_param(instance,"mod_chrom_below","Off");
    record_note(instance,64);API.set_param(instance,"motif_record","Done");
    assert(g_motifs[0].events[0].modifier==1&&g_motifs[0].events[1].modifier==-1);
    advance(instance,500,64);position=0;API.set_param(instance,"motif_arrival","Next Beat");API.set_param(instance,"motif_trigger","67");
    render_count=0;position=0.5;API.tick(instance,1,48000,output,lengths,64);assert(rendered[render_count-1][2]==69);
    position=0.75;API.tick(instance,1,48000,output,lengths,64);assert(rendered[render_count-1][2]==66);
    position=1;API.tick(instance,1,48000,output,lengths,64);assert(rendered[render_count-1][2]==67);
    uint8_t stop=0xfc;API.process_midi(instance,&stop,1,output,lengths,64);API.tick(instance,1,48000,output,lengths,64);
    for(int index=0;index<HB_MT_SCHEDULE;index++)assert(!instance->motif.events[index].used);
    API.destroy_instance(instance);
}
static void anchor_and_late(void){
    Inst *instance=fixture();API.set_param(instance,"motif_record","Edit");record_note(instance,60);
    API.set_param(instance,"motif_anchor","On");record_note(instance,64);record_note(instance,67);API.set_param(instance,"motif_record","Done");assert(g_motifs[0].anchor==1);
    advance(instance,500,64);API.set_param(instance,"motif_arrival","Next Beat");API.set_param(instance,"motif_late","Trim");position=.9;API.set_param(instance,"motif_trigger","64");
    int attacks=0;for(int index=0;index<HB_MT_SCHEDULE;index++)if(instance->motif.events[index].used){assert(instance->motif.events[index].on>=1.0-1e-6);attacks++;}assert(attacks==2);
    position=1;API.tick(instance,1,48000,output,lengths,1);assert(rendered[render_count-1][2]==64);
    instance->motif.cancel=1;API.tick(instance,1,48000,output,lengths,1);
    for(int index=0;index<HB_MT_SCHEDULE;index++)assert(!instance->motif.events[index].used);
    API.destroy_instance(instance);
}

static void fit_defer_and_cancel(void){
    Inst *instance=fixture();API.set_param(instance,"motif_record","Edit");
    record_note(instance,60);record_note(instance,62);record_note(instance,64);API.set_param(instance,"motif_record","Done");
    advance(instance,500,64);position=.75;API.set_param(instance,"motif_arrival","Next Beat");API.set_param(instance,"motif_late","Fit");
    assert(hb_mt_launch(instance,64,100,0));int count=0;
    for(int index=0;index<HB_MT_SCHEDULE;index++)if(instance->motif.events[index].used){assert(fabs(instance->motif.events[index].on-(.75+count*.125))<1e-6);count++;}
    assert(count==3);instance->motif.cancel=1;assert(!hb_mt_launch(instance,64,100,0));
    API.tick(instance,1,48000,output,lengths,64);assert(!instance->motif.pending);
    API.set_param(instance,"motif_late","Defer");assert(hb_mt_launch(instance,64,100,0));count=0;
    for(int index=0;index<HB_MT_SCHEDULE;index++)if(instance->motif.events[index].used){assert(fabs(instance->motif.events[index].on-(1.5+count*.25))<1e-6);count++;}
    assert(count==3);API.destroy_instance(instance);
}
static void bank_capacity_and_snapshots(void){
    Inst *instance=fixture();API.set_param(instance,"motif_record","Edit");record_note(instance,64);API.set_param(instance,"motif_record","Done");
    hb_mt_event event=g_motifs[0].events[0];
    /* Exercise the final provenance word as well as the highest operation lanes. */
    event.actions[HB_MOTION_LANES]=0xdeadbeef12345678ULL;
    event.actions[HB_MOTION_LANES-1]=0xabcdef0987654321ULL;
    for(int slot=0;slot<4;slot++){
        g_motifs[slot].count=32;g_motifs[slot].anchor=31;
        for(int step=0;step<32;step++)g_motifs[slot].events[step]=event;
    }
    char state[8192];int size=API.get_param(instance,"state",state,sizeof(state));assert(size>0&&size<(int)sizeof(state));
    char short_buffer[16];assert(hb_mt_save(instance,short_buffer,sizeof(short_buffer),0)<0);
    API.destroy_instance(instance);instance=API.create_instance("",NULL);API.set_param(instance,"state",state);
    assert(g_motifs[3].count==32&&!memcmp(g_motifs[3].events[31].actions,event.actions,sizeof(event.actions)));
    API.set_param(instance,"motif_slot","5");API.set_param(instance,"motif_record","Edit");
    record_note(instance,67);API.set_param(instance,"motif_record","Done");
    assert(instance->motif.editor.recording==4&&instance->motif.editor.error==9);
    assert(!g_motifs[4].count&&g_motifs[3].count==32);
    API.set_param(instance,"motif_cancel","Cancel");API.destroy_instance(instance);
}


static void tap_presets_and_clock(void){
    Inst *instance=fixture();API.set_param(instance,"motif_preset","ii-V-Target");
    API.set_param(instance,"motif_playback","Tap Free");API.set_param(instance,"motif_arm","1");
    const int expected[]={62,55,60};const double beats[]={.13,.71,1.0};
    for(int step=0;step<3;step++){
        position=beats[step];record_note(instance,60);
        assert(instance->motif.tap_step==step+1);
        int found=0;for(int index=0;index<HB_MT_SCHEDULE;index++){
            hb_mt_scheduled *event=&instance->motif.events[index];
            if(event->used&&fabs(event->on-position)<1e-6){assert(event->pitch==expected[step]);found++;}
        }assert(found==1);
        API.tick(instance,1,48000,output,lengths,64);
    }
    assert(!instance->motif.tap_active);API.set_param(instance,"motif_cancel","Cancel");API.tick(instance,1,48000,output,lengths,64);
    API.set_param(instance,"motif_playback","Tap Grid");API.set_param(instance,"motif_grid","1/16");position=.13;
    for(int step=0;step<3;step++)assert(hb_mt_launch(instance,60,100,0));
    int found=0;for(int index=0;index<HB_MT_SCHEDULE;index++)if(instance->motif.events[index].used){assert(fabs(instance->motif.events[index].on-(.25+found*.25))<1e-6);found++;}assert(found==3);
    API.set_param(instance,"motif_cancel","Cancel");API.tick(instance,1,48000,output,lengths,64);
    API.set_param(instance,"motif_playback","Tap Guided");API.set_param(instance,"motif_arrival","Next Bar");position=1.9;
    assert(hb_mt_launch(instance,60,100,0));position=4.1;API.tick(instance,1,48000,output,lengths,64);
    assert(instance->motif.tap_active&&instance->motif.tap_step==1&&!instance->motif.pending); /* Manual never completes itself. */
    API.set_param(instance,"motif_cancel","Cancel");API.tick(instance,1,48000,output,lengths,64);
    API.set_param(instance,"motif_completion","Auto Finish");position=1.9;assert(hb_mt_launch(instance,60,100,0));
    position=3;API.tick(instance,1,48000,output,lengths,64);assert(!instance->motif.tap_active);
    int anchor=0;for(int index=0;index<HB_MT_SCHEDULE;index++)if(instance->motif.events[index].used&&instance->motif.events[index].pitch==60&&fabs(instance->motif.events[index].on-4)<1e-6)anchor++;
    assert(anchor==1);
    API.set_param(instance,"motif_cancel","Cancel");API.tick(instance,1,48000,output,lengths,64);
    position=5;assert(hb_mt_launch(instance,60,100,0));uint8_t stop=0xfc;API.process_midi(instance,&stop,1,output,lengths,64);API.tick(instance,1,48000,output,lengths,64);assert(!instance->motif.tap_active&&!instance->motif.pending);
    API.destroy_instance(instance);
}
static void preset_copy_and_state(void){
    Inst *instance=fixture();
    for(int preset=1;preset<HB_MT_PRESET_COUNT;preset++){
        hb_mt_phrase phrase;hb_mt_preset(preset,&phrase);assert(phrase.count>=2&&phrase.anchor==phrase.count-1);
        for(int step=0;step<phrase.count;step++)assert(phrase.events[step].count==1&&phrase.events[step].duration==24);
    }
    API.set_param(instance,"motif_preset","V/V-V-I");API.set_param(instance,"motif_copy","Copy to Slot");
    assert(instance->motif.editor.recording==0&&instance->motif.editor.draft.count==3&&!g_motifs[0].count);
    API.set_param(instance,"motif_record","Done");assert(g_motifs[0].count==3&&g_motifs[0].anchor==2);
    API.set_param(instance,"motif_playback","Tap Grid");API.set_param(instance,"motif_completion","Auto Finish");
    API.set_param(instance,"motif_preset","ii-V-Target");API.set_param(instance,"motif_grid","1/8");
    char state[8192];assert(API.get_param(instance,"state",state,sizeof(state))<(int)sizeof(state));
    API.destroy_instance(instance);instance=API.create_instance("",NULL);API.set_param(instance,"state",state);
    assert(instance->motif.editor.playback==3&&instance->motif.editor.preset==2&&instance->motif.editor.tap_grid==2&&instance->motif.editor.completion==1);
    assert(g_motifs[0].count==3&&g_motifs[0].events[0].chord_mode==3&&!instance->motif.tap_active);
    API.destroy_instance(instance);
}


static void tap_chord_identity(void){
    Inst *instance=fixture();API.set_param(instance,"chord_mode","Scale Degree");
    instance->player.config.mode=1;API.set_param(instance,"chord_form","Seventh");
    API.set_param(instance,"motif_preset","ii-V-Target");API.set_param(instance,"motif_playback","Tap Free");
    const unsigned expected[]={ (1u<<2)|(1u<<5)|(1u<<9)|(1u<<0), (1u<<7)|(1u<<11)|(1u<<2)|(1u<<5), (1u<<0)|(1u<<4)|(1u<<7)|(1u<<11) };
    for(int step=0;step<3;step++){
        position=step*2;assert(hb_mt_launch(instance,60,100,0));unsigned mask=0;
        for(int index=0;index<HB_MT_SCHEDULE;index++)if(instance->motif.events[index].used&&fabs(instance->motif.events[index].on-position)<1e-6)mask|=1u<<mod12(instance->motif.events[index].pitch);
        assert(mask==expected[step]);API.tick(instance,1,48000,output,lengths,64);
    }
    API.destroy_instance(instance);
}

int main(void){tap_chord_identity();tap_presets_and_clock();preset_copy_and_state();fit_defer_and_cancel();bank_capacity_and_snapshots();recording();explicit_intent();anchor_and_late();puts("motifs: untimed entry, cadence presets, tap/free/grid/guided, auto finish, chord intent, anchors, late policies, stop and state pass");}
