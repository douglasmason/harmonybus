/* Feedback describes actual render results without evaluating music on reads. */
#define HB_RELATIVE_OPERATIONS_FIXTURE
#include "relative_operations_test.c"

static unsigned feedback(Inst *instance){return (unsigned)__atomic_load_n(&instance->relative_feedback_word,__ATOMIC_ACQUIRE);}
static void assert_feedback(Inst *instance,int token,int target,int root,unsigned mask,int chord,int origin){
    unsigned event=feedback(instance);
    if((event&63)!=(unsigned)token||((event>>6)&15)!=(unsigned)target||((event>>10)&15)!=(unsigned)root||((event>>14)&4095)!=mask||((event>>26)&1)!=(unsigned)chord||((event>>27)&3)!=(unsigned)origin)
        fprintf(stderr,"feedback %u token%u target%u root%u mask%x chord%u origin%u; expected %d %d %d %x %d %d\n",event,event&63,(event>>6)&15,(event>>10)&15,(event>>14)&4095,(event>>26)&1,(event>>27)&3,token,target,root,mask,chord,origin);
    assert((event&63)==(unsigned)token);assert(((event>>6)&15)==(unsigned)target);
    assert(((event>>10)&15)==(unsigned)root);assert(((event>>14)&4095)==mask);
    assert(((event>>26)&1)==(unsigned)chord);assert(((event>>27)&3)==(unsigned)origin);
}
static void live_and_recorded(void){
    for(int role=0;role<2;role++)for(int mode=0;mode<2;mode++)for(int side=0;side<2;side++)for(int high=0;high<2;high++){
        Inst *instance=approach_fixture();API.set_param(instance,"role",role?"Follower":"Conductor");API.set_param(instance,"source_channel","1");
        instance->player.config.mode=mode;
        API.set_param(instance,"motion_operation",side?"Smart Above":"Smart Below");API.set_param(instance,"motion_amount","1");API.set_param(instance,"motion_enabled","On");
        int target=high?68:65,root=high?7:6,token=high?14:60;
        /* Compare against the same onset context: a conductor's sounding
           chord can itself replace the bus harmony after this press. */
        Inst *preview=malloc(sizeof(*preview));assert(preview);memcpy(preview,instance,sizeof(*preview));
        unsigned preview_mask=hb_pad_render_mask(preview,instance,g_bus.observed_harmony,target,0,0,0);free(preview);
        note(instance,target,1);
        unsigned mask=mode?hb_transpose_mask(0x249,root):1u<<root;
        assert_feedback(instance,token,target%12,root,mask,mode,0);
        assert(preview_mask==mask);
        unsigned long long before=instance->relative_feedback_word;
        hb_motion_config motion=instance->motion;hb_chord_player player=instance->player;
        int actions=instance->action_count;
        char view[16384];API.get_param(instance,"pad_view",view,sizeof(view));
        assert(strstr(view,"|ro1,"));
        API.get_param(instance,"pad_view",view,sizeof(view));
        assert(instance->relative_feedback_word==before&&instance->action_count==actions);
        assert(!memcmp(&motion,&instance->motion,sizeof(motion))&&!memcmp(&player,&instance->player,sizeof(player)));
        unsigned long long captured[HB_MOTION_LANES+1];hb_capture_input_intent(instance,target,captured);
        note(instance,target,0);API.set_param(instance,"motion_operation","Off");
        memcpy(instance->recorded_actions[target],captured,sizeof(captured));instance->recorded_action_valid[target]=1;
        instance->movy_playback=1;note(instance,target,1);
        assert_feedback(instance,token,target%12,root,mask,mode,1);
        note(instance,target,0);instance->movy_playback=0;
        hb_stop_instance_note_state(instance);assert(!(feedback(instance)&63));
        note(instance,69,1);assert(!(feedback(instance)&63));
        API.destroy_instance(instance);
    }
}
static void live_key_and_parallel(void){
    for(int mode=0;mode<2;mode++)for(int role=0;role<2;role++)for(int parallel=0;parallel<2;parallel++)for(int high=0;high<2;high++){
        Inst *instance=approach_fixture();API.set_param(instance,"role",role?"Follower":"Conductor");API.set_param(instance,"source_channel","1");instance->player.config.mode=mode;
        API.set_param(instance,"motion_operation","Smart Below");API.set_param(instance,"motion_amount","1");API.set_param(instance,"motion_enabled","On");
        if(parallel){API.set_param(instance,"parallel_scale","Major");API.set_param(instance,"parallel_mode","Down");}
        else g_key_context=(hb_key_context){.active=1,.source_root=9,.target_root=11,.source_mask=hb_explicit_scale_mask(9,8),.target_mask=hb_explicit_scale_mask(11,8)};
        int source=high?68:65;
        Inst *preview=malloc(sizeof(*preview));assert(preview);memcpy(preview,instance,sizeof(*preview));
        unsigned preview_mask=hb_pad_render_mask(preview,instance,g_bus.observed_harmony,source,0,0,0);free(preview);
        note(instance,source,1);
        unsigned event=feedback(instance),mask=mode?sounding_mask(instance,source):((event>>14)&4095);
        assert(((event>>14)&4095)==mask&&((event>>26)&1)==(unsigned)mode);
        if(!parallel)assert_feedback(instance,high?14:60,high?10:7,high?9:8,mode?hb_transpose_mask(0x249,high?9:8):1u<<(high?9:8),mode,0);
        else assert((event&63)==14||(event&63)==60);
        assert(preview_mask==mask);
        API.destroy_instance(instance);
    }
}
static void motif_onsets(void){
    Inst *instance=approach_fixture();API.set_param(instance,"role","Conductor");
    g_key_context=(hb_key_context){.active=1,.source_root=9,.target_root=11,.source_mask=hb_explicit_scale_mask(9,8),.target_mask=hb_explicit_scale_mask(11,8)};
    hb_mt_phrase phrase={.count=1,.anchor=0};
    phrase.events[0]=(hb_mt_event){.duration=24,.count=1,.chord_mode=1,.scale=0xAB5,.selector=4096,.notes={{60,100}}};
    for(int high=0;high<2;high++){
        memset(instance->motif.events,0,sizeof(instance->motif.events));instance->motif.pending=0;
        unsigned long long before=instance->relative_feedback_word;
        assert(hb_mt_schedule(instance,&phrase,high?68:65,100,0,0,1,position,position,0));
        assert(instance->relative_feedback_word==before);
        uint8_t output[32][3];int lengths[32];
        for(int index=0;index<HB_MT_SCHEDULE;index++)if(instance->motif.events[index].used)
            hb_mt_emit(instance,&instance->motif.events[index],1,output,lengths,32);
        int target=high?10:7,root=high?9:8;
        assert_feedback(instance,high?14:60,target,root,hb_transpose_mask(0x249,root),1,2);
    }
    API.destroy_instance(instance);
}
int main(void){live_and_recorded();live_key_and_parallel();motif_onsets();puts("Rendered operation feedback: live/recorded, raw/chord, key/Parallel, preview, mapped motifs, read isolation and stop clearing pass");}
