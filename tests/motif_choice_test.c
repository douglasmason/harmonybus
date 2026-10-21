#define main previous_motif_main
#include "chord_player_test.c"
#undef main
static void record_note(Inst *instance,int pitch){midi(instance,1,pitch);midi(instance,0,pitch);}

int main(void){
    Inst *instance=fixture();int slots[16];double probabilities[16];
    assert(!hb_mt_choice_view(instance,slots,probabilities));
    for(int slot=0;slot<3;slot++)hb_mt_preset_definition(slot+1,&g_motifs[slot]);
    API.set_param(instance,"field_glide","0");
    API.set_param(instance,"field_outputs","fo1,2000,3000,5000,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1");
    hb_nf_advance(.01,120);
    assert(hb_mt_choice_view(instance,slots,probabilities)==3);
    assert(fabs(probabilities[0]-.2)<1e-9);
    API.set_param(instance,"motif_choice_edit","0,6000");
    assert(hb_mt_choice_view(instance,slots,probabilities)==3);
    assert(fabs(probabilities[0]-.6)<1e-9&&fabs(probabilities[1]-.15)<1e-9&&fabs(probabilities[2]-.25)<1e-9);
    API.set_param(instance,"motif_choice_edit","1,8000");
    hb_mt_choice_view(instance,slots,probabilities);
    assert(fabs(probabilities[0]-.6)<1e-9&&fabs(probabilities[1]-.4)<1e-9&&probabilities[2]==0);
    API.set_param(instance,"field_override","1");
    API.set_param(instance,"motif_choice_release","0,1");assert(instance->motif.choice.latched==1);
    hb_mt_choice_advance(instance,2);assert(!instance->motif.choice.held&&instance->motif.choice.latched==1);
    API.set_param(instance,"field_glide","100");
    API.set_param(instance,"motif_choice_clear","16");hb_mt_choice_advance(instance,.05);
    hb_mt_choice_view(instance,slots,probabilities);assert(probabilities[0]>.2&&probabilities[0]<.6);
    assert(fabs(probabilities[0]+probabilities[1]+probabilities[2]-1)<1e-9);
    hb_mt_choice_advance(instance,.1);hb_mt_choice_view(instance,slots,probabilities);assert(fabs(probabilities[0]-.2)<1e-9);
    API.set_param(instance,"motif_choice_edit","0,10000");
    API.set_param(instance,"motion_operation","Play Motif");API.set_param(instance,"motion_field_target","3");
    API.set_param(instance,"motif_edit","Open");API.set_param(instance,"motif_arrival","Now");API.set_param(instance,"motif_playback","Tap Free");API.set_param(instance,"motif_close","Close");
    API.set_param(instance,"motion_hold_1","On");record_note(instance,67);
    assert(instance->motif.choice.draws==1&&instance->motif.choice.last_slot==0&&instance->motif.tap_active);
    hb_mt_phrase captured=instance->motif.tap_phrase;
    char snapshot[2048];for(int index=0;index<50;index++)assert(API.get_param(instance,"motif_choice_view",snapshot,sizeof(snapshot))>0);
    assert(instance->motif.choice.draws==1);
    API.set_param(instance,"motif_choice_clear","16");API.set_param(instance,"motif_choice_edit","1,10000");
    record_note(instance,67);assert(instance->motif.choice.draws==1);
    assert(!memcmp(&captured,&instance->motif.tap_phrase,sizeof(captured)));
    while(instance->motif.tap_active)record_note(instance,67);
    record_note(instance,67);assert(instance->motif.choice.draws==2&&instance->motif.choice.last_slot==1);
    char saved[131072];API.get_param(instance,"state",saved,sizeof(saved));
    hb_motion_config restored;hb_mo_defaults(&restored);hb_mo_restore(&restored,saved);hb_field_mapping_restore(&restored,saved);assert(restored.lanes[0].field_target==3);
    /* Membership changes invalidate pins rather than attaching them elsewhere. */
    memset(&g_motifs[0],0,sizeof(g_motifs[0]));hb_mt_choice_advance(instance,.01);
    assert(!instance->motif.choice.held&&!instance->motif.choice.latched);
    assert(hb_mt_choice_view(instance,slots,probabilities)==2);
    /* A full scheduler must not leave an undrawn first tap active. */
    instance->motif.tap_active=0;instance->motif.cancel=0;
    for(int index=0;index<HB_MT_SCHEDULE;index++)instance->motif.events[index].used=1;
    instance->motif.pending=HB_MT_SCHEDULE;
    unsigned before=instance->motif.choice.draws;
    assert(!hb_mt_choice_launch(instance,0,67,100,0));
    assert(instance->motif.choice.draws==before&&!instance->motif.tap_active);
    memset(instance->motif.events,0,sizeof(instance->motif.events));instance->motif.pending=0;
    assert(hb_mt_choice_launch(instance,0,67,100,0));assert(instance->motif.choice.draws==before+1);
    API.destroy_instance(instance);
    puts("Motif probabilities: exact edits, leases, latch, release, phrase snapshots, previews and persistence passed");
}
