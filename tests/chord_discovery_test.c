#define main ignored_player_main
#include "chord_player_test.c"
#undef main

static void probe(Inst *owner,int variant,const char *selection){
    char request[256],response[512];snprintf(request,sizeof(request),"0,%d;%s",variant,selection);
    API.set_param(owner,"hb_chord_discovery_probe",request);
    assert(API.get_param(owner,"hb_chord_discovery_probe",response,sizeof(response))>0);
    assert(!strncmp(response,"cd1|",4)&&owner->discovery_count>0);
}
int main(void){
    Inst *conductor=fixture();conductor->movy_track=0;
    API.set_param(conductor,"role","Conductor");API.set_param(conductor,"chord_form","Triad");
    API.set_param(conductor,"chord_register","0");
    const char *selection="0,1,2,1,0,1,12,0,0,0";
    unsigned long long pending=conductor->selection_pending;
    unsigned actions=conductor->action_count;hb_chord_queue queue=g_chord_queue;
    char before[65536],after[65536];API.get_param(conductor,"state",before,sizeof(before));
    hb_harmony_t harmony=hb_render_harmony(conductor);
    for(int variant=0;variant<7;variant++){
        probe(conductor,variant,selection);
        assert(conductor->selection_pending==pending&&conductor->action_count==actions);
        assert(!memcmp(&queue,&g_chord_queue,sizeof(queue)));
        API.set_param(conductor,"hb_chord_discovery_audition","on");
        assert(conductor->motion_local.owned>0&&conductor->motion_render.owned>0);
        advance(conductor,1,64);
        assert(recorded_count==0&&!hb_selected_harmony(conductor).valid);
        assert(hb_render_harmony(conductor).root_pc==harmony.root_pc);
        API.set_param(conductor,"hb_chord_discovery_audition","off");advance(conductor,1,64);
        assert(!conductor->motion_local.owned&&!conductor->motion_render.owned);
    }
    API.get_param(conductor,"state",after,sizeof(after));assert(!strcmp(before,after));
    /* An audition and a performance may own the same pitch independently. */
    probe(conductor,1,selection);int pitch=conductor->discovery_notes[0];
    uint8_t attack[3]={0x90,(uint8_t)pitch,100},release[3]={0x80,(uint8_t)pitch,0};
    hb_mo_event_owned(&conductor->motion_local,attack,pitch,100,-1,-1,0,42);
    API.set_param(conductor,"hb_chord_discovery_audition","on");advance(conductor,1,64);
    API.set_param(conductor,"hb_chord_discovery_audition","off");advance(conductor,1,64);
    assert(conductor->motion_local.refs[0][pitch]==1&&conductor->motion_local.owned==1);
    hb_mo_event_owned(&conductor->motion_local,release,pitch,0,-1,-1,0,42);advance(conductor,1,64);
    assert(!conductor->motion_local.owned);
    /* A bad request cannot replay stale notes. Stop/panic owns all cleanup. */
    API.set_param(conductor,"hb_chord_discovery_probe","0,7;bad");
    API.set_param(conductor,"hb_chord_discovery_audition","on");assert(!conductor->motion_local.owned);
    probe(conductor,0,selection);API.set_param(conductor,"hb_chord_discovery_audition","on");
    hb_stop_instance_note_state(conductor);assert(!conductor->motion_local.owned&&!conductor->motion_render.owned);
    Inst *comp=API.create_instance("",0);comp->movy_track=2;API.set_param(comp,"role","Follower");
    API.set_param(comp,"chord_form","Ninth");API.set_param(comp,"chord_register","Top 79");
    probe(comp,4,"0,0,3,3,0,1,12,3,1,0");
    assert(comp->discovery_notes[comp->discovery_count-1]<=79);
    assert(conductor->selection_pending==pending&&recorded_count==0);
    API.destroy_instance(comp);API.destroy_instance(conductor);
    puts("Chord discovery: native voicings, non-recording audition, shared gates, invalid requests and lifecycle pass");
    return 0;
}
