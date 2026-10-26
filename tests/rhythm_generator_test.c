#define main reference_tests_main
#include "follower_reference_test.c"
#undef main
static void patterns(void){
    hb_rg_clock r={.pattern=5,.span=2};int hits=0;
    for(int k=0;k<64;k++){int hit=hb_rg_advance(&r,k/16.0,1);if(hit>=0)hits++;}
    assert(hits==3);assert(hb_rg_advance(&r,4,1)==0);
    hb_rg_reset(&r);r.mode=1;assert(hb_rg_advance(&r,0,1)==0);assert(hb_rg_advance(&r,4,1)<0&&r.done==1);
    assert(hb_rg_advance(&r,5,0)<0&&!r.active);r.mode=2;
    assert(hb_rg_advance(&r,1,1)==0);assert(hb_rg_advance(&r,4,1)==8&&r.done==2);
    assert(hb_rg_advance(&r,4.1,1)<0);hb_rg_advance(&r,4.2,0);assert(!r.done);
    r.mode=0;hb_rg_advance(&r,0,1);assert(hb_rg_advance(&r,400.5,1)<0); /* no catch-up burst */
}
static void playback(void){
    Inst *i=fixture();API.set_param(i,"role","Conductor");API.set_param(i,"source_channel","1");API.set_param(i,"auto_chord","Off");
    API.set_param(i,"render_rhythm_mode","Off");API.set_param(i,"rhythm_gen_pattern","Straight");API.set_param(i,"rhythm_gen_span","1 Bar");
    uint8_t on[]={0x90,60,100},off[]={0x80,60,0},out[256][3];int sizes[256];
    int attacks=0,releases=0;
    for(int k=0;k<64;k++){
        position=k/16.0;int n=k?API.tick(i,1500,48000,out,sizes,256):API.process_midi(i,on,3,out,sizes,256);
        for(int j=0;j<n;j++){if((out[j][0]&240)==0x90&&out[j][2])attacks++;if((out[j][0]&240)==0x80)releases++;}
    }
    assert(attacks==8&&releases==8);position=4;API.process_midi(i,off,3,out,sizes,256);API.tick(i,0,48000,out,sizes,256);assert(!i->player.sounding_count);
    API.set_param(i,"rhythm_gen_edit","1");API.set_param(i,"rhythm_gen_step","2");API.set_param(i,"rhythm_gen_kind","Tie");
    Inst *other=API.create_instance("",0);char value[64];API.set_param(other,"rhythm_gen_step","2");API.get_param(other,"rhythm_gen_kind",value,sizeof(value));assert(!strcmp(value,"Tie"));
    char saved[16384];assert(API.get_param(i,"state",saved,sizeof(saved))>0);
    API.set_param(i,"rhythm_gen_pattern","Off");API.set_param(i,"state",saved);assert(i->player.rhythm.pattern==1);
    API.destroy_instance(other);API.destroy_instance(i);
    i=API.create_instance("",0);API.set_param(i,"state",saved);API.set_param(i,"rhythm_gen_step","2");API.get_param(i,"rhythm_gen_kind",value,sizeof(value));assert(!strcmp(value,"Tie"));
    API.set_param(i,"role","Conductor");API.set_param(i,"source_channel","1");position=4;API.tick(i,0,48000,out,sizes,256);API.process_midi(i,on,3,out,sizes,256);API.tick(i,0,48000,out,sizes,256);assert(i->player.sounding_count);
    API.set_param(i,"rhythm_gen_pattern","Off");API.tick(i,0,48000,out,sizes,256);assert(!i->player.sounding_count);
    API.destroy_instance(i);
    i=fixture();API.set_param(i,"role","Conductor");API.set_param(i,"source_channel","1");API.set_param(i,"rhythm_gen_pattern","Offbeats");position=0;API.tick(i,0,48000,out,sizes,256);
    char actions[256]="60";for(int lane=0;lane<HB_MOTION_LANES;lane++)strcat(actions,",0");strcat(actions,",128");
    API.set_param(i,"hb_movy_actions",actions);assert(i->recorded_action_valid[60]);API.set_param(i,"hb_movy_playback","1");
    API.process_midi(i,on,3,out,sizes,256);API.tick(i,0,48000,out,sizes,256);assert(i->player.keys[0].rhythm_baked&&i->player.sounding_count==1);
    API.process_midi(i,off,3,out,sizes,256);API.tick(i,0,48000,out,sizes,256);assert(!i->player.sounding_count);API.destroy_instance(i);
}
static void fills(void){
    hb_mt_phrase source={.count=3,.anchor=2};for(int k=0;k<3;k++){source.events[k].count=1;source.events[k].notes[0]=(hb_mt_note){60+k,100};source.events[k].duration=24;}
    hb_rg_clock r={.pattern=5,.span=2};hb_mt_phrase out;double gates[34];
    assert(hb_rg_motif(&source,&r,3,&out,gates));assert(out.anchor==8&&out.count==9);
    double arrival=0;for(int k=0;k<out.anchor;k++)arrival+=out.durations[k];assert(arrival==3&&out.events[8].notes[0].pitch==62);
    r.source=1;assert(hb_rg_motif(&source,&r,3,&out,gates));for(int k=0;k<8;k++)if(out.events[k].count)assert(out.events[k].notes[0].pitch==62);
    hb_mc_plan plan={.count=3,.target_first=-1,.target_last=2};for(int k=0;k<3;k++)plan.events[k]=(hb_mc_event){.event=source.events[k],.duration=24,.source_step=k};
    assert(hb_rg_clip(&plan,&r,0));assert(plan.target_last>=0&&plan.events[plan.target_last].onset==96);assert(plan.duration==120);
}
static void ownership(void){
    hb_chord_player p={0};hb_cp_defaults(&p.config);p.rhythm.pattern=1;p.rhythm.span=2;p.rhythm.mode=1;
    int notes[]={60,64,67};uint8_t out[128][3];int sizes[128];
    hb_cp_on_origin(&p,60,0,100,notes,3,0);assert(hb_cp_tick(&p,out,sizes,128)==3);
    p.beat=.25;assert(hb_cp_tick(&p,out,sizes,128)==3);p.beat=4;assert(hb_cp_tick(&p,out,sizes,128)==0&&p.rhythm.done==1);
    hb_cp_off_origin(&p,60,0,0);hb_cp_tick(&p,out,sizes,128);assert(!p.rhythm.active);
    p.rhythm.mode=2;p.config.playback=1;p.config.order=0;p.config.start=0;p.beat=5;
    hb_cp_on_origin(&p,60,0,100,notes,3,0);assert(hb_cp_tick(&p,out,sizes,128)==1);assert(out[0][1]==60);
    p.beat=5.375;hb_cp_tick(&p,out,sizes,128);assert(p.arp_note==64);
    p.beat=8;hb_cp_tick(&p,out,sizes,128);assert(p.rhythm.done==2&&p.sounding_count==3);
    hb_cp_on_origin(&p,60,0,100,notes,3,1);hb_cp_off_origin(&p,60,0,0);assert(hb_cp_held(&p)==1);
    hb_cp_clear(&p);hb_cp_tick(&p,out,sizes,128);assert(!p.sounding_count);
    p.rhythm.pattern=2;p.rhythm.mode=0;p.config.playback=0;p.beat=9;
    hb_cp_on_origin(&p,60,0,100,notes,3,1);p.keys[0].rhythm_baked=1;
    assert(hb_cp_tick(&p,out,sizes,128)==3);assert(!p.rhythm.active);
    p.beat=9.1;hb_cp_off_origin(&p,60,0,1);hb_cp_tick(&p,out,sizes,128);assert(!p.sounding_count);
}
static void gate_transitions(void){
    hb_chord_player p={0};hb_cp_defaults(&p.config);p.rhythm.pattern=1;p.rhythm.span=2;
    int note=60;uint8_t out[128][3];int sizes[128];
    hb_cp_on(&p,60,0,100,&note,1);hb_cp_tick(&p,out,sizes,128);assert(p.sounding[0][60]);
    p.beat=.1;p.keys[0].notes[0]=64;assert(hb_cp_tick(&p,out,sizes,128)==0&&p.sounding[0][60]);
    hb_cp_off(&p,60,0);assert(hb_cp_tick(&p,out,sizes,128)==0);
    note=67;hb_cp_on(&p,60,0,90,&note,1);assert(hb_cp_tick(&p,out,sizes,128)==0&&p.sounding[0][60]);
    p.beat=.25;hb_cp_tick(&p,out,sizes,128);assert(!p.sounding_count);
    p.beat=.5;hb_cp_tick(&p,out,sizes,128);assert(p.sounding[0][67]);
    p.beat=.6;hb_cp_off(&p,60,0);assert(hb_cp_tick(&p,out,sizes,128)==0);
    p.beat=.75;hb_cp_tick(&p,out,sizes,128);assert(!p.sounding_count);
    p.beat=1;hb_cp_tick(&p,out,sizes,128);assert(!p.sounding_count&&!p.rhythm.active);
    p.config.latch=1;note=60;hb_cp_on(&p,60,0,100,&note,1);hb_cp_tick(&p,out,sizes,128);
    p.beat=1.1;hb_cp_off(&p,60,0);note=64;hb_cp_on(&p,64,0,100,&note,1);assert(hb_cp_tick(&p,out,sizes,128)==0);
    assert(p.sounding[0][60]&&!p.sounding[0][64]);
    p.beat=1.5;hb_cp_tick(&p,out,sizes,128);assert(p.sounding[0][64]&&!p.sounding[0][60]);
    hb_cp_off(&p,64,0);p.beat=2;hb_cp_tick(&p,out,sizes,128);assert(p.sounding[0][64]);
    hb_cp_clear(&p);hb_cp_tick(&p,out,sizes,128);assert(!p.sounding_count);
}
static void written_arp(void){
    for(int pattern=0;pattern<=2;pattern+=2)for(int order=0;order<7;order++){
        hb_chord_player p={0};hb_cp_defaults(&p.config);p.config.playback=1;p.config.start=0;p.config.order=order;p.config.latch=1;p.rhythm.pattern=pattern;
        int notes[]={60,64,67};int expected[][4]={{60,64,67,60},{67,64,60,67},{60,64,67,64}};
        uint8_t out[128][3];int sizes[128];int visited=0;
        for(int attack=0;attack<4;attack++){
            p.beat=attack*.7;hb_cp_on_origin(&p,60,0,90,notes,3,1);
            for(int k=0;k<HB_CP_KEYS;k++)if(p.keys[k].used)p.keys[k].rhythm_baked=1;
            int count=hb_cp_tick(&p,out,sizes,128);assert(count==1&&(out[0][0]&240)==0x90);
            if(order<3)assert(out[0][1]==expected[order][attack]);
            if(order>=5&&attack<3){int bit=1<<(out[0][1]==60?0:out[0][1]==64?1:2);assert(!(visited&bit));visited|=bit;}
            p.beat+=.05;assert(hb_cp_tick(&p,out,sizes,128)==0); /* no second arp clock */
            hb_cp_off_origin(&p,60,0,1);assert(hb_cp_tick(&p,out,sizes,128)==1&&!p.sounding_count);
            p.beat+=.4;assert(hb_cp_tick(&p,out,sizes,128)==0);
        }
    }
}
static void pitch_material(void){
    hb_mt_phrase source={.count=5,.anchor=4};
    for(int k=0;k<5;k++){source.events[k].count=1;source.events[k].notes[0]=(hb_mt_note){60+k,100};source.events[k].duration=24;}
    source.events[1].kind=1;source.events[2].kind=2;
    source.events[0].modifier=-1;source.events[3].secondary=2;
    source.events[3].chord_mode=1;source.events[3].actions[0]=123;
    hb_rg_clock rhythm={.pattern=1,.span=2};hb_mt_phrase phrase;double gates[34];
    assert(hb_rg_motif(&source,&rhythm,4,&phrase,gates));
    hb_mc_plan clip={.count=5,.target_first=-1,.target_last=4};
    for(int k=0;k<5;k++)clip.events[k]=(hb_mc_event){.event=source.events[k],.duration=24,.source_step=k};
    assert(hb_rg_clip(&clip,&rhythm,&source.events[4]));
    int attacks=0;for(int k=0;k<clip.target_last;k++)if(clip.events[k].event.kind==0){
        int pitch=attacks%2?63:60;
        assert(clip.events[k].event.notes[0].pitch==pitch);
        assert(phrase.events[attacks].kind==0&&phrase.events[attacks].notes[0].pitch==pitch);attacks++;
        const hb_mt_event *expected=&source.events[pitch==60?0:3];
        assert(clip.events[k].event.modifier==expected->modifier);
        assert(clip.events[k].event.secondary==expected->secondary);
        assert(phrase.events[attacks-1].chord_mode==expected->chord_mode);
        assert(phrase.events[attacks-1].actions[0]==expected->actions[0]);
    }
    assert(attacks==8&&phrase.events[phrase.anchor].notes[0].pitch==64);
}


static void euclidean_fill(void){
    /* Exact cyclic spacing, zero/all hits, and rotation for 1..16 divisions. */
    for(int length=1;length<=HB_RG_STEPS;length++)for(int hits=0;hits<=length;hits++)
    for(int rotation=0;rotation<HB_RG_STEPS;rotation++){
        hb_rg_pattern pattern={.count=length};
        for(int index=0;index<length;index++)
            pattern.steps[index]=(hb_rg_step){2,(unsigned char)(60+index),(unsigned char)(20+index)};
        hb_rg_euclidean_fill(&pattern,length,hits,rotation);
        int attack_count=0,positions[HB_RG_STEPS];
        for(int index=0;index<length;index++){
            assert(pattern.steps[index].kind<=1);
            assert(pattern.steps[index].accent==60+index&&pattern.steps[index].gate==20+index);
            if(pattern.steps[index].kind)positions[attack_count++]=index;
        }
        assert(attack_count==hits);
        if(hits>0&&hits<length)for(int index=0;index<hits;index++){
            int next=positions[(index+1)%hits]+(index+1==hits?length:0);
            int distance=next-positions[index];
            assert(distance==length/hits||distance==(length+hits-1)/hits);
        }
    }
    Inst *instance=fixture();
    char value[128];
    API.set_param(instance,"rhythm_gen_edit","3");
    API.set_param(instance,"rhythm_gen_step","2");
    API.set_param(instance,"rhythm_gen_euclid_steps","8");
    API.set_param(instance,"rhythm_gen_euclid_hits","3");
    API.set_param(instance,"rhythm_gen_euclid_rotation","1");
    assert(API.get_param(instance,"rhythm_gen_euclid_preview",value,sizeof(value))>0);
    assert(!strcmp(value,".X..X..X"));
    API.set_param(instance,"rhythm_gen_euclid_steps","13");
    assert(g_rg_users[2].count==8); /* changing draft length must not edit live slots */
    API.set_param(instance,"rhythm_gen_euclid_steps","8");
    /* Changes remain a harmless preview until Apply is explicitly selected. */
    API.get_param(instance,"rhythm_gen_kind",value,sizeof(value));
    assert(!strcmp(value,"Attack"));
    API.set_param(instance,"rhythm_gen_euclid_apply","Apply");
    API.get_param(instance,"rhythm_gen_euclid_apply",value,sizeof(value));
    assert(!strcmp(value,"Ready")); /* one shot; can be applied again */
    API.get_param(instance,"rhythm_gen_kind",value,sizeof(value));
    assert(!strcmp(value,"Attack"));
    API.set_param(instance,"rhythm_gen_step","1");
    API.get_param(instance,"rhythm_gen_kind",value,sizeof(value));
    assert(!strcmp(value,"Rest"));
    API.get_param(instance,"rhythm_gen_accent",value,sizeof(value));
    assert(!strcmp(value,"100")); /* nonrhythmic expression is unchanged */
    /* User pattern is shared across tracks, and state persists on reload. */
    Inst *other=API.create_instance("",0);
    API.set_param(other,"rhythm_gen_edit","3");
    API.set_param(other,"rhythm_gen_step","1");
    API.get_param(other,"rhythm_gen_kind",value,sizeof(value));
    assert(!strcmp(value,"Rest"));
    char saved[16384];assert(API.get_param(instance,"state",saved,sizeof(saved))>0);
    assert(strstr(saved,";eg1,8,3,1"));
    API.set_param(instance,"rhythm_gen_euclid_hits","0");
    API.get_param(instance,"rhythm_gen_euclid_preview",value,sizeof(value));
    assert(!strcmp(value,"........"));
    API.set_param(instance,"state",saved);
    API.get_param(instance,"rhythm_gen_euclid_steps",value,sizeof(value));assert(!strcmp(value,"8"));
    API.get_param(instance,"rhythm_gen_euclid_hits",value,sizeof(value));assert(!strcmp(value,"3"));
    API.get_param(instance,"rhythm_gen_euclid_rotation",value,sizeof(value));assert(!strcmp(value,"1"));
    API.destroy_instance(other);API.destroy_instance(instance);
}

int main(void){patterns();playback();fills();ownership();gate_transitions();written_arp();pitch_material();euclidean_fill();puts("Rhythm generation: patterns, bounded clock, shared edits, Euclidean fill, state, clip and target fills pass");}
