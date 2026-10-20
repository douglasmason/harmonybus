#define main field_existing_main
#include "chord_player_test.c"
#undef main
#include <math.h>
int main(void){
    Inst *instance=fixture();
    API.set_param(instance,"field_detail","3");API.set_param(instance,"field_seed","7");
    double first=hb_nf_sample(.2,.7,.3);assert(first>=0&&first<=1);assert(first==hb_nf_sample(.2,.7,.3));
    assert(fabs(first-hb_nf_sample(.200001,.7,.3))<.0001);
    API.set_param(instance,"field_glide","0");API.set_param(instance,"field_gesture","0,100");
    double position=g_hb_field.position[0];hb_nf_advance(.1,120);assert(g_hb_field.position[0]==position);
    API.set_param(instance,"field_gesture","100,100");hb_nf_advance(.1,120);double forward=g_hb_field.position[0];assert(forward>position);
    API.set_param(instance,"field_gesture","-100,100");hb_nf_advance(.1,120);assert(fabs(g_hb_field.position[0]-position)<1e-10);
    API.set_param(instance,"field_release","1");hb_nf_advance(.1,120);assert(g_hb_field.position[0]>position);
    API.set_param(instance,"field_gesture","-400,25");for(int index=0;index<20;index++)hb_nf_advance(.1,120);assert(g_hb_field.target_rate==1&&g_hb_field.target_zoom==1);
    API.set_param(instance,"field_gesture","999999,0");assert(g_hb_field.target_rate==1);
    API.set_param(instance,"motion_lane","1");API.set_param(instance,"motion_operation","Velocity");
    API.set_param(instance,"motion_field_target","1");API.set_param(instance,"motion_field_low","0");API.set_param(instance,"motion_field_high","0");
    double value=0;assert(!hb_mo_value(&instance->motion,0,0,0,&value));
    API.set_param(instance,"motion_field_low","100");API.set_param(instance,"motion_field_high","100");
    assert(hb_mo_value(&instance->motion,0,0,0,&value));
    char probability[32];API.get_param(instance,"motion_probability",probability,sizeof(probability));assert(!strcmp(probability,"100"));
    char saved[16384];API.get_param(instance,"state",saved,sizeof(saved));assert(strstr(saved,";nm1,0,1,100,100"));
    hb_motion_config restored;hb_mo_defaults(&restored);hb_mo_restore(&restored,saved);hb_field_mapping_restore(&restored,saved);
    assert(restored.lanes[0].field_target==1&&restored.lanes[0].field_high==100);
    char snapshot[1024];double before=g_hb_field.position[0];assert(API.get_param(instance,"field_view",snapshot,sizeof(snapshot))>0);assert(g_hb_field.position[0]==before);
    const char *held="fo1,2500,7500,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1";
    const char *released="fo1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1";
    API.set_param(instance,"field_outputs",held);hb_nf_advance(.01,120);
    assert(fabs(hb_nf_output(0,.8)-.25)<1e-9&&fabs(hb_nf_output(1,.2)-.75)<1e-9);
    API.set_param(instance,"field_outputs_remote","fo1,9000,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1");hb_nf_advance(.01,120);
    assert(fabs(hb_nf_output(0,.8)-.9)<1e-9);
    API.set_param(instance,"field_outputs",held);hb_nf_advance(.01,120);assert(fabs(hb_nf_output(0,.8)-.9)<1e-9);
    API.set_param(instance,"field_outputs_remote",released);hb_nf_advance(.01,120);assert(fabs(hb_nf_output(0,.8)-.25)<1e-9);
    API.set_param(instance,"field_outputs","fo1,8000");hb_nf_advance(.01,120);assert(fabs(hb_nf_output(0,.8)-.25)<1e-9);
    API.set_param(instance,"motion_field_target","2");API.set_param(instance,"motion_field_low","0");API.set_param(instance,"motion_field_high","100");
    assert(hb_mo_value(&instance->motion,0,0,0,&value)&&fabs(value-25)<1e-9);
    API.set_param(instance,"field_glide","100");API.set_param(instance,"field_outputs",released);hb_nf_advance(.01,120);
    assert(hb_nf_output(0,.8)>.25&&hb_nf_output(0,.8)<.8);
    API.set_param(instance,"field_glide","0");API.set_param(instance,"field_outputs",held);
    for(int index=0;index<20;index++)hb_nf_advance(.1,120);assert(hb_nf_output(0,.8)==.8);
    API.set_param(instance,"field_display","1");API.set_param(instance,"field_orientation","1");
    API.get_param(instance,"state",saved,sizeof(saved));assert(strstr(saved,";nv1,1,1,0")&&!strstr(saved,"fo1"));
    before=g_hb_field.position[0];assert(API.get_param(instance,"field_outputs_view",snapshot,sizeof(snapshot))>0);assert(before==g_hb_field.position[0]);
    puts("field: deterministic continuity, stop/reverse/release, timeout, probability, persistence and read-only previews pass");
}
