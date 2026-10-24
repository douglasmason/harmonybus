#define main ignored_reference_main
#include "follower_reference_test.c"
#undef main
static void event(Inst *instance,int pitch,int on){
    uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)pitch,(uint8_t)(on?100:0)},output[256][3];int lengths[256];
    assert(API.process_midi(instance,message,3,output,lengths,256)==0);
    g_conductor_block_ready=0;
    assert(API.tick(instance,24000,48000,output,lengths,256)==0);
}
int main(void){
    Inst *conductor=API.create_instance("",0);API.set_param(conductor,"role","Conductor");
    API.set_param(conductor,"follower_root_policy","Explicit");API.set_param(conductor,"follower_explicit_root","C");
    API.set_param(conductor,"follower_scale","Major");API.set_param(conductor,"render_channel","Off");
    const char *commands[]={"0,0,3,3,0,1,12,2,4","0,0,3,3,0,1,12,3,4","0,0,3,3,0,1,12,4,4","0,0,3,3,0,1,12,5,4","0,0,2,3,0,1,12,11,0"};
    const int roots[]={9,2,6,8,10},kinds[]={8,2,3,4,1};
    const unsigned masks[]={0x291,0x245,0x249,0x149,0x424};
    unsigned long long dominant=0;char buffer[8192],recorded[4096],dominant_recording[4096];
    for(int index=0;index<5;index++){
        API.set_param(conductor,"hb_movy_input_chord",commands[index]);
        assert(hb_selection_has_intent(conductor->selection_pending));
        int note=conductor->selection_note;assert(note==60+roots[index]);
        if(index==1)dominant=conductor->selection_pending;
        event(conductor,note,1);hb_harmony_t harmony=hb_render_harmony(conductor);
        fprintf(stderr,"intent %d root=%d mask=%x kind=%d target=%d scale=%x\n",index,harmony.root_pc,hb_harmony_chord_mask(harmony),harmony.intent_kind,harmony.intent_target,harmony.intent_scale);
        assert(harmony.root_pc==roots[index]&&hb_harmony_chord_mask(harmony)==masks[index]);
        assert(harmony.intent_kind==kinds[index]&&harmony.intent_target==(index==4?0:7));
        assert(harmony.chord_index&HB_HARMONY_FIXED_SCALE);
        API.get_param(conductor,"hb_record_action",recorded,sizeof(recorded));
        if(index==1)strcpy(dominant_recording,recorded+4);
        event(conductor,note,0);
    }
    hb_harmony_t frozen=hb_selection_decode(dominant,62);
    API.set_param(conductor,"secondary_scale","Whole Tone");
    assert(hb_parent_chord_scale(conductor,frozen,0,0xab5,0)==frozen.intent_scale);
    assert(hb_local_output_scale_resolved(conductor,frozen,0)==frozen.intent_scale);
    API.set_param(conductor,"hb_movy_actions",dominant_recording);assert(conductor->recorded_action_valid[62]);
    conductor->movy_playback=1;event(conductor,62,1);
    hb_harmony_t replay=hb_render_harmony(conductor);
    assert(replay.intent_kind==2&&replay.intent_target==7&&replay.intent_scale==frozen.intent_scale);
    assert(replay.chord_index&HB_HARMONY_FIXED_SCALE);event(conductor,62,0);conductor->movy_playback=0;
    char opening[4096];int opening_size=snprintf(opening,sizeof(opening),"1;62,0");
    for(int index=0;index<=HB_MOTION_LANES;index++)opening_size+=snprintf(opening+opening_size,sizeof(opening)-opening_size,",%llx",conductor->recorded_actions[62][index]);
    API.set_param(conductor,"hb_opening_preview",opening);hb_harmony_t preview=hb_opening_harmony();
    assert(preview.intent_kind==2&&preview.intent_target==7&&(preview.chord_index&HB_HARMONY_FIXED_SCALE));
    hb_harmony_t shifted=hb_transpose_harmony(frozen,2);assert(shifted.root_pc==4&&shifted.intent_target==9);
    assert(hb_harmony_chord_mask(shifted)==hb_transpose_mask(0x245,2));
    unsigned long long actions[HB_MOTION_LANES+1]={0};actions[0]=dominant;
    assert(hb_selection_actions(actions));actions[1]=1;assert(!hb_selection_actions(actions));actions[1]=0;
    assert(!hb_selection_word(dominant|(1ULL<<46)));
    assert(!hb_selection_word((dominant&~(15ULL<<41))|(15ULL<<41)));
    char mixed[4096];int mixed_size=snprintf(mixed,sizeof(mixed),"63");
    for(int index=0;index<=HB_MOTION_LANES;index++)mixed_size+=snprintf(mixed+mixed_size,sizeof(mixed)-mixed_size,",%llx",index==0?dominant:index==1?1ULL:0ULL);
    API.set_param(conductor,"hb_movy_actions",mixed);assert(!conductor->recorded_action_valid[63]);
    char bank[2048];int used=0;
    for(int index=0;index<16;index++)used+=snprintf(bank+used,sizeof(bank)-used,"%s%s",index?";":"",commands[index%5]);
    API.set_param(conductor,"hb_chord_bank",bank);
    unsigned long long pending=conductor->selection_pending;
    int count=API.get_param(conductor,"hb_chord_bank_snapshot",buffer,sizeof(buffer));
    assert(count>0&&strstr(buffer,"cb1|2741|")&&conductor->selection_pending==pending);
    char *slot=strchr(buffer,'|');slot=strchr(slot+1,'|');slot=strchr(slot+1,'|');
    int families[]={2,3,4,3,1},chromatic[]={0,1,1,1,1};
    for(int index=0;index<5;index++){
        int slot_root,slot_kind,slot_target,slot_bass,family,outside;unsigned mask,scale;
        assert(sscanf(slot+1,"%d,%u,%u,%d,%d,%d,%d,%d",&slot_root,&mask,&scale,&slot_kind,&slot_target,&slot_bass,&family,&outside)==8);
        assert(slot_root==roots[index]&&mask==masks[index]&&family==families[index]&&outside==chromatic[index]);
        slot=strchr(slot+1,'|');
    }
    int explicit_fields[]={2,6,3,2,7,1,12,0,0},note=0;
    hb_harmony_t explicit_harmony=hb_selection_decode(hb_selection_resolve(conductor,explicit_fields,&note),62);
    assert(hb_harmony_chord_mask(explicit_harmony)&~0xab5u); /* G major accommodation must not hide D7's F#. */
    assert(!(hb_harmony_chord_mask(explicit_harmony)&~explicit_harmony.intent_scale));
    int old=conductor->selection_bank[0][7];API.set_param(conductor,"hb_chord_bank","bad");assert(conductor->selection_bank[0][7]==old);
    API.set_param(conductor,"follower_explicit_root","D");
    API.set_param(conductor,"hb_movy_input_chord",commands[1]);
    assert(conductor->selection_note==64); /* V/V moves with the reference key. */
    API.destroy_instance(conductor);
    puts("Chord intent: shared families, exact tones, destination, frozen scale, atomic snapshots and key-relative targets pass");
}
