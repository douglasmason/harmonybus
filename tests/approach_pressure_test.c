#define main reference_fixture_main
#include "follower_reference_test.c"
#undef main
static Inst *setup(void){Inst *instance=fixture();instance->boundary_buffer_ms=0;instance->next_anti_buffer_ms=0;g_bus.observed_harmony=chord(0,0,0);hb_effective_write(g_bus.observed_harmony);return instance;}
static void input(Inst *instance,int source,int on){uint8_t message[3]={(uint8_t)(on?0x90:0x80),(uint8_t)source,(uint8_t)(on?100:0)},output[64][3];int lengths[64];API.process_midi(instance,message,3,output,lengths,64);API.tick(instance,64,48000,output,lengths,64); }

static int pressure(Inst *instance,int source,int amount){
    uint8_t message[3]={0xA0,(uint8_t)source,(uint8_t)amount},output[64][3];int lengths[64];
    return API.process_midi(instance,message,3,output,lengths,64);
}
static void configure(Inst *instance,int mode,int reverse){
    instance->approach_layout=1;instance->preview_count=32;
    API.set_param(instance,"approach_pressure_row","3");
    API.set_param(instance,"approach_pressure_lane","1");
    char text[16];snprintf(text,sizeof(text),"%d",mode);API.set_param(instance,"approach_pressure_mode",text);
    snprintf(text,sizeof(text),"%d",reverse);API.set_param(instance,"approach_pressure_reverse",text);
    instance->motion.lanes[0].operation=HB_MO_VELOCITY;
    instance->motion.lanes[0].amount=-80;instance->motion.lanes[0].enabled=0;
}
static void press(Inst *instance,int source){
    char text[32];snprintf(text,sizeof(text),"%d,-36,3",source);
    API.set_param(instance,"hb_movy_input_approach",text);input(instance,source,1);
}
static double amount(Inst *instance){
    double value=999;return hb_mo_value(&instance->motion,0,0,60,&value)?value:0;
}
int main(void){
    Inst *instance=setup();configure(instance,HB_PRESSURE_PEAK,0);
    press(instance,96);assert(instance->approach_rows.pressure[3].down);
    assert(pressure(instance,96,100)==0);pressure(instance,96,20);input(instance,96,0);
    assert(instance->approach_rows.pressure[3].latched);
    assert(fabs(amount(instance)+80*100.0/127)<.001);
    /* A subsequent ordinary attack hears the latched amount; original pitch
       ownership and the release edge remain in the production MIDI path. */
    uint8_t attack[3]={0x90,64,100},output[64][3];int lengths[64];
    int count=API.process_midi(instance,attack,3,output,lengths,64);
    count+=API.tick(instance,64,48000,output+count,lengths+count,64-count);
    int heard=0;for(int index=0;index<count;index++)if((output[index][0]&240)==0x90&&output[index][2]){assert(output[index][2]==37);heard++;}
    assert(heard==1);input(instance,64,0);
    press(instance,96);assert(amount(instance)==0);pressure(instance,96,40);input(instance,96,0);
    assert(fabs(amount(instance)+80*40.0/127)<.001);
    press(instance,96);input(instance,96,0);assert(!instance->motion.pressure_active);
    configure(instance,HB_PRESSURE_PEAK,1);
    press(instance,96);assert(amount(instance)==-80);
    pressure(instance,96,100);pressure(instance,96,20);input(instance,96,0);
    assert(fabs(amount(instance)+80*27.0/127)<.001);
    press(instance,96);input(instance,96,0);assert(!instance->motion.pressure_active);
    /* Pressure zero retains contact; two pads on a row share the maximum. */
    configure(instance,HB_PRESSURE_HOLD,0);press(instance,96);pressure(instance,96,100);
    press(instance,98);pressure(instance,98,40);input(instance,96,0);
    assert(fabs(amount(instance)+80*40.0/127)<.001);
    pressure(instance,98,0);assert(instance->approach_rows.pressure[3].down);
    input(instance,98,0);assert(!instance->motion.pressure_active);
    /* A light tap latches prepared amount; a squeeze temporarily varies it. */
    configure(instance,HB_PRESSURE_TAP,0);API.set_param(instance,"approach_pressure_prepared","60");
    press(instance,96);input(instance,96,0);assert(fabs(amount(instance)+48)<.001);
    press(instance,96);pressure(instance,96,127);assert(amount(instance)==-80);
    input(instance,96,0);assert(fabs(amount(instance)+48)<.001);
    press(instance,96);input(instance,96,0);assert(!instance->motion.pressure_active);
    /* Settings persist; contacts and latches never revive on reload/Stop. */
    configure(instance,HB_PRESSURE_PEAK,1);press(instance,96);pressure(instance,96,80);input(instance,96,0);
    char saved[32768];API.get_param(instance,"state",saved,sizeof(saved));
    API.set_param(instance,"state",saved);
    assert(instance->approach_rows.pressure_mode[3]==HB_PRESSURE_PEAK);
    assert(instance->approach_rows.pressure_reverse[3]==1&&!instance->motion.pressure_active);
    press(instance,96);pressure(instance,96,60);hb_stop_instance_note_state(instance);
    pressure(instance,96,100);input(instance,96,0);assert(!instance->motion.pressure_active);
    /* Unsupported categorical operations cannot be swept accidentally. */
    instance->motion.lanes[0].operation=HB_MO_SECONDARY_V;
    press(instance,96);pressure(instance,96,127);assert(!instance->motion.pressure_active);input(instance,96,0);
    API.destroy_instance(instance);puts("approach pressure gestures, row ownership and persistence passed");
    return 0;
}
