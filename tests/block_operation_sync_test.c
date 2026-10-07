/* Compare complete event traces with the old all-track-per-tick traversal.
 * Build once normally and once with HB_LEGACY_TICK_SYNC, then compare stdout. */
#define main fixture_main
#include "chord_player_test.c"
#undef main
static unsigned long long digest=14695981039346656037ULL;
static void consume(unsigned long long value){digest^=value;digest*=1099511628211ULL;}
static void collect(int count){
    consume(count);
    for(int event=0;event<count;event++){
        consume(lengths[event]);
        for(int byte=0;byte<lengths[event];byte++)consume(output[event][byte]);
    }
    consume(render_count);consume(recorded_count);
    for(int event=0;event<render_count;event++)for(int byte=0;byte<4;byte++)consume(rendered[event][byte]);
    for(int event=0;event<recorded_count;event++)for(int byte=0;byte<4;byte++)consume(recorded[event][byte]);
    render_count=recorded_count=0;
}
static void edge(Inst *instance,int on,int note){
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)note,(uint8_t)(on?100:0)};
    collect(API.process_midi(instance,message,3,output,lengths,64));
}
int main(void){
    for(int barrier=0;barrier<2;barrier++)for(int playback=0;playback<2;playback++){
        Inst *instances[16];instances[0]=fixture();
        for(int track=1;track<16;track++)instances[track]=API.create_instance("",0);
        for(int track=0;track<16;track++){
            Inst *instance=instances[track];instance->movy_track=track;
            API.set_param(instance,"role",track<4?"Conductor":track<12?"Follower":track<14?"Receiver":"Off");
            API.set_param(instance,"render_channel","4");
            API.set_param(instance,"chord_mode",track%2?"Scale Degree":"Off");
            API.set_param(instance,"motion_operation_1","Auto Chord Repeat");
            API.set_param(instance,"motion_operation_2","Parallel Scale");
            assert(instance->motion.lanes[0].operation==HB_MO_AUTO_CHORD_REPEAT);
            assert(instance->motion.lanes[1].operation==HB_MO_PARALLEL_SCALE);
            API.set_param(instance,"motion_lane","3");
            API.set_param(instance,"motion_operation","Chord/Arp State");
            API.set_param(instance,"motion_amount","Scale Degree Burst");
            API.set_param(instance,"motion_enabled","Off");
            assert(instance->motion.lanes[2].operation==HB_MO_CHORD_STATE);
            /* Time-conditioned operations on an unplayed track must still
               update shared key state before the other tracks tick. */
            if(track==10){
                instance->motion.lanes[4].operation=HB_MO_PARALLEL_SCALE;
                instance->motion.lanes[4].enabled=1;
                instance->motion.lanes[4].every=2;
                instance->motion.lanes[4].from=instance->motion.lanes[4].through=1;
            }
            instance->boundary_buffer_ms=0;
            instance->player.config.playback=playback;
            instance->surface_enabled=1;instance->surface_count[0]=32;
            for(int slot=0;slot<32;slot++){
                instance->surface_notes[0][slot]=48+slot;
                instance->surface_targets[0][slot]=-1;
            }
        }
        collect(0);
        for(int block=0;block<2400;block++){
            transport=block>=1800&&block<2100?MOVE_CLOCK_STATUS_STOPPED:MOVE_CLOCK_STATUS_RUNNING;
            position=(block%1800)*128.0/44100*2;
            int selected=(block/48)%12;
            if(block%48==0){API.set_param(instances[selected],"motion_gesture_1","Touch");collect(0);}
            if(block%48==20){API.set_param(instances[selected],"motion_gesture_1","Release");collect(0);}
            if(block%97==0){API.set_param(instances[selected],"motion_gesture_2","Touch");collect(0);}
            if(block%97==48){API.set_param(instances[selected],"motion_gesture_2","Release");collect(0);}
            if(block%131==0){API.set_param(instances[selected],"motion_gesture_3","Touch");collect(0);}
            if(block%131==30){API.set_param(instances[selected],"motion_gesture_3","Release");collect(0);}
            if(block%211==0){
                API.set_param(instances[0],"key_center_tonic",(block/211)%2?"A":"C");
                API.set_param(instances[0],"key_center_apply","Apply");collect(0);
            }
            if(block%8==0)for(int track=0;track<12;track++){
                edge(instances[track],0,60+track%7);edge(instances[track],1,60+track%7);
            }
            if(barrier){
                char message[80];snprintf(message,sizeof(message),"%d,128,44100",block+1);
                API.set_param(instances[0],"hb_movy_block",message);collect(0);
            }
            for(int track=0;track<16;track++){
                /* Same-block edits and note edges cannot wait for the next barrier. */
                if(track==8&&block%53==0){edge(instances[3],0,63);edge(instances[3],1,63);}
                collect(API.tick(instances[track],128,44100,output,lengths,64));
            }
            if(block%60==0){char view[8192];int count=API.get_param(instances[4],"surface_view0",view,sizeof(view));
                assert(count>0&&count<(int)sizeof(view));for(int byte=0;byte<count;byte++)consume((unsigned char)view[byte]);}
            consume(g_key_context.target_root);consume(g_key_context.target_mask);
            for(int track=0;track<16;track++){
                consume(instances[track]->key_lane_active);
                consume(instances[track]->player.repeat_override);
                consume(instances[track]->player.state_override);
                consume(instances[track]->player.sounding_count);
            }
            printf("%d %d %d %016llx\n",barrier,playback,block,digest);
        }
        for(int track=0;track<16;track++)API.destroy_instance(instances[track]);
    }
}
