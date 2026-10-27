#define main original_player_suite
#include "chord_player_test.c"
#undef main

static void capture_context(Inst *owner,int mode){
    char request[160],response[640];snprintf(request,sizeof(request),"0,%d;2,2,2,1,0,1,12,0,0,0",mode);
    API.set_param(owner,"hb_chord_discovery_context",request);
    assert(API.get_param(owner,"hb_chord_discovery_context",response,sizeof(response))>0);
    assert(!strncmp(response,"dc1|",4)&&owner->discovery_anchor.valid);
}
static void probe_context(Inst *owner,int anchor,int variant){
    char request[160];snprintf(request,sizeof(request),"c2,%u,%d,%d;0,0,2,3,0,1,12,1,3,0",owner->discovery_epoch,variant,anchor);
    API.set_param(owner,"hb_chord_discovery_probe",request);assert(owner->discovery_count>0);
}
int main(void){
    Inst *conductor=fixture();conductor->movy_track=0;API.set_param(conductor,"role","Conductor");
    API.set_param(conductor,"chord_register","0");API.set_param(conductor,"chord_form","Triad");
    char state_before[65536],state_after[65536],frozen[160];
    API.get_param(conductor,"state",state_before,sizeof(state_before));
    capture_context(conductor,0);assert(conductor->discovery_anchor.root_pc==0);
    assert(strstr(conductor->discovery_context_text,"|Current|")&&conductor->discovery_anchor.intent_scale);
    capture_context(conductor,2);assert(conductor->discovery_anchor.root_pc==2&&strstr(conductor->discovery_context_text,"|Edit|"));
    API.get_param(conductor,"state",state_after,sizeof(state_after));assert(!strcmp(state_before,state_after));
    API.set_param(conductor,"chord_queue_depth","Touch Phrase");
    API.set_param(conductor,"hb_chord_queue","0,1,2,1,0,1,12,0,0,0");
    API.set_param(conductor,"hb_chord_queue","7,1,2,1,0,1,12,0,0,0");
    API.set_param(conductor,"hb_chord_queue","5,1,2,1,0,1,12,0,0,84");
    hb_chord_queue queue_before=g_chord_queue;
    capture_context(conductor,1);assert(conductor->discovery_anchor.root_pc==5&&strstr(conductor->discovery_context_text,"|Queue|"));
    assert(hb_selection_top(conductor->discovery_word)==76);
    probe_context(conductor,1,0);assert(strstr(conductor->discovery_text,"cd1|F|"));
    for(int index=0;index<conductor->discovery_count;index++)assert(conductor->discovery_notes[index]<=76);
    assert(!memcmp(&queue_before,&g_chord_queue,sizeof(queue_before))&&recorded_count==0);
    /* Moving playback cannot rewrite the captured anchor or its voicing. */
    unsigned epoch=conductor->discovery_epoch;
    g_chord_queue.count=1;probe_context(conductor,1,0);assert(strstr(conductor->discovery_text,"cd1|F|"));
    int used=snprintf(frozen,sizeof(frozen),"notes:");
    for(int index=0;index<conductor->discovery_count;index++)used+=snprintf(frozen+used,sizeof(frozen)-(size_t)used,"%s%d",index?",":"",conductor->discovery_notes[index]);
    API.set_param(conductor,"master_transpose","D");
    char request[160];snprintf(request,sizeof(request),"c2,%u,0,1;0,0,2,3,0,1,12,1,3,0",epoch);
    API.set_param(conductor,"hb_chord_discovery_probe",request);assert(!conductor->discovery_count);
    API.set_param(conductor,"hb_chord_discovery_audition",frozen);assert(conductor->motion_local.owned>0);
    API.set_param(conductor,"hb_chord_discovery_audition","off");advance(conductor,1,64);assert(!conductor->motion_local.owned);
    for(int index=0;index<4;index++){
        const char *invalid[]={"notes:","notes:60,","notes:128","notes:60,bad"};
        API.set_param(conductor,"hb_chord_discovery_audition",invalid[index]);assert(!conductor->motion_local.owned&&!conductor->discovery_count);
    }
    API.set_param(conductor,"master_transpose","C");capture_context(conductor,0);
    assert(conductor->discovery_epoch!=epoch&&conductor->discovery_anchor.root_pc==0);
    API.set_param(conductor,"hb_chord_discovery_probe",request);assert(!conductor->discovery_count);
    API.set_param(conductor,"hb_chord_queue","off");
    /* Live selection is delivered through the same input bridge as Chord Play. */
    hb_selection_prepare(conductor,"7,1,2,1,0,1,12,0,0,0");midi(conductor,1,67);
    capture_context(conductor,0);assert(conductor->discovery_anchor.root_pc==7);
    assert(recorded_count==0);API.destroy_instance(conductor);
    puts("Contextual discovery: current/manual/queued anchors, waiting queue and range, frozen notes, stale contexts and saved-state isolation pass");
}
