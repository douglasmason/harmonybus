#ifndef HB_CADENCES_H
#define HB_CADENCES_H
/* Stable program/step ids are stored in note-relative recordings. Append
   programs; never reorder them. Paths resolve intermediate targets inside out.
   Positive path terms count parent degrees; negative terms use major-reference
   intervals (e.g. -5 is the functional dominant target, a perfect fifth). */
#define HB_CADENCE_COUNT 14
#define HB_CADENCE_STEPS 6
enum { HB_CAD_TARGET,HB_CAD_DEGREE,HB_CAD_DOMINANT,HB_CAD_BORROWED,
       HB_CAD_LEADING,HB_CAD_MINOR_DEGREE,HB_CAD_MINOR_DOMINANT,HB_CAD_MINOR_TARGET };
typedef struct { int path[3],degree,kind;const char *label; } hb_cadence_step;
typedef struct {const char *name;int length;hb_cadence_step steps[HB_CADENCE_STEPS];} hb_cadence_program;
#define CAD(kind,degree,label) {{0,0,0},degree,kind,label}
#define NEST(kind,target,degree,label) {{target,0,0},degree,kind,label}
static const hb_cadence_program HB_CADENCES[HB_CADENCE_COUNT]={
 {"bVI-bVII-I",3,{CAD(HB_CAD_BORROWED,6,"bVI"),CAD(HB_CAD_BORROWED,7,"bVII"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"bVI-V-I",3,{CAD(HB_CAD_BORROWED,6,"bVI"),CAD(HB_CAD_DOMINANT,5,"V"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"bIII-IV-I",3,{CAD(HB_CAD_BORROWED,3,"bIII"),CAD(HB_CAD_DEGREE,4,"IV"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"vi-V-I",3,{CAD(HB_CAD_DEGREE,6,"vi"),CAD(HB_CAD_DOMINANT,5,"V"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"iii-vi-ii-V-I",5,{CAD(HB_CAD_DEGREE,3,"iii"),CAD(HB_CAD_DEGREE,6,"vi"),CAD(HB_CAD_DEGREE,2,"ii"),CAD(HB_CAD_DOMINANT,5,"V"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"IV-iv-I",3,{CAD(HB_CAD_DEGREE,4,"IV"),CAD(HB_CAD_BORROWED,4,"iv"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"ii halfdim-V-i",3,{CAD(HB_CAD_MINOR_DEGREE,2,"ii halfdim"),CAD(HB_CAD_MINOR_DOMINANT,5,"V"),CAD(HB_CAD_MINOR_TARGET,1,"i")}},
 {"I-VI7-ii-V-I",5,{CAD(HB_CAD_TARGET,1,"I"),NEST(HB_CAD_DOMINANT,2,5,"VI7"),CAD(HB_CAD_DEGREE,2,"ii"),CAD(HB_CAD_DOMINANT,5,"V"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"V/V-V-I",3,{NEST(HB_CAD_DOMINANT,-5,5,"V/V"),CAD(HB_CAD_DOMINANT,5,"V"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"ii/V-V/V-V-I",4,{NEST(HB_CAD_DEGREE,-5,2,"ii/V"),NEST(HB_CAD_DOMINANT,-5,5,"V/V"),CAD(HB_CAD_DOMINANT,5,"V"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"V/ii-ii-V-I",4,{NEST(HB_CAD_DOMINANT,2,5,"V/ii"),CAD(HB_CAD_DEGREE,2,"ii"),CAD(HB_CAD_DOMINANT,5,"V"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"V/vi-vi-ii-V-I",5,{NEST(HB_CAD_DOMINANT,6,5,"V/vi"),CAD(HB_CAD_DEGREE,6,"vi"),CAD(HB_CAD_DEGREE,2,"ii"),CAD(HB_CAD_DOMINANT,5,"V"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"vii dim/V-V-I",3,{NEST(HB_CAD_LEADING,-5,7,"vii dim/V"),CAD(HB_CAD_DOMINANT,5,"V"),CAD(HB_CAD_TARGET,1,"Target")}},
 {"III7-VI7-II7-V7-I",5,{NEST(HB_CAD_DOMINANT,6,5,"III7"),NEST(HB_CAD_DOMINANT,2,5,"VI7"),NEST(HB_CAD_DOMINANT,-5,5,"II7"),CAD(HB_CAD_DOMINANT,5,"V7"),CAD(HB_CAD_TARGET,1,"Target")}}
};
#undef CAD
#undef NEST
static const hb_cadence_step *hb_cadence_decode(unsigned id){
    if(!id)return 0;id--;
    unsigned program=id/HB_CADENCE_STEPS,step=id%HB_CADENCE_STEPS;
    return program<HB_CADENCE_COUNT&&step<(unsigned)HB_CADENCES[program].length?&HB_CADENCES[program].steps[step]:0;
}
#endif
