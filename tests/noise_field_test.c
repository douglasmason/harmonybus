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
    API.set_param(instance,"field_override","1");API.set_param(instance,"field_glide","100");
    API.set_param(instance,"field_outputs",held);hb_nf_advance(.01,120);
    API.set_param(instance,"field_output_finish","0,0,9000");
    assert(g_hb_field_latched&1u);assert(g_hb_field_latch_values[0]==.9);
    API.set_param(instance,"field_outputs",released);
    for(int index=0;index<40;index++)hb_nf_advance(.1,120);
    assert(fabs(hb_nf_output(0,.1)-.9)<1e-9); /* target, not in-flight glide */
    API.set_param(instance,"field_outputs",held); /* a new press replaces the latch */
    assert(!(g_hb_field_latched&1u));
    for(int index=0;index<20;index++)hb_nf_advance(.1,120);
    assert(!g_hb_field_latched);API.set_param(instance,"field_output_finish","0,0,7500");assert(!g_hb_field_latched); /* timeout cannot latch */
    API.set_param(instance,"field_outputs",held);
    API.set_param(instance,"field_output_finish","0,0,2500");API.set_param(instance,"field_output_finish","0,1,7500");
    assert(g_hb_field_latched==3);API.set_param(instance,"field_output_clear","0");assert(g_hb_field_latched==2);
    API.set_param(instance,"field_outputs",held);API.set_param(instance,"field_output_clear","16");
    assert(!g_hb_field_latched&&hb_nf_blocked(0)==3);
    API.set_param(instance,"field_outputs",held);API.set_param(instance,"field_output_finish","0,0,5000");
    assert(!g_hb_field_latched&&g_hb_field_inputs[0][0].lease==0); /* stale held frames cannot undo clear */
    API.set_param(instance,"field_outputs",released);assert(hb_nf_blocked(0)==0);
    API.set_param(instance,"field_outputs_remote",held);API.set_param(instance,"field_output_finish","1,1,7500");assert(g_hb_field_latched==2);
    API.set_param(instance,"field_display","0");API.set_param(instance,"field_display","1");assert(g_hb_field_latched==2);
    API.get_param(instance,"state",saved,sizeof(saved));assert(strstr(saved,";nl1,1")&&!strstr(saved,"fo1"));
    assert(API.get_param(instance,"field_outputs_view2",snapshot,sizeof(snapshot))>0&&!strncmp(snapshot,"fo2,",4));
    hb_field_restore(saved);assert(!g_hb_field_latched); /* load clears runtime latches */
    /* Placement remains fixed across time, view reads and field-seed changes. */
    hb_nf_reset();double manual=hb_nf_sample(.2,.7,.3);
    assert(manual==hb_nf_lane_sample(0,.2,.7,.3));
    API.set_param(instance,"field_distribution","2");API.set_param(instance,"field_spread","250");
    double points[16][3];memcpy(points,g_hb_field_points,sizeof(points));
    for(int lane=0;lane<16;lane++){
        double norm=0;for(int axis=0;axis<3;axis++)norm+=points[lane][axis]*points[lane][axis];
        assert(fabs(norm-6.25)<1e-9);
        assert(hb_nf_lane_sample(lane,12,13,14)==hb_nf_sample(points[lane][0],points[lane][1],points[lane][2]));
    }
    API.set_param(instance,"field_seed","99");hb_nf_advance(.1,120);
    assert(!memcmp(points,g_hb_field_points,sizeof(points)));
    API.set_param(instance,"field_placement_seed","2");assert(memcmp(points,g_hb_field_points,sizeof(points)));
    API.set_param(instance,"field_placement_seed","1");assert(!memcmp(points,g_hb_field_points,sizeof(points)));
    API.set_param(instance,"field_spread","0");
    for(int lane=1;lane<16;lane++)assert(hb_nf_lane_sample(lane,0,0,0)==hb_nf_lane_sample(0,0,0,0));
    API.set_param(instance,"field_spread","100");
    for(int shape=1;shape<=3;shape++){
        double radius_sum=0,axis_sum[3]={0},axis_squares[3]={0};int count=0;
        g_hb_field_distribution=shape;
        for(int seed=1;seed<=1024;seed++){
            g_hb_field_placement_seed=seed;hb_nf_place();
            for(int lane=0;lane<16;lane++){
                double norm=0;for(int axis=0;axis<3;axis++){double coordinate=g_hb_field_points[lane][axis];assert(isfinite(coordinate));norm+=coordinate*coordinate;axis_sum[axis]+=coordinate;axis_squares[axis]+=coordinate*coordinate;}
                if(shape<=2)assert(norm<=1.000000001);
                radius_sum+=norm;count++;
            }
        }
        double expected=shape==1?.6:1.;assert(fabs(radius_sum/count-expected)<.03);
        for(int axis=0;axis<3;axis++){assert(fabs(axis_sum[axis]/count)<.02);assert(fabs(axis_squares[axis]/count-expected/3)<.02);}
    }
    API.set_param(instance,"field_distribution","1");API.set_param(instance,"field_randomness","0");
    memcpy(points,g_hb_field_points,sizeof(points));API.set_param(instance,"field_placement_seed","9");assert(!memcmp(points,g_hb_field_points,sizeof(points)));
    API.set_param(instance,"field_randomness","65");API.set_param(instance,"field_spread","234");
    memcpy(points,g_hb_field_points,sizeof(points));API.get_param(instance,"state",saved,sizeof(saved));assert(strstr(saved,";nfp1,1,234,65,9"));
    hb_nf_reset();hb_field_restore(saved);assert(!memcmp(points,g_hb_field_points,sizeof(points)));
    hb_nf_reset();hb_field_restore("legacy");assert(g_hb_field_distribution==0);
    puts("field: deterministic continuity, stop/reverse/release, timeout, probability, persistence and read-only previews pass");
}
