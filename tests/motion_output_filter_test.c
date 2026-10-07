/* Differential oracle retained from v0.2.267 before operation filtering.
 * Exercise every operation at every lane, including recorded replacement and
 * held replacement of a recorded operation. The oracle must remain unchanged. */
#define main fixture_suite_main
#include "follower_reference_test.c"
#undef main
static void reference_motion_values(Inst *instance,const uint8_t message[3],int *pitch,int *velocity,int *pan,double *off_beat,int *skip){
    *pitch=message[1];*velocity=message[2];*pan=-1;*off_beat=-1;*skip=0;
    double beat=hb_motion_position(instance),condition=hb_motion_condition_position();
    if(instance->role==0&&!hb_cp_mode(&instance->player)){
        int secondary=hb_secondary_at(instance,message[1]);
        const hb_cadence_step *cadence=hb_mo_current_cadence(&instance->motion);
        if(secondary||cadence){
            hb_harmony_t harmony=hb_render_harmony(instance);
            int parent_root=0;hb_resolve_follower_reference_root(instance,&parent_root);
            unsigned parent=hb_parent_chord_scale(instance,harmony,parent_root,hb_follower_input_scale(instance,parent_root),g_bus.global_transpose);
            if(cadence)*pitch=hb_resolve_cadence(instance,cadence,*pitch,parent).root;
            else *pitch+=hb_context_approach_offset(instance,secondary,*pitch,hb_secondary_collection(instance,secondary,*pitch,parent));
            while(*pitch<0)*pitch+=12;while(*pitch>127)*pitch-=12;
        }
    }
    for(int index=0;index<HB_MOTION_LANES;index++){
        double value;
        if(!hb_mo_value_at(&instance->motion,index,beat,condition,message[1],&value))continue;
        hb_motion_lane resolved=hb_mo_settings(&instance->motion,index);const hb_motion_lane *lane=&resolved;
        if(lane->operation==HB_MO_VELOCITY)*velocity=hb_mo_clamp(hb_mo_round(*velocity*(1.0+value/100.0)),1,127);
        else if(lane->operation==HB_MO_PAN)*pan=hb_mo_clamp(hb_mo_round(64.0+value*0.63),0,127);
        else if(lane->operation==HB_MO_OCTAVE){
            *pitch+=12*hb_mo_clamp(hb_mo_round(value),-4,4);
            while(*pitch<0)*pitch+=12;while(*pitch>127)*pitch-=12;
        }else if(lane->operation==HB_MO_ROTATE){
            hb_harmony_t harmony=hb_render_harmony(instance);if(!harmony.valid)continue;
            hb_harmony_t collection=instance->content_map==1?hb_follower_scale_target(instance,harmony):hb_follower_content_target(instance,harmony,instance->content_map);
            if(instance->content_map==2)collection.pitch_mask=0xfff;
            hb_fp_config transform={0};transform.rotate=hb_mo_clamp(hb_mo_round(value),-24,24);
            *pitch=hb_fp_note(transform,*pitch,harmony.root_pc,collection.pitch_mask);
        }else if(lane->operation==HB_MO_GATE){
            double deadline=beat+hb_mo_grid(lane->grid)*hb_mo_clamp(hb_mo_round(value),1,400)/100.0;
            if(*off_beat<0||deadline<*off_beat)*off_beat=deadline;
        }else if(lane->operation==HB_MO_SKIP&&value>0)*skip=1;
        else if(lane->operation==HB_MO_TRANSPOSE)*pitch=hb_mo_clamp(*pitch+hb_mo_round(value),0,127);
        else if(!hb_mo_chord_approach_done(&instance->motion)&&!(instance->motion.held&(1ULL<<index))&&(lane->operation==HB_MO_BELOW||lane->operation==HB_MO_ABOVE||lane->operation==HB_MO_CHROM_ABOVE||lane->operation==HB_MO_TRITONE_V)&&value>0)
            *pitch=hb_apply_approach(instance,*pitch,lane->operation==HB_MO_BELOW?HB_APPROACH_CHROM_BELOW:(lane->operation==HB_MO_CHROM_ABOVE||lane->operation==HB_MO_TRITONE_V)?HB_APPROACH_CHROM_ABOVE:HB_APPROACH_SCALE_ABOVE);
    }
}

int main(void){
    Inst *instance=fixture();hb_commit_observed_harmony(chord(2,1,1));
    hb_motion_config original=instance->motion;
    unsigned long long recorded[HB_MOTION_LANES+1]={0};
    unsigned comparisons=0;
    for(int operation=HB_MO_OFF;operation<=HB_MO_KEY_RETURN;operation++)
    for(int slot=0;slot<HB_MOTION_LANES;slot++)
    for(int state=0;state<8;state++)for(int pattern=0;pattern<7;pattern++){
        instance->motion=original;memset(recorded,0,sizeof(recorded));
        instance->motion.bypass=state&1;
        instance->motion.held=(state&2)?1ULL<<slot:0;
        hb_motion_lane *lane=&instance->motion.lanes[slot];
        lane->operation=operation;lane->enabled=1;lane->pattern=pattern;
        lane->amount=pattern%2?-3:37;lane->offset=pattern-3;
        lane->grid=(slot%9);lane->cycle=slot%8;lane->phase=slot%4;
        lane->probability=pattern%3==0?0:pattern%3==1?65:100;
        lane->group=slot%2;lane->evolve=1;lane->advance=slot%3;
        lane->every=slot%4+1;lane->from=1;lane->through=2;
        instance->motion.events[slot]=slot+3;
        /* An active velocity lane also catches ordering interactions. */
        int adjacent=(slot+1)%HB_MOTION_LANES;
        instance->motion.lanes[adjacent].operation=HB_MO_VELOCITY;
        instance->motion.lanes[adjacent].enabled=1;
        instance->motion.lanes[adjacent].amount=-15;
        if(state&4){
            recorded[slot]=HB_MO_RECORDED|hb_mo_operation_word(operation)|
                ((unsigned long long)(8-slot%9)<<37)|
                ((unsigned long long)400<<41)|(uint32_t)(int32_t)(pattern%2?-1750:37250);
            instance->motion.event_override=recorded;
            /* Opposite class: relevant recorded op replacing irrelevant live
               op, and ignored recorded op replacing a live output op. */
            lane->operation=operation==HB_MO_HARMONY?HB_MO_TRANSPOSE:HB_MO_HARMONY;
        }
        position=slot%3==0?-1:slot%3==1?0.25:17.75;
        instance->role=slot%2;instance->content_map=slot%3;
        uint8_t message[3]={0x90,(uint8_t)(slot%3==0?1:slot%3==1?60:126),91};
        int pitch[2],velocity[2],pan[2],skip[2];double deadline[2];
        hb_motion_config before,expected;memcpy(&before,&instance->motion,sizeof(before));
        reference_motion_values(instance,message,&pitch[0],&velocity[0],&pan[0],&deadline[0],&skip[0]);
        memcpy(&expected,&instance->motion,sizeof(expected));
        memcpy(&instance->motion,&before,sizeof(before));
        hb_motion_values(instance,message,&pitch[1],&velocity[1],&pan[1],&deadline[1],&skip[1]);
        assert(!memcmp(&expected,&instance->motion,sizeof(expected)));
        assert(pitch[0]==pitch[1]&&velocity[0]==velocity[1]&&pan[0]==pan[1]);
        assert(deadline[0]==deadline[1]&&skip[0]==skip[1]);comparisons++;
    }
    API.destroy_instance(instance);
    printf("Motion output: %u baseline comparisons passed across every operation/lane, patterns, conditions, recorded and held overrides\n",comparisons);
    return 0;
}
