/* Physical/recorded pressure must follow the existing sounding voice owner. */
#define HB_PRESSURE_TEST_HELPERS
#include "approach_pressure_test.c"

static uint8_t messages[512][3];
static int message_count;
static void event(Inst *instance,int status,int pitch,int value){
    uint8_t input_message[3]={(uint8_t)status,(uint8_t)pitch,(uint8_t)value};
    int lengths[128];
    message_count=API.process_midi(instance,input_message,3,messages,lengths,128);
    message_count+=API.tick(instance,64,48000,messages+message_count,lengths,128);
}
static int pressure_count(int channel,int pitch,int value){
    int count=0;
    for(int index=0;index<message_count;index++)
        if(messages[index][0]==(0xa0|channel)&&messages[index][1]==pitch&&messages[index][2]==value)count++;
    return count;
}
static void mapped_owners(void){
    Inst *instance=setup();instance->travel_map=7;instance->content_map=2;
    instance->motion.lanes[0].operation=HB_MO_TRANSPOSE;
    instance->motion.lanes[0].amount=12;
    event(instance,0x90,60,100);
    event(instance,0xa0,60,45);
    assert(pressure_count(0,72,45)==1);
    assert(pressure_count(0,60,45)==0);
    /* Pressure follows captured onset operations, not their edited settings. */
    instance->motion.lanes[0].amount=-12;
    event(instance,0xa0,60,0);assert(pressure_count(0,72,0)==1);
    assert(instance->motion_local.refs[0][72]);
    instance->movy_playback=1;event(instance,0x90,60,90);
    event(instance,0xa0,60,31);assert(pressure_count(0,48,31)==1&&pressure_count(0,72,31)==0);
    instance->movy_playback=0;event(instance,0xa0,60,67);
    assert(pressure_count(0,72,67)==1&&pressure_count(0,48,67)==0);
    /* The final shared transpose is applied exactly once. */
    instance->motion.lanes[1].operation=HB_MO_GLOBAL_TRANSPOSE;
    instance->motion.lanes[1].amount=6;
    event(instance,0xa0,60,51);assert(pressure_count(0,78,51)==1);
    event(instance,0x80,60,0);event(instance,0xa0,60,80);assert(message_count==0);
    instance->movy_playback=1;event(instance,0xa0,60,62);assert(pressure_count(0,54,62)==1);
    event(instance,0x80,60,0);event(instance,0xa0,60,80);assert(message_count==0);
    API.destroy_instance(instance);
}
static void harmony_and_bounds(void){
    Inst *instance=setup();instance->travel_map=0;instance->content_map=2;
    hb_effective_write(chord(2,0,0));event(instance,0x90,60,100);
    int target=instance->follower_voices[0].pitch;assert(target!=60);
    event(instance,0xa0,60,55);assert(pressure_count(0,target,55)==1);
    instance->retrigger_held=0;hb_effective_write(chord(5,0,0));
    event(instance,0xa0,60,32);assert(pressure_count(0,target,32)==1);
    event(instance,0x80,60,0);event(instance,0xa0,60,22);assert(message_count==0);
    API.destroy_instance(instance);
    instance=setup();instance->travel_map=7;instance->content_map=2;
    instance->motion.lanes[0].operation=HB_MO_GLOBAL_TRANSPOSE;instance->motion.lanes[0].amount=24;
    event(instance,0x90,120,100);event(instance,0xa0,120,90);assert(message_count==0);
    instance->motion.lanes[0].amount=0;
    event(instance,0xa0,120,41);assert(pressure_count(0,120,41)==1);
    hb_stop_instance_note_state(instance);event(instance,0xa0,120,41);
    assert(pressure_count(0,120,41)==0);
    API.destroy_instance(instance);
}
static uint8_t packets[256][4];static int packet_count;
static int capture_pressure(const uint8_t *packet,int length){
    assert(length==4&&packet_count<256);memcpy(packets[packet_count++],packet,4);return length;
}
static void render_and_receiver(int chord_mode){
    host.midi_inject_to_move=capture_pressure;
    Inst *instance=setup();instance->travel_map=7;instance->content_map=2;instance->movy_track=0;
    API.set_param(instance,"render_channel","3");
    if(chord_mode){
        API.set_param(instance,"chord_mode","Conductor Chord");
        API.set_param(instance,"chord_form","Triad");API.set_param(instance,"chord_inversion","Root");
        API.set_param(instance,"arp_playback","Together");API.set_param(instance,"chord_register","0");
    }
    instance->motion.lanes[0].operation=HB_MO_TRANSPOSE;instance->motion.lanes[0].amount=12;
    Inst *receiver=API.create_instance("",0);API.set_param(receiver,"role","Receiver");
    API.set_param(receiver,"receive_channel","3");
    packet_count=0;event(instance,0x90,60,100);
    int held[128]={0},voices=0;
    for(int index=0;index<packet_count;index++)if(packets[index][1]==0x92&&packets[index][3]){held[packets[index][2]]=1;voices++;}
    assert(voices==(chord_mode?3:1));
    uint8_t output[1][3];int lengths[1];
    for(int tick=0;tick<16;tick++)API.tick(receiver,64,48000,output,lengths,1);
    packet_count=0;event(instance,0xa0,60,43);
    assert(packet_count==voices);
    for(int index=0;index<packet_count;index++)assert(packets[index][0]==0x2a&&packets[index][1]==0xa2&&held[packets[index][2]]&&packets[index][3]==43);
    int received=0;
    for(int tick=0;tick<16;tick++)if(API.tick(receiver,64,48000,output,lengths,1)){
        assert(output[0][0]==0xa0&&held[output[0][1]]&&output[0][2]==43);received++;
    }
    assert(received==voices);
    for(int pitch=0;pitch<128;pitch++)if(held[pitch])assert(receiver->receiver_sounding[pitch]);
    event(instance,0x80,60,0);packet_count=0;event(instance,0xa0,60,99);assert(!packet_count);
    for(int tick=0;tick<16;tick++)API.tick(receiver,64,48000,output,lengths,1);
    for(int pitch=0;pitch<128;pitch++)assert(!receiver->receiver_sounding[pitch]);
    API.destroy_instance(receiver);API.destroy_instance(instance);host.midi_inject_to_move=0;
}
static void filtered_and_skipped(void){
    Inst *instance=setup();instance->travel_map=7;instance->content_map=2;
    API.set_param(instance,"source_channel","1");
    event(instance,0x91,60,100);
    event(instance,0xa1,60,72);assert(pressure_count(1,60,72)==1);
    event(instance,0x81,60,0);
    event(instance,0xa0,60,72);assert(message_count==0);
    instance->motion.lanes[0].operation=HB_MO_SKIP;instance->motion.lanes[0].amount=1;
    event(instance,0x90,60,100);event(instance,0xa0,60,72);assert(message_count==0);
    API.destroy_instance(instance);
}
static void delayed_pressure(void){
    Inst *instance=setup();hb_motion_route *route=&instance->motion_local;
    route->rhythm_enabled=1;route->rhythm_delay=.25;
    uint8_t attack[3]={0x90,60,100},output[3];
    assert(hb_mo_event_owned(route,attack,72,100,-1,-1,0,hb_fv_identity(60,0,0)));
    hb_note_pressure_route(instance,route,0,60,58,0);
    assert(!hb_mo_pop(route,output));
    route->rhythm_now=.25;
    assert(hb_mo_pop(route,output)&&output[0]==0x90&&output[1]==72);
    assert(hb_mo_pop(route,output)&&output[0]==0xa0&&output[1]==72&&output[2]==58);
    assert(!hb_mo_pop(route,output));
    API.destroy_instance(instance);
}
int main(void){
    mapped_owners();harmony_and_bounds();render_and_receiver(0);render_and_receiver(1);filtered_and_skipped();delayed_pressure();
    puts("note pressure: onset/clip owners, mapping, transpose, zero, bounds, chord fan-out, Render To, receiver and cleanup passed");
    return 0;
}
