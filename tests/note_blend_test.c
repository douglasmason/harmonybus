#define HB_PRESSURE_TEST_HELPERS
#include "approach_pressure_test.c"

static int sounding[16][128],bends[16],attacks,releases;
static void observe_blend(const uint8_t message[3]){
    int type=message[0]&240,channel=message[0]&15,pitch=message[1];
    if(type==0x90&&message[2]){sounding[channel][pitch]=1;attacks++;}
    if(type==0x80||(type==0x90&&!message[2])){sounding[channel][pitch]=0;releases++;}
    if(type==0xe0)bends[channel]=(message[2]<<7)|message[1];
}
static void clear_observation(void){
    memset(sounding,0,sizeof(sounding));attacks=releases=0;
    for(int channel=0;channel<16;channel++)bends[channel]=8192;
}
static void pop_blend(hb_note_blend *state){
    uint8_t output[3];int count=0;
    while(hb_nb_pop(state,output)){assert(++count<4096);observe_blend(output);}
}
static void feed_blend(hb_note_blend *state,int status,int pitch,int value){
    uint8_t message[3]={(uint8_t)status,(uint8_t)pitch,(uint8_t)value};
    if(hb_nb_message(state,message))observe_blend(message);
    pop_blend(state);
}
static void settle_blend(hb_note_blend *state){hb_nb_tick(state,1);pop_blend(state);}
static void centroid_and_lifecycle(void){
    hb_note_blend state;hb_nb_init(&state);clear_observation();hb_nb_mode(&state,3);pop_blend(&state);
    feed_blend(&state,0x92,60,100);feed_blend(&state,0x92,72,100);settle_blend(&state);
    assert(attacks==1&&sounding[2][60]&&!sounding[2][72]&&bends[2]==12288);
    feed_blend(&state,0xa2,60,25);feed_blend(&state,0xa2,72,75);settle_blend(&state);
    assert(bends[2]==14336&&attacks==1); /* 1:3 -> pitch 69, anchor 60. */
    feed_blend(&state,0xa2,60,0);feed_blend(&state,0xa2,72,0);settle_blend(&state);
    assert(isfinite(state.current[2])&&attacks==1&&sounding[2][60]);
    feed_blend(&state,0xa2,60,127);settle_blend(&state);assert(bends[2]==8192);
    feed_blend(&state,0x82,60,0);assert(sounding[2][60]); /* Anchor may outlive its input. */
    feed_blend(&state,0xa2,72,127);settle_blend(&state);assert(bends[2]==16383);
    feed_blend(&state,0x82,72,0);assert(!sounding[2][60]&&bends[2]==8192&&releases==1);
    feed_blend(&state,0xa2,72,100);assert(!state.active&&!state.dirty);
    /* Channel isolation, bend span rebasing, and panic with output outstanding. */
    feed_blend(&state,0x90,60,100);feed_blend(&state,0x91,80,100);
    feed_blend(&state,0x90,96,100);settle_blend(&state);
    assert(sounding[0][78]&&!sounding[0][60]&&sounding[1][80]&&bends[1]==8192);
    hb_nb_mode(&state,0);uint8_t output[3];assert(hb_nb_pop(&state,output));observe_blend(output);
    hb_nb_panic(&state);pop_blend(&state);
    for(int channel=0;channel<16;channel++)for(int pitch=0;pitch<128;pitch++)assert(!sounding[channel][pitch]);
}
static void modes_and_steps(void){
    hb_note_blend state;hb_nb_init(&state);clear_observation();
    feed_blend(&state,0x90,60,100);feed_blend(&state,0x90,72,100);assert(attacks==2);
    hb_nb_mode(&state,3);pop_blend(&state);assert(sounding[0][60]&&!sounding[0][72]&&bends[0]==12288);
    hb_nb_mode(&state,0);pop_blend(&state);assert(sounding[0][60]&&sounding[0][72]&&bends[0]==8192);
    hb_nb_mode(&state,1);pop_blend(&state);assert(sounding[0][66]&&!sounding[0][60]&&!sounding[0][72]);
    feed_blend(&state,0xa0,60,0);feed_blend(&state,0xa0,72,100);settle_blend(&state);
    assert(sounding[0][72]&&!sounding[0][66]&&bends[0]==8192);
    feed_blend(&state,0xb0,123,0);pop_blend(&state);assert(!state.active&&state.bend[0]==8192);
    /* Same smoothing result at different audio block sizes. */
    hb_nb_init(&state);hb_nb_mode(&state,5);pop_blend(&state);
    feed_blend(&state,0x90,60,100);feed_blend(&state,0x90,72,100);
    hb_note_blend other=state;
    for(int tick=0;tick<100;tick++)hb_nb_tick(&state,.001);
    hb_nb_tick(&other,.1);assert(fabs(state.current[0]-other.current[0])<1e-9);
}
static void production_drain(Inst *instance){
    uint8_t output[1][3];int lengths[1];
    for(int attempt=0;attempt<400;attempt++)if(API.tick(instance,64,48000,output,lengths,1))observe_blend(output[0]);
}
static void production_event(Inst *instance,int status,int pitch,int value){
    uint8_t message[3]={(uint8_t)status,(uint8_t)pitch,(uint8_t)value},output[1][3];int lengths[1];
    if(API.process_midi(instance,message,3,output,lengths,1))observe_blend(output[0]);
    production_drain(instance);
}
static void production_and_state(void){
    Inst *instance=setup();instance->travel_map=7;instance->content_map=2;clear_observation();
    API.set_param(instance,"note_blend","Bend 12");
    instance->motion.lanes[0].operation=HB_MO_TRANSPOSE;instance->motion.lanes[0].amount=12;
    production_event(instance,0x90,48,100);production_event(instance,0x90,60,100);
    assert(sounding[0][60]&&!sounding[0][72]&&bends[0]==12288);
    production_event(instance,0xa0,48,25);production_event(instance,0xa0,60,75);
    assert(bends[0]==14336&&attacks==1);
    char state[65536],value[64];API.get_param(instance,"state",state,sizeof(state));assert(strstr(state,";nb1,3"));
    hb_stop_instance_note_state(instance);production_drain(instance);assert(!sounding[0][60]&&bends[0]==8192);
    API.set_param(instance,"note_blend","Off");API.set_param(instance,"state",state);
    API.get_param(instance,"note_blend",value,sizeof(value));assert(!strcmp(value,"Bend 12"));
    char *suffix=strstr(state,";nb1,3");memmove(suffix,suffix+6,strlen(suffix+6)+1);
    API.set_param(instance,"state",state);API.get_param(instance,"note_blend",value,sizeof(value));assert(!strcmp(value,"Off"));
    API.set_param(instance,"note_blend","Steps");production_event(instance,0x90,48,100);production_event(instance,0x90,60,100);
    assert(sounding[0][66]);API.set_param(instance,"role","Receiver");production_drain(instance);
    for(int pitch=0;pitch<128;pitch++)assert(!sounding[0][pitch]);API.destroy_instance(instance);
}
static void receiver_bend(void){
    Inst *source=setup(),*receiver=API.create_instance("",0);source->travel_map=7;source->content_map=2;
    API.set_param(source,"render_channel","2");API.set_param(source,"note_blend","Bend 12");
    API.set_param(receiver,"role","Receiver");API.set_param(receiver,"receive_channel","2");
    production_event(source,0x90,60,100);production_event(source,0x90,72,100);
    clear_observation();production_drain(receiver);assert(sounding[0][60]&&bends[0]==12288);
    /* Source loss removes its queued traffic, releases the anchor, centers bend. */
    hb_receiver_remove_source(source);production_drain(receiver);assert(!sounding[0][60]&&bends[0]==8192);
    API.destroy_instance(source);API.destroy_instance(receiver);
}
int main(void){
    centroid_and_lifecycle();modes_and_steps();production_and_state();receiver_bend();
    puts("note blend: weighted pitch, zero pressure, steps, smoothing, rebase, mode edits, transpose, one-message buffers, restore, stop and receiver cleanup passed");return 0;
}
