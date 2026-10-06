/* Live input and clip playback own their notes independently. */
#define main chord_player_suite_main
#include "chord_player_test.c"
#undef main

static uint8_t heard[128];
static void hear(int count){
    for(int i=0;i<count;i++){
        int type=output[i][0]&240,pitch=output[i][1];
        if(type==0x90&&output[i][2])heard[pitch]=1;
        else if(type==0x80||type==0x90)heard[pitch]=0;
    }
}
static void input_origin(Inst *i,int recorded,int on,int pitch){
    i->movy_playback=recorded;
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)pitch,(uint8_t)(on?100:0)};
    hear(API.process_midi(i,message,3,output,lengths,64));
    i->movy_playback=0;
    hear(advance(i,1,64));
}
static void overlapping_input(int chord,int separate_outputs,int release_clip_first){
    Inst *i=fixture();memset(heard,0,sizeof(heard));
    API.set_param(i,"boundary_buffer_ms","0");API.set_param(i,"render_channel","Off");
    if(chord)API.set_param(i,"chord_mode","Follow Chord");
    if(separate_outputs){API.set_param(i,"play_scope","Clip");API.set_param(i,"play_octave","1");}
    input_origin(i,1,1,60);
    uint8_t clip[128];memcpy(clip,heard,sizeof(clip));
    assert(clip[separate_outputs?72:60]);
    input_origin(i,0,1,60);
    for(int p=0;p<128;p++)if(clip[p])assert(heard[p]);
    input_origin(i,release_clip_first,0,60);
    if(!release_clip_first){for(int p=0;p<128;p++)if(clip[p])assert(heard[p]);}
    else assert(heard[60]);
    input_origin(i,!release_clip_first,0,60);
    for(int p=0;p<128;p++)assert(!heard[p]);
    API.destroy_instance(i);
}
static void held_chord_change(int chord,int retrigger){
    Inst *i=fixture();memset(heard,0,sizeof(heard));
    API.set_param(i,"boundary_buffer_ms","0");API.set_param(i,"render_channel","Off");
    API.set_param(i,"retrigger_held",retrigger?"On":"Off");
    if(chord)API.set_param(i,"chord_mode","Follow Chord");
    API.set_param(i,"play_scope","Clip");API.set_param(i,"play_octave","1");
    input_origin(i,1,1,60);input_origin(i,0,1,60);
    uint8_t before[128];memcpy(before,heard,sizeof(before));
    uint8_t d_major[3]={62,66,69};hb_commit_observed_harmony(hb_infer_harmony(d_major,3));
    hear(advance(i,1,64));
    assert(i->retrigger_held==retrigger);
    if(retrigger){assert(heard[62]&&heard[74]);assert(!heard[60]&&!heard[72]);}
    else assert(!memcmp(before,heard,sizeof(before)));
    input_origin(i,0,0,60);assert(heard[retrigger?74:72]);
    input_origin(i,1,0,60);
    for(int p=0;p<128;p++)assert(!heard[p]);
    API.destroy_instance(i);
}
static void latch_ownership(void){
    for(int latch=1;latch<=5;latch++){
        hb_chord_player player={0};hb_cp_defaults(&player.config);player.config.latch=latch;
        int clip=72,live=60,next=64;
        assert(hb_cp_on_origin(&player,60,0,100,&clip,1,1));
        assert(hb_cp_on_origin(&player,60,0,100,&live,1,0));
        hb_cp_off_origin(&player,60,0,0);
        assert(hb_cp_on_origin(&player,64,0,100,&next,1,0));
        int found=0;
        for(int k=0;k<HB_CP_KEYS;k++)if(player.keys[k].used&&player.keys[k].playback_origin){
            assert(player.keys[k].held&&player.keys[k].source==60&&player.keys[k].notes[0]==72);found++;
        }
        assert(found==1);
    }
}
static void motion_overlap(void){
    Inst *i=fixture();memset(heard,0,sizeof(heard));
    API.set_param(i,"boundary_buffer_ms","0");API.set_param(i,"render_channel","Off");
    API.set_param(i,"motion_lane","1");API.set_param(i,"motion_operation","Octave");API.set_param(i,"motion_amount","1");
    input_origin(i,1,1,60);assert(heard[72]);
    API.set_param(i,"motion_operation","Off");
    input_origin(i,0,1,60);assert(heard[60]&&heard[72]);
    input_origin(i,0,0,60);assert(!heard[60]&&heard[72]);
    input_origin(i,1,0,60);assert(!heard[72]);
    assert(!i->motion_local.owned);
    API.destroy_instance(i);
}
static void conductor_overlap(int chord,int baked){
    Inst *i=fixture();memset(heard,0,sizeof(heard));
    API.set_param(i,"role","Conductor");API.set_param(i,"render_channel","Off");
    if(chord)API.set_param(i,"chord_mode","Scale Degree");
    i->movy_passthrough=baked;input_origin(i,1,1,60);i->movy_passthrough=0;
    assert(heard[60]);input_origin(i,0,1,60);input_origin(i,0,0,60);assert(heard[60]);
    i->movy_passthrough=baked;input_origin(i,1,0,60);
    for(int p=0;p<128;p++)assert(!heard[p]);
    API.destroy_instance(i);
}
static void rhythm_overlap(void){
    hb_motion_route r;hb_mo_route_init(&r);r.rhythm_enabled=1;
    uint8_t on[3]={0x90,60,100},off[3]={0x80,60,0},out[3];
    for(int first=0;first<2;first++){
        r.rhythm_now=0;r.rhythm_delay=0;
        assert(hb_mo_event_owned(&r,on,60,100,-1,-1,0,1));
        assert(hb_mo_pop(&r,out)&&out[0]==0x90);
        r.rhythm_now=.1;r.rhythm_delay=.2;
        assert(hb_mo_event_owned(&r,on,60,100,-1,-1,0,2));
        r.rhythm_now=.3;assert(!hb_mo_pop(&r,out));
        assert(hb_mo_event_owned(&r,off,60,0,-1,-1,0,first?1:2));
        r.rhythm_now=.5;assert(!hb_mo_pop(&r,out));
        assert(hb_mo_event_owned(&r,off,60,0,-1,-1,0,first?2:1));
        r.rhythm_now=.7;assert(hb_mo_pop(&r,out)&&out[0]==0x80);
        assert(!hb_mo_pop(&r,out)&&!r.owned&&!hb_rr_active(&r.rhythm));
    }
}
static void queued_intent(void){
    Inst *i=fixture();API.set_param(i,"boundary_buffer_ms","0");
    API.set_param(i,"travel_map","Closest Split Chromatic");API.set_param(i,"content_map","Scale");
    i->movy_playback=1;API.set_param(i,"hb_movy_input_role","66,3,67");
    int clip=hb_map_follower_note_now(i,66);
    midi(i,1,66);i->movy_playback=0;midi(i,1,66);advance(i,1,64);
    hb_follower_voice *recorded=hb_fv_find(i->follower_voices,66,0,1,0);
    hb_follower_voice *live=hb_fv_find(i->follower_voices,66,0,0,0);
    assert(recorded&&live&&recorded->pitch==clip&&recorded->input.target==68&&!live->input.target);
    i->retrigger_held=1;
    uint8_t d_major[3]={62,66,69};hb_commit_observed_harmony(hb_infer_harmony(d_major,3));
    hb_input_intent saved=hb_input_get(i,66);hb_input_set(i,66,recorded->input);i->movy_playback=1;
    clip=hb_map_follower_note_now(i,66);hb_input_set(i,66,saved);i->movy_playback=0;
    advance(i,1,64);assert(recorded->pitch==clip&&recorded->input.target==68&&!live->input.target);
    input_origin(i,0,0,66);assert(recorded->used);input_origin(i,1,0,66);
    assert(!i->motion_local.owned);API.destroy_instance(i);
}
static void pressure_and_pairs(void){
    Inst *i=fixture();API.set_param(i,"arp_playback","Repeat Arp");
    input_origin(i,1,1,60);input_origin(i,0,1,60);
    uint8_t pressure[3]={0xA0,60,47};API.process_midi(i,pressure,3,output,lengths,64);
    int owners=0;
    for(int k=0;k<HB_CP_KEYS;k++)if(i->player.keys[k].used){
        assert(i->player.keys[k].velocity==(i->player.keys[k].playback_origin?100:47));owners++;
    }
    assert(owners==2);API.destroy_instance(i);
    i=fixture();API.set_param(i,"motion_lane","1");API.set_param(i,"motion_operation","Chord/Arp State");
    API.set_param(i,"motion_enabled","Off");API.set_param(i,"motion_amount","Scale Degree Burst");
    API.set_param(i,"chord_edit_target","Lane 1");API.set_param(i,"chord_input","Root/Bass + Top");
    API.set_param(i,"motion_hold_1","On");assert(i->chord_pair_input);
    input_origin(i,1,1,60);input_origin(i,1,1,67);
    input_origin(i,0,1,60);input_origin(i,0,1,64);input_origin(i,0,0,64);
    owners=0;
    for(int k=0;k<HB_CP_KEYS;k++)if(i->player.keys[k].used){
        assert(i->player.keys[k].playback_origin&&i->player.keys[k].held);owners++;
    }
    assert(owners==1);API.destroy_instance(i);
}
static void rendered_overlap(void){
    Inst *i=fixture();API.set_param(i,"boundary_buffer_ms","0");
    input_origin(i,1,1,60);assert(render_count==1);
    input_origin(i,0,1,60);input_origin(i,0,0,60);assert(render_count==1);
    input_origin(i,1,0,60);assert(render_count==2&&rendered[1][1]==0x83&&rendered[1][2]==60);
    API.destroy_instance(i);
}
int main(void){
    for(int chord=0;chord<2;chord++)for(int separate=0;separate<2;separate++)
        for(int first=0;first<2;first++)overlapping_input(chord,separate,first);
    for(int chord=0;chord<2;chord++)for(int retrigger=0;retrigger<2;retrigger++)held_chord_change(chord,retrigger);
    latch_ownership();motion_overlap();rhythm_overlap();queued_intent();pressure_and_pairs();rendered_overlap();
    for(int chord=0;chord<2;chord++)for(int baked=0;baked<2;baked++)conductor_overlap(chord,baked);
    puts("live overdub: clip/live ownership, output collisions, latch, motion, release order and Held Retrigger pass");
}
