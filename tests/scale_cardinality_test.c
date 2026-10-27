#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main
int main(void){
    Inst *instance=fixture();
    const int degrees[2][8]={{0,1,3,4,6,7,9,10},{0,2,3,5,6,8,9,11}};
    assert(HB_SCALE_COUNT==22&&HB_PARALLEL_COUNT==23&&HB_PARALLEL_SCALE_IDS[18]==0);
    assert(HB_SCALE_MASKS[19]==0x9b5&&HB_SCALE_MASKS[20]==0x6db&&HB_SCALE_MASKS[21]==0xb6d);
    for(int tonic=0;tonic<12;tonic++)for(int scale=0;scale<2;scale++){
        unsigned mask=hb_explicit_scale_mask(tonic,20+scale);hb_set_shared_follower_scale(20+scale);
        char label[8];snprintf(label,sizeof(label),"%d",tonic);API.set_param(instance,"follower_explicit_root",label);
        for(int degree=0;degree<8;degree++){
            int note=48+tonic+degrees[scale][degree];
            assert(hb_input_degree(instance,note,tonic,mask)==degree);
            assert(hb_context_degree_interval(instance,mask,tonic,degree)==degrees[scale][degree]);
            assert(hb_context_degree_interval(instance,mask,tonic,degree+8)==12+degrees[scale][degree]);
            hb_harmony_t harmony={.valid=1,.root_pc=tonic,.pitch_mask=mask,.chord_index=HB_HARMONY_EXPLICIT_TONES|HB_HARMONY_FIXED_SCALE,.intent_scale=mask};
            assert(hb_context_render_degree(instance,mask,harmony,degree)==degrees[scale][degree]);
            hb_harmony_t content=harmony;content.pitch_mask=mask;
            for(int split=0;split<4;split++){
                int mapped=hb_map_follower_note_classified(instance,note,harmony,content,split);
                assert(mapped>=0&&mapped<128);
            }
            assert(hb_map_follower_note_relative(instance,note,harmony,content,1)==note);
        }
        int notes[HB_CP_VOICES];unsigned semantic=0;
        hb_cp_config config={.mode=1,.size=3};
        assert(hb_cp_voice_semantic(config,60+tonic,tonic,0,mask,notes,&semantic)==4);
        assert(semantic==hb_transpose_mask(scale?0x249:0x491,tonic));
        char state[65536];API.get_param(instance,"state",state,sizeof(state));
        hb_set_shared_follower_scale(1);g_scale_restored=0;API.set_param(instance,"state",state);assert(hb_shared_follower_scale()==20+scale);
    }
    API.set_param(instance,"hb_movy_input_role","70,7,-1");assert(instance->movy_input_degree[70]==8);
    assert(!strcmp(hb_role_name_for_degree(7),"8th"));
    API.set_param(instance,"role","Conductor");hb_set_shared_follower_scale(20);
    API.set_param(instance,"follower_explicit_root","C");hb_effective_write((hb_harmony_t){0});
    hb_mt_phrase phrase={.count=1,.anchor=0};
    phrase.events[0]=(hb_mt_event){.duration=24,.count=1,.secondary=1,.notes={{60,100}}};
    assert(hb_mt_schedule(instance,&phrase,60,100,0,0,1,position,position,0));
    int scheduled=0;for(int i=0;i<HB_MT_SCHEDULE;i++)if(instance->motif.events[i].used){assert(instance->motif.events[i].pitch==61);scheduled++;}
    assert(scheduled==1);
    API.destroy_instance(instance);puts("Six/eight cardinality: stable scale IDs, every tonic/degree, chord policy, bridge and save/restore pass");
}
