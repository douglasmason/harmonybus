#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/harmony_core.h"
#include "../src/motion.h"
#include "../src/motif.h"
#include "../src/motif_clip.h"
static void placement_and_padding(void){
    hb_mt_definition definition;hb_mt_preset_definition(2,&definition);
    hb_mt_definition original=definition;hb_mc_plan plan;
    assert(definition.body.count==2&&definition.placement==HB_MT_TARGET_END);
    for(int placement=0;placement<4;placement++){
        assert(hb_mc_build(&definition,placement,96,1,1,&plan));
        assert(plan.duration==(placement==HB_MC_OMIT?192:384));
        if(placement==HB_MC_START){
            assert(plan.events[0].source_step==-1&&plan.events[0].duration==192);
            assert(plan.events[1].source_step==0&&plan.events[2].source_step==1);
        }else if(placement==HB_MC_END){
            assert(plan.events[2].source_step==-1&&plan.events[2].duration==192);
        }else if(placement==HB_MC_BOTH){
            assert(plan.count==4&&plan.events[0].source_step==-1&&plan.events[3].source_step==-1);
        }else assert(plan.count==2&&plan.events[1].source_step==1);
        for(int index=0;index<plan.count;index++)if(plan.events[index].source_step>=0)
            assert(!memcmp(&plan.events[index].event,&definition.body.events[plan.events[index].source_step],sizeof(hb_mt_event)));
    }
    assert(!memcmp(&original,&definition,sizeof(definition)));
}
static void both_boundary_targets_are_separate(void){
    hb_mt_definition definition;hb_mt_preset_definition(13,&definition);
    assert(definition.body.count==3&&definition.placement==3);
    /* I-VI7-ii-V-I stores only VI7-ii-V. Each target remains outside. */
    assert(hb_cadence_decode(definition.body.events[0].cadence)->kind==HB_CAD_DOMINANT);
    for(int step=0;step<3;step++)assert(hb_cadence_decode(definition.body.events[step].cadence)->kind!=HB_CAD_TARGET);
    hb_mc_plan plan;
    assert(hb_mc_build(&definition,HB_MC_START,96,1,0,&plan));
    assert(plan.count==4&&plan.events[0].source_step==-1&&plan.events[1].source_step==0);
    assert(hb_mc_build(&definition,HB_MC_OMIT,96,1,1,&plan));
    assert(plan.count==4&&plan.events[3].event.kind==1);
    hb_mt_phrase phrase;
    assert(hb_mt_materialize(&definition,definition.placement,&phrase));
    assert(phrase.count==5&&phrase.anchor==4);
    assert(hb_cadence_decode(phrase.events[0].cadence)->kind==HB_CAD_TARGET);
    assert(hb_cadence_decode(phrase.events[4].cadence)->kind==HB_CAD_TARGET);
}
static void omitted_target_is_still_a_reference(void){
    hb_mt_definition definition;hb_mt_preset_definition(21,&definition);
    assert(definition.body.count==3&&!definition.placement&&definition.reference_pitch==60);
    hb_mc_plan plan;
    assert(hb_mc_build(&definition,HB_MC_OMIT,96,1,1,&plan));
    assert(plan.count==4&&plan.duration==384);
    assert(plan.events[2].event.modifier==-1&&plan.events[3].event.kind==1);
    assert(plan.events[3].source_step==-2);
    assert(hb_mc_build(&definition,HB_MC_END,96,1,1,&plan));
    assert(plan.count==4&&plan.events[3].source_step==-1&&!plan.events[3].event.kind);
    assert(!plan.events[3].event.modifier&&!plan.events[3].event.secondary);
}
static void all_presets_share_one_body(void){
    for(int preset=1;preset<HB_MT_PRESET_COUNT;preset++){
        hb_mt_definition definition;hb_mt_preset_definition(preset,&definition);
        assert(definition.body.count>0);
        assert(definition.placement==(preset>=20?0:preset==13?3:HB_MT_TARGET_END));
        for(int step=0;step<definition.body.count;step++){
            const hb_mt_event *event=&definition.body.events[step];
            const hb_cadence_step *cadence=hb_cadence_decode(event->cadence);
            assert(event->secondary!=3);
            assert(!cadence||(cadence->kind!=HB_CAD_TARGET&&cadence->kind!=HB_CAD_MINOR_TARGET));
        }
        hb_mc_plan plan;
        for(int placement=0;placement<4;placement++)assert(hb_mc_build(&definition,placement,96,1,1,&plan));
    }
    hb_mt_definition minor;hb_mt_preset_definition(12,&minor);
    assert(hb_cadence_decode(minor.target_end.cadence)->kind==HB_CAD_MINOR_TARGET);
    assert(!memcmp(&minor.target_start,&minor.target_end,sizeof(hb_mt_event)));
}
static void automatic_scrub_preserves_interior_targets(void){
    hb_mt_event target={.duration=24,.count=1,.chord_mode=3,.scale=0xAB5,.notes={{60,100}}};
    hb_mt_phrase phrase={.count=7,.anchor=5};
    phrase.events[0]=target;
    phrase.events[1]=target;phrase.events[1].secondary=1;
    phrase.events[2]=target; /* Interior I must remain. */
    phrase.events[3]=target;phrase.events[3].modifier=-1;
    phrase.events[4]=target;phrase.events[4].secondary=2;
    phrase.events[5]=target;phrase.events[5].notes[0].velocity=77;
    phrase.events[6]=(hb_mt_event){.kind=2,.duration=48};
    hb_mt_definition definition;
    assert(hb_mt_normalize(&phrase,&definition));
    assert(definition.placement==3&&definition.body.count==4);
    assert(!memcmp(&definition.body.events[1],&target,sizeof(target)));
    assert(definition.target_end.duration==72&&definition.target_end.notes[0].velocity==77);
    assert(definition.body.events[2].modifier==-1);
    hb_mt_phrase approach;hb_mt_preset(21,&approach);
    assert(hb_mt_normalize(&approach,&definition));
    assert(definition.body.count==3&&!definition.placement); /* LT is not Target. */
    hb_mt_phrase only={.count=1,.anchor=0};only.events[0]=target;
    assert(hb_mt_normalize(&only,&definition));
    assert(definition.body.count==0&&definition.placement==HB_MT_TARGET_END);
    hb_mt_phrase materialized;
    assert(hb_mt_materialize(&definition,definition.placement,&materialized));
    assert(materialized.count==1);
}
static void rhythm_ties_and_atomic_failure(void){
    hb_mt_definition definition;hb_mt_preset_definition(2,&definition);hb_mc_plan plan;
    for(int rhythm=0;rhythm<6;rhythm++){
        assert(hb_mc_build(&definition,HB_MC_END,48,rhythm,1,&plan));
        assert(plan.duration==192);
        for(int index=1;index<plan.count;index++)assert(plan.events[index].onset==plan.events[index-1].onset+plan.events[index-1].duration);
    }
    definition.body.events[2]=(hb_mt_event){.kind=2,.duration=24};definition.body.count=3;
    assert(hb_mc_build(&definition,HB_MC_OMIT,96,1,0,&plan));
    assert(plan.count==2&&plan.events[1].duration==192&&plan.duration==288);
    hb_mc_plan previous=plan;definition.body.events[0].kind=2;
    assert(!hb_mc_build(&definition,HB_MC_START,96,1,1,&plan));
    assert(!memcmp(&previous,&plan,sizeof(plan)));
}
static void entered_lengths_and_fractional_padding(void){
    hb_mt_definition definition;hb_mt_preset_definition(2,&definition);hb_mc_plan plan;
    definition.body.events[0].duration=12;definition.body.events[1].duration=6;
    assert(hb_mc_build(&definition,HB_MC_END,96,0,1,&plan));
    assert(plan.count==3&&plan.duration==192);
    assert(plan.events[0].duration==48&&plan.events[1].duration==24&&plan.events[2].duration==120);
    assert(hb_mc_build(&definition,HB_MC_OMIT,96,0,1,&plan));
    assert(plan.count==3&&plan.duration==192&&plan.events[2].event.kind==1&&plan.events[2].duration==120);
    hb_mt_event reference={.count=2,.notes={{60,100},{64,100}}};
    hb_mt_event duplicate={.count=2,.notes={{60,100},{60,100}}};
    assert(!hb_mt_is_target(&duplicate,&reference));
}
int main(void){entered_lengths_and_fractional_padding();placement_and_padding();both_boundary_targets_are_separate();omitted_target_is_still_a_reference();all_presets_share_one_body();automatic_scrub_preserves_interior_targets();rhythm_ties_and_atomic_failure();puts("Normalized motif body, target placement, padding, intent and timing regressions pass");}
