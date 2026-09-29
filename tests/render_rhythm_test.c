#define main original_chord_suite
#include "chord_player_test.c"
#undef main
static void mapping(void){
    for(int pattern=0;pattern<6;pattern++)for(int window=1;window<=4;window+=3){
        double last=-10;
        for(int i=-384;i<=384;i++){double t=i/96.0,x=hb_rr_time(t,pattern,window);assert(x>=last-1e-9);last=x;}
        assert(hb_rr_time(0,pattern,window)==0);assert(hb_rr_time(window,pattern,window)==window);
    }
    assert(fabs(hb_rr_time(.25,2,1)-.375)<1e-9);
    assert(fabs(hb_rr_time(.25,3,1)-.125)<1e-9);
}
static void rhythm_ownership(void){
    hb_rr_route r={0};uint8_t on[]={0x90,60,100},off[]={0x80,60,0},out[3];
    hb_rr_push(&r,on,.25,.125);hb_rr_push(&r,off,.30,0);
    assert(!hb_rr_pop(&r,.374,out));assert(hb_rr_pop(&r,.375,out)&&out[0]==0x90);
    assert(!hb_rr_pop(&r,.424,out));assert(hb_rr_pop(&r,.425,out)&&out[0]==0x80);
    assert(!hb_rr_active(&r));
    hb_rr_push(&r,on,1,0);hb_rr_push(&r,on,1.01,0);hb_rr_push(&r,off,1.02,0);hb_rr_push(&r,off,1.03,0);
    assert(hb_rr_pop(&r,2,out)&&out[0]==0x90);assert(hb_rr_pop(&r,2,out)&&out[0]==0x80);assert(hb_rr_pop(&r,2,out)&&out[0]==0x90);
    assert(hb_rr_pop(&r,2,out)&&out[0]==0x80);assert(!hb_rr_pop(&r,2,out));assert(!hb_rr_active(&r));
    hb_rr_push(&r,on,3,0);assert(hb_rr_pop(&r,3,out));hb_rr_push(&r,on,3,1);hb_rr_panic(&r);
    assert(hb_rr_pop(&r,3,out)&&out[0]==0x80);assert(!hb_rr_pop(&r,4,out));
    for(int i=0;i<HB_RR_VOICES+1;i++)hb_rr_push(&r,on,5,1);
    assert(r.panic);assert(!hb_rr_pop(&r,6,out));assert(!hb_rr_active(&r));
}
static void motion_rhythm_overlap(void){
    hb_motion_route r;hb_mo_route_init(&r);r.rhythm_enabled=1;r.rhythm_now=.25;r.rhythm_delay=.125;
    uint8_t on[]={0x90,60,100},off[]={0x80,60,0},out[3];
    assert(hb_mo_event(&r,on,60,100,-1,-1,0));assert(hb_mo_event(&r,on,60,100,-1,-1,0));
    assert(hb_mo_event(&r,off,60,0,-1,-1,0));assert(hb_mo_event(&r,off,60,0,-1,-1,0));
    r.rhythm_now=.375;assert(hb_mo_pop(&r,out)&&out[0]==0x90);assert(hb_mo_pop(&r,out)&&out[0]==0x80);assert(hb_mo_pop(&r,out)&&out[0]==0x90);assert(hb_mo_pop(&r,out)&&out[0]==0x80);
    assert(!hb_mo_pop(&r,out));assert(!hb_rr_active(&r.rhythm));assert(!r.owned);
}
static void live_and_state(void){
    Inst *i=fixture();API.set_param(i,"motif_rhythm","Long-Short");
    API.set_param(i,"role","Conductor");API.set_param(i,"auto_chord","Off");
    uint8_t on[]={0x90,60,100},off[]={0x80,60,0};
    position=.25;
    int count=API.process_midi(i,on,3,output,lengths,64);assert(count==0);
    position=.375;count=API.tick(i,0,48000,output,lengths,64);assert(count>0&&output[0][0]==0x90);
    API.set_param(i,"render_rhythm_mode","Off");position=.5;
    count=API.process_midi(i,off,3,output,lengths,64);assert(count==0);
    position=.625;count=API.tick(i,0,48000,output,lengths,64);assert(count>0&&output[0][0]==0x80);
    API.set_param(i,"render_rhythm_mode","Override");API.set_param(i,"render_rhythm_pattern","Accelerate");API.set_param(i,"render_rhythm_track_window","Bar");
    char state[8192],value[64];assert(API.get_param(i,"state",state,sizeof(state))>0);
    API.set_param(i,"render_rhythm_mode","Off");API.set_param(i,"state",state);
    API.get_param(i,"render_rhythm_config",value,sizeof(value));assert(!strcmp(value,"rr1,4,1"));
    API.set_param(i,"render_rhythm_host","movy-rhythm-v1");i->movy_playback=1;position=1.25;
    count=API.process_midi(i,on,3,output,lengths,64);assert(count>0); // already warped clip
    API.destroy_instance(i);
}
int main(void){mapping();rhythm_ownership();motion_rhythm_overlap();live_and_state();puts("Render Rhythm: monotone windows, live delay, release ownership, panic, state and clip bypass pass");}
