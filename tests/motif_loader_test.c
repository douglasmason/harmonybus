#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"
static void export_plan(void){
    Inst *instance=setup();char header[1024],event[2048];
    API.set_param(instance,"motif_load_choice","ii-V-Target");
    API.set_param(instance,"motif_load_target","62");
    API.set_param(instance,"motif_placement","Both");
    API.set_param(instance,"motif_load_prepare","Preview");
    API.get_param(instance,"motif_load_header",header,sizeof(header));
    unsigned serial;int duration,count,root,scale,transpose;
    assert(sscanf(header,"ml1,%u,%d,%d,%d,%d,%d",&serial,&duration,&count,&root,&scale,&transpose)==6);
    assert(duration==1536&&count==4&&root==0&&scale==1);
    for(int index=0;index<count;index++){
        char key[80];snprintf(key,sizeof(key),"motif_load_event_%u_%d",serial,index);
        API.get_param(instance,key,event,sizeof(event));int tick,gate,pitch,velocity;
        assert(sscanf(event,"%d,%d,%d.%d:",&tick,&gate,&pitch,&velocity)==4);
        assert(tick==index*384&&gate==384&&pitch==62&&velocity==100);
        assert(strchr(event,':'));
    }
    assert(g_ml_plan.events[1].event.secondary==1&&g_ml_plan.events[2].event.secondary==2);
    /* These exported input notes still ask the real renderer for ii and V.
       Auto Chord can change their voicing after the clip has been written. */
    instance->player.config.mode=1;instance->player.config.size=3;
    API.set_param(instance,"chord_form","Triad");
    instance->movy_playback=1;
    const unsigned expected[]={ (1u<<4)|(1u<<7)|(1u<<11), (1u<<9)|(1u<<1)|(1u<<4) };
    for(int step=1;step<=2;step++){
        instance->recorded_action_valid[62]=1;
        memcpy(instance->recorded_actions[62],g_ml_plan.events[step].event.actions,sizeof(instance->recorded_actions[62]));
        assert(played(instance,62)==expected[step-1]);release(instance,62);
    }
    instance->movy_playback=0;
    API.set_param(instance,"motif_load_collection","Parallel");
    API.set_param(instance,"parallel_scale","Natural Minor");
    API.set_param(instance,"motif_load_prepare","Preview");
    assert(((g_ml_plan.events[1].event.actions[HB_MOTION_LANES]>>36)&31)==2);
    assert(!g_parallel_on); /* Explicit import does not arm a live operation. */
    API.set_param(instance,"motif_placement","End");API.set_param(instance,"motif_load_even","On");
    API.set_param(instance,"motif_load_prepare","Preview");
    assert(g_ml_plan.count==3&&g_ml_plan.duration==384&&g_ml_plan.events[2].duration==192);
    API.set_param(instance,"motif_preset","ii-V-Target");
    hb_mt_phrase live;hb_mt_selected(instance,&live);
    assert(live.count==3&&hb_mt_duration(&live,2,0,0)==2);
    API.set_param(instance,"motif_placement","Omit");API.set_param(instance,"motif_load_choice","V-Target");
    API.set_param(instance,"motif_load_prepare","Preview");
    assert(g_ml_plan.count==2&&g_ml_plan.events[1].event.kind==1);
    API.set_param(instance,"motif_load_choice","User 16");API.set_param(instance,"motif_load_prepare","Preview");
    API.get_param(instance,"motif_load_header",header,sizeof(header));assert(strstr(header,"Empty motif"));
    char saved[256];int used=hb_ml_save(instance,saved,sizeof(saved),0);assert(used>0);
    instance->motif_load[1]=1;hb_ml_restore(instance,saved);assert(instance->motif_load[1]==62);
    hb_ml_restore(instance,";ml1,0,999,0,12,0,0");assert(instance->motif_load[1]==62);
    hb_mt_preset_definition(1,&g_motifs[0]);
    hb_mt_event *captured=&g_motifs[0].body.events[0];
    captured->secondary=0;captured->modifier=-1;
    captured->actions[HB_MOTION_LANES]=(1ULL<<20)|(1ULL<<21)|(3ULL<<27)|(7ULL<<32)|(5ULL<<36);
    API.set_param(instance,"motif_load_choice","User 1");API.set_param(instance,"motif_load_collection","Parent");
    API.set_param(instance,"motif_load_prepare","Preview");
    unsigned long long intent=g_ml_plan.events[0].event.actions[HB_MOTION_LANES];
    assert((intent&3)==1&&((intent>>32)&15)==0&&((intent>>36)&31)==1&&((intent>>27)&31)==3);
    memset(g_motifs,0,sizeof(g_motifs));
    API.destroy_instance(instance);
}
static void scrub_all_edges(void){
    hb_mt_event target={.duration=24,.count=1,.chord_mode=3,.scale=0xAB5,.notes={{60,100}}};
    hb_mt_phrase phrase={.count=7,.anchor=6};for(int i=0;i<7;i++)phrase.events[i]=target;
    phrase.events[2].secondary=1;phrase.events[4].secondary=2;
    hb_mt_definition definition;assert(hb_mt_normalize(&phrase,&definition));
    assert(definition.body.count==3&&definition.body.events[1].secondary==0);
    assert(definition.target_start.duration==48&&definition.target_end.duration==48);
    hb_mt_phrase view;g_motifs[0]=definition;hb_mt_library_view(0,&view);assert(view.count==7);
    for(int i=0;i<7;i++)assert(view.events[i].duration==24);
    unsigned tokens[HB_MT_STEPS+2];assert(hb_ar_live_steps(35,4,1,tokens)==3);
    memset(g_motifs,0,sizeof(g_motifs));
}
int main(void){scrub_all_edges();export_plan();puts("motif loader: intent export, target placement, parallel borrowing, padding, state and complete boundary normalization pass");}
