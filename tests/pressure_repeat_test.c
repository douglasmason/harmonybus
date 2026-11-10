/* Exercise actual pressure input and emitted MIDI, plus repeat owner edges. */
#define HB_PRESSURE_TEST_HELPERS
#include "approach_pressure_test.c"

static int attacks(hb_motion_route *route){
    uint8_t message[3];int count=0;
    while(hb_mo_pop(route,message))if((message[0]&240)==0x90&&message[2])count++;
    return count;
}
static void route_tick(hb_motion_route *route,hb_motion_config *config,double beat){
    hb_mo_note_repeat_sync(route,config,beat,beat,beat);
    hb_mo_repeat_tick(route,config,beat);
}
static void ownership_and_clock(void){
    hb_motion_config config;hb_mo_defaults(&config);
    hb_motion_route route;hb_mo_route_init(&route);
    config.lanes[0].operation=HB_MO_NOTE_REPEAT;config.lanes[0].amount=16;
    config.lanes[0].enabled=0;config.pressure_active=1;config.pressure_value[0]=.1;config.pressure_rate[0]=1;
    uint8_t on[3]={0x90,60,100},off[3]={0x80,60,0};
    assert(hb_mo_event(&route,on,60,100,-1,-1,0));assert(attacks(&route)==1);
    route_tick(&route,&config,0);route_tick(&route,&config,.99);assert(!attacks(&route));
    route_tick(&route,&config,1);assert(attacks(&route)==1);
    config.pressure_rate[0]=4;config.pressure_value[0]=.5;
    route_tick(&route,&config,1.1);assert(!attacks(&route));
    route_tick(&route,&config,1.25);assert(attacks(&route)==1);
    route_tick(&route,&config,1.5);assert(attacks(&route)==1);
    /* The rate changes in flight without a new source note or catch-up burst. */
    config.pressure_rate[0]=16;config.pressure_value[0]=1;
    route_tick(&route,&config,1.51);route_tick(&route,&config,1.5625);assert(attacks(&route)==1);
    route_tick(&route,&config,100);assert(attacks(&route)==1);
    route_tick(&route,&config,100);assert(!attacks(&route));
    /* An unrelated held note remains owned when the repeating seed releases. */
    uint8_t peer[3]={0x90,67,90};assert(hb_mo_event(&route,peer,67,90,-1,-1,0));attacks(&route);
    assert(hb_mo_event(&route,off,60,0,-1,-1,0));route_tick(&route,&config,100.01);
    assert(!route.refs[0][60]&&route.refs[0][67]);
    route_tick(&route,&config,100.0625);assert(attacks(&route)==1);
    config.pressure_value[0]=0;config.pressure_rate[0]=0;route_tick(&route,&config,100.07);
    assert(!route.note_repeat_pending);route_tick(&route,&config,110);assert(!attacks(&route));
    config.pressure_value[0]=1;config.pressure_rate[0]=16;route_tick(&route,&config,110);
    route_tick(&route,&config,110.0625);assert(attacks(&route)==1);
    /* Reusing an owner slot cannot revive the old seed. */
    hb_mo_panic(&route);attacks(&route);route_tick(&route,&config,111);assert(!attacks(&route)&&!route.owned);
    assert(hb_mo_event(&route,on,62,100,-1,-1,0));attacks(&route);
    route_tick(&route,&config,111.01);route_tick(&route,&config,111.0625);assert(attacks(&route)==1);
    assert(!route.refs[0][60]&&!route.refs[0][67]);hb_mo_panic(&route);
    /* A held chord repeats together, rather than becoming an arpeggio. */
    hb_mo_route_init(&route);config.pressure_rate[0]=4;
    for(int pitch=60;pitch<=68;pitch+=4){on[1]=(uint8_t)pitch;assert(hb_mo_event(&route,on,pitch,100,-1,-1,0));}
    assert(attacks(&route)==3);route_tick(&route,&config,0);route_tick(&route,&config,.25);assert(attacks(&route)==3);
    hb_mo_panic(&route);assert(!route.owned);
    hb_mo_route_init(&route);on[1]=60;
    hb_mo_event(&route,on,60,100,-1,-1,0);attacks(&route);
    route_tick(&route,&config,0);route_tick(&route,&config,.25);assert(attacks(&route)==1);
    hb_mo_event(&route,off,60,0,-1,-1,0);on[1]=62;
    hb_mo_event(&route,on,62,100,-1,-1,0);attacks(&route);
    route_tick(&route,&config,.26);assert(!route.refs[0][60]&&route.refs[0][62]);
    route_tick(&route,&config,.5);assert(attacks(&route)==1);hb_mo_panic(&route);
    unsigned long long captured[HB_MOTION_LANES+1];hb_mo_capture(&config,0,0,60,captured);
    assert(!(captured[0]&HB_MO_RECORDED)); /* Runtime repeat is not a frozen onset amount. */
}
static void pressure_api(void){
    Inst *instance=setup();configure(instance,HB_PRESSURE_HOLD,0);
    API.set_param(instance,"motion_operation","Note Repeat");
    assert(instance->motion.lanes[0].operation==HB_MO_NOTE_REPEAT&&instance->motion.lanes[0].amount==16);
    assert(!instance->motion.lanes[0].enabled);
    press(instance,96);pressure(instance,96,15);assert(instance->motion.pressure_rate[0]==1);
    const int pressures[]={15,38,64,90,127},rates[]={1,2,4,8,16};
    for(int index=0;index<5;index++){
        pressure(instance,96,pressures[index]);assert(instance->motion.pressure_rate[0]==rates[index]);
    }
    pressure(instance,96,15);assert(instance->motion.pressure_rate[0]==1);
    pressure(instance,96,25);pressure(instance,96,26);assert(instance->motion.pressure_rate[0]==1);
    pressure(instance,96,30);assert(instance->motion.pressure_rate[0]==2);
    pressure(instance,96,25);assert(instance->motion.pressure_rate[0]==2);
    pressure(instance,96,20);assert(instance->motion.pressure_rate[0]==1);
    pressure(instance,96,127);
    int count=0;uint8_t messages[64][3];int lengths[64];
    for(int block=0;block<400;block++){
        position+=128.0/48000*2;
        int emitted=API.tick(instance,128,48000,messages,lengths,64);
        for(int index=0;index<emitted;index++)if((messages[index][0]&240)==0x90&&messages[index][2])count++;
    }
    assert(count>=30&&count<=35); /* 1.066s at 120 BPM, 1/64 rate. */
    char label[64];API.get_param(instance,"approach_pressure_value",label,sizeof(label));assert(!strcmp(label,"Repeat 1/64"));
    input(instance,96,0);assert(!instance->motion.pressure_active);
    for(int block=0;block<50;block++){
        position+=128.0/48000*2;
        int emitted=API.tick(instance,128,48000,messages,lengths,64);
        for(int index=0;index<emitted;index++)assert((messages[index][0]&240)!=0x90||!messages[index][2]);
    }
    API.set_param(instance,"approach_pressure_mode","3");API.set_param(instance,"approach_pressure_reverse","1");
    press(instance,96);assert(instance->motion.pressure_rate[0]==16);pressure(instance,96,100);input(instance,96,0);
    assert(instance->approach_rows.pressure[3].latched&&instance->motion.pressure_rate[0]==2);
    press(instance,96);input(instance,96,0);assert(!instance->motion.pressure_active);
    API.set_param(instance,"motion_operation","Repeat Divisions");
    API.set_param(instance,"approach_pressure_mode","1");API.set_param(instance,"approach_pressure_reverse","0");
    press(instance,96);pressure(instance,96,19);assert(instance->motion.pressure_rate[0]==3);
    pressure(instance,96,35);assert(instance->motion.pressure_rate[0]==5);
    API.get_param(instance,"approach_pressure_value",label,sizeof(label));assert(!strcmp(label,"Repeat 5/beat"));
    tempo=60;count=0;
    for(int block=0;block<400;block++){
        position+=128.0/48000;
        int emitted=API.tick(instance,128,48000,messages,lengths,64);
        for(int index=0;index<emitted;index++)if((messages[index][0]&240)==0x90&&messages[index][2])count++;
    }
    assert(count>=5&&count<=6);input(instance,96,0);tempo=120;
    int rate=0;
    for(int division=1;division<=16;division++){
        rate=hb_pressure_repeat_rate((division-.5)/16,16,rate,1);assert(rate==division);
    }
    for(int division=16;division>=1;division--){
        rate=hb_pressure_repeat_rate((division-.5)/16,16,rate,1);assert(rate==division);
    }
    assert(!hb_pressure_repeat_rate(0,16,rate,1));
    assert(hb_pressure_repeat_rate(1,5,0,1)==5&&hb_pressure_repeat_rate(1,5,0,0)==4);
    char saved[32768],metadata[196608];API.get_param(instance,"state",saved,sizeof(saved));
    API.set_param(instance,"state",saved);
    assert(instance->motion.lanes[0].operation==HB_MO_REPEAT_DIVISIONS&&!instance->motion.pressure_active);
    API.get_param(instance,"chain_params",metadata,sizeof(metadata));
    assert(strstr(metadata,"Note Repeat")&&strstr(metadata,"Repeat Divisions")&&strstr(metadata,"Max Repeats / Beat"));
    API.destroy_instance(instance);
}
int main(void){ownership_and_clock();pressure_api();puts("pressure repeat: live rates, hysteresis, reverse latch, clock and note cleanup pass");return 0;}
