#ifndef HB_NOISE_FIELD_H
#define HB_NOISE_FIELD_H
#ifdef HB_FREESTANDING
extern double sqrt(double),cos(double),sin(double),log(double),cbrt(double);
#else
#include <math.h>
#endif
/* Fixed-coordinate, seeded 3-D gradient noise. No allocation or mutable RNG. */
static unsigned hb_nf_hash(unsigned value){value^=value>>16;value*=0x7feb352du;value^=value>>15;value*=0x846ca68bu;return value^(value>>16);}
static double hb_nf_bound(double value,double low,double high){return value>=low?(value<=high?value:high):low;}
static int hb_nf_floor(double value){int whole=(int)value;return whole-(value<whole);}
static double hb_nf_fade(double value){return value*value*value*(value*(value*6-15)+10);}
static double hb_nf_mix(double first,double second,double amount){return first+(second-first)*amount;}
static double hb_nf_grad(unsigned hash,double x,double y,double z){unsigned h=hash&15;double first=h<8?x:y,second=h<4?y:(h==12||h==14?x:z);return ((h&1)?-first:first)+((h&2)?-second:second);}
static double hb_nf_noise(double x,double y,double z,unsigned seed){
    int ix=hb_nf_floor(x),iy=hb_nf_floor(y),iz=hb_nf_floor(z);x-=ix;y-=iy;z-=iz;
    double corners[8];
    for(int corner=0;corner<8;corner++){
        int dx=corner&1,dy=(corner>>1)&1,dz=(corner>>2)&1;
        unsigned hash=hb_nf_hash(seed^hb_nf_hash((unsigned)(ix+dx))^hb_nf_hash((unsigned)(iy+dy)+0x9e3779b9u)^hb_nf_hash((unsigned)(iz+dz)+0x85ebca6bu));
        corners[corner]=hb_nf_grad(hash,x-dx,y-dy,z-dz);
    }
    double fx=hb_nf_fade(x),fy=hb_nf_fade(y),fz=hb_nf_fade(z);
    return hb_nf_mix(hb_nf_mix(hb_nf_mix(corners[0],corners[1],fx),hb_nf_mix(corners[2],corners[3],fx),fy),hb_nf_mix(hb_nf_mix(corners[4],corners[5],fx),hb_nf_mix(corners[6],corners[7],fx),fy),fz);
}
typedef struct {
    int speed,direction,depth,detail,roughness,scale,seed,glide,animation;
    double position[3],rate,zoom,target_rate,target_zoom,lease;
    int active;
} hb_noise_field;
static hb_noise_field g_hb_field={16,0,0,3,50,100,1,100,1,{0,0,0},1,1,1,1,0,0};
typedef struct { double value,target,weight,lease; } hb_field_override;
static hb_field_override g_hb_field_outputs[16];
typedef struct { double target,lease; unsigned order; int blocked; } hb_field_input;
static hb_field_input g_hb_field_inputs[2][16];
static unsigned g_hb_field_input_order;
static int g_hb_field_latch_mode;
static unsigned g_hb_field_latched;
static double g_hb_field_latch_values[16];
/* A clear suppresses stale held frames until their source explicitly releases. */
static void hb_nf_clear_outputs(int lane){
    for(int index=0;index<16;index++)if(lane==16||index==lane){
        g_hb_field_latched&=~(1u<<index);
        for(int source=0;source<2;source++){
            hb_field_input *input=&g_hb_field_inputs[source][index];
            if(input->lease>0)input->blocked=1;
            input->lease=0;
        }
    }
}
static unsigned hb_nf_blocked(int source){
    unsigned mask=0;for(int index=0;index<16;index++)if(g_hb_field_inputs[source][index].blocked)mask|=1u<<index;
    return mask;
}
static int g_hb_field_view,g_hb_field_orientation,g_hb_field_bank;
static int g_hb_field_restored;
/* Placement is independent of noise seed/time and cached on setting changes. */
static int g_hb_field_distribution,g_hb_field_spread=100,g_hb_field_randomness=100,g_hb_field_placement_seed=1;
static double g_hb_field_points[16][3];
static double hb_nf_uniform(unsigned lane,unsigned dimension){
    return (hb_nf_hash((unsigned)g_hb_field_placement_seed^hb_nf_hash(lane+101u)^hb_nf_hash(dimension+701u))+.5)/4294967296.;
}
static double hb_nf_stratified(int lane,int axis){return (((lane*(axis*4+3)+axis*5)%16)+.5)/16;}
static void hb_nf_place(void){
    const double tau=6.283185307179586,randomness=g_hb_field_randomness*.01,spread=g_hb_field_spread*.01;
    for(int lane=0;lane<16;lane++){
        double regular_z=1-2*(lane+.5)/16,regular_angle=lane*2.399963229728653;
        double random_z=1-2*hb_nf_uniform(lane,0),random_angle=tau*hb_nf_uniform(lane,1);
        double regular_radius=sqrt(1-regular_z*regular_z),random_radius=sqrt(1-random_z*random_z);
        double direction[3]={hb_nf_mix(regular_radius*cos(regular_angle),random_radius*cos(random_angle),randomness),hb_nf_mix(regular_radius*sin(regular_angle),random_radius*sin(random_angle),randomness),hb_nf_mix(regular_z,random_z,randomness)};
        double norm=sqrt(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]);
        if(norm<1e-12){direction[0]=1;direction[1]=direction[2]=0;norm=1;}
        /* A permuted stratification avoids coupling radius to polar latitude. */
        double quantile=hb_nf_mix(((lane*7)%16+.5)/16,hb_nf_uniform(lane,2),randomness);
        double radius=g_hb_field_distribution==1?cbrt(quantile):1;
        for(int axis=0;axis<3;axis++){
            double coordinate=direction[axis]/norm*radius;
            if(g_hb_field_distribution==3){
                double gaussian=sqrt(-2*log(hb_nf_uniform(lane,3+axis*2)))*cos(tau*hb_nf_uniform(lane,4+axis*2))/sqrt(3.);
                double regular=sqrt(-2*log(hb_nf_stratified(lane,axis)))*cos(tau*hb_nf_stratified(lane,axis+3))/sqrt(3.);
                coordinate=hb_nf_mix(regular,gaussian,randomness);
            }
            g_hb_field_points[lane][axis]=spread*coordinate;
        }
    }
}
/* Runtime-only overlays, shared by live and clip readers; never saved. */
static double hb_nf_output(int index,double raw){
    if(index<0||index>=16)return raw;
    return hb_nf_mix(raw,g_hb_field_outputs[index].value,g_hb_field_outputs[index].weight);
}
static void hb_nf_reset(void){hb_noise_field fresh={16,0,0,3,50,100,1,100,1,{0,0,0},1,1,1,1,0,0};g_hb_field=fresh;g_hb_field_restored=0;memset(g_hb_field_outputs,0,sizeof(g_hb_field_outputs));memset(g_hb_field_inputs,0,sizeof(g_hb_field_inputs));g_hb_field_input_order=0;g_hb_field_latch_mode=0;g_hb_field_latched=0;memset(g_hb_field_latch_values,0,sizeof(g_hb_field_latch_values));g_hb_field_view=g_hb_field_orientation=g_hb_field_bank=0;g_hb_field_distribution=0;g_hb_field_spread=g_hb_field_randomness=100;g_hb_field_placement_seed=1;hb_nf_place();}
static double hb_nf_sample(double x,double y,double z){
    const hb_noise_field *field=&g_hb_field;
    double scale=hb_nf_bound(field->scale*.01*field->zoom,.01,100),sum=0,weight=1,total=0;
    x=x/scale+field->position[0]+.137;y=y/scale+field->position[1]+.271;z=z/scale+field->position[2]+.419;
    for(int layer=0;layer<field->detail;layer++){
        sum+=weight*hb_nf_noise(x,y,z,(unsigned)field->seed+(unsigned)layer*1013u);total+=weight;
        x=x*2+.173;y=y*2+.317;z=z*2+.113;weight*=field->roughness*.01;
    }
    return hb_nf_bound(.5+.5*sum/(total?total:1),0,1);
}
static double hb_nf_lane_sample(int lane,double x,double y,double z){
    if(g_hb_field_distribution&&lane>=0&&lane<16){const double *point=g_hb_field_points[lane];return hb_nf_sample(point[0],point[1],point[2]);}
    return hb_nf_sample(x,y,z);
}
/* Audio clock owner advances once per block. Display reads never advance time.
   Expiring live gestures protects against a disconnected surface or UI crash. */
static void hb_nf_advance(double seconds,double bpm){
    hb_noise_field *field=&g_hb_field;if(seconds<=0||seconds>1)return;
    if(field->lease>0){field->lease-=seconds;if(field->lease<=0){field->target_rate=field->target_zoom=1;}}
    double blend=field->glide?hb_nf_bound(seconds*1000/field->glide,0,1):1;
    for(int index=0;index<16;index++){
        hb_field_override *output=&g_hb_field_outputs[index];
        hb_field_input *selected=0;
        for(int source=0;source<2;source++){
            hb_field_input *input=&g_hb_field_inputs[source][index];
            input->lease=hb_nf_bound(input->lease-seconds,0,1.5);
            if(input->lease>0&&(!selected||input->order>selected->order))selected=input;
        }
        output->lease=selected?selected->lease:0;
        if(selected){output->target=selected->target;if(output->weight==0)output->value=output->target;}
        int latched=(g_hb_field_latched&(1u<<index))!=0;
        if(!selected&&latched){output->target=g_hb_field_latch_values[index];if(output->weight==0)output->value=output->target;}
        output->weight=hb_nf_mix(output->weight,selected||latched?1:0,blend);
        output->value=hb_nf_mix(output->value,output->target,blend);
    }
    field->rate=hb_nf_mix(field->rate,field->target_rate,blend);field->zoom=hb_nf_mix(field->zoom,field->target_zoom,blend);
    static const double directions[8][2]={{1,0},{.707106781,.707106781},{0,1},{-.707106781,.707106781},{-1,0},{-.707106781,-.707106781},{0,-1},{.707106781,-.707106781}};
    double advance=seconds*bpm/(60*4*field->speed)*field->rate;
    field->position[0]+=advance*directions[field->direction][0];field->position[1]+=advance*directions[field->direction][1];field->position[2]+=advance*field->depth*.01;
}
#endif
