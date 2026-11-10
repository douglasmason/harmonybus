#ifndef HB_PRESSURE_CONTROL_H
#define HB_PRESSURE_CONTROL_H
/* Normalized pressure gestures. Contact edges, not pressure zero, own lifetime.
   Peak is measured before inversion, so Reverse Peak retains a minimum output. */
enum { HB_PRESSURE_OFF, HB_PRESSURE_HOLD, HB_PRESSURE_TAP, HB_PRESSURE_PEAK };
typedef struct {
    int down,active,latched,was_latched,squeezed;
    double value,peak,saved,started;
} hb_pressure_control;
static void hb_pressure_begin(hb_pressure_control *state,double now){
    state->was_latched=state->latched;state->saved=state->value;
    state->down=1;state->active=1;state->latched=0;
    state->value=state->peak=0;state->squeezed=0;state->started=now;
}
static void hb_pressure_update(hb_pressure_control *state,int mode,double pressure){
    if(!state->down)return;
    pressure=pressure>=0?(pressure<=1?pressure:1):0;
    if(pressure>state->peak)state->peak=pressure;
    if(pressure>=8.0/127.0)state->squeezed=1;
    state->value=mode==HB_PRESSURE_PEAK?state->peak:pressure;
}
static void hb_pressure_end(hb_pressure_control *state,int mode,double now,double hold_ms,double prepared){
    if(!state->down)return;
    int tap=!state->squeezed&&now-state->started<hold_ms;
    state->down=0;state->active=state->latched=0;
    if(mode==HB_PRESSURE_PEAK){
        if(!tap){state->active=state->latched=1;state->value=state->peak;}
    }else if(mode==HB_PRESSURE_TAP){
        if(tap&&!state->was_latched){state->active=state->latched=1;state->value=prepared;}
        else if(!tap&&state->was_latched){state->active=state->latched=1;state->value=state->saved;}
    }
}
static double hb_pressure_value(const hb_pressure_control *state,int reverse){
    return state->active?(reverse?1-state->value:state->value):0;
}
/* Straight beat subdivisions, with a small dead band at pressure boundaries. */
static int hb_pressure_repeat_rate(double pressure,int maximum,int previous,int all_divisions){
    if(pressure<=0)return 0;
    if(all_divisions){
        if(maximum<1)maximum=1;if(maximum>16)maximum=16;
        int rate=previous;if(rate>maximum)rate=maximum;
        if(!rate){rate=1+(int)(pressure*maximum);if(rate>maximum)rate=maximum;}
        else {
            while(rate<maximum&&pressure>=(double)rate/maximum+3.0/127.0)rate++;
            while(rate>1&&pressure<(double)(rate-1)/maximum-3.0/127.0)rate--;
        }
        return rate;
    }
    int highest=0;while(highest<4&&(2<<highest)<=maximum)highest++;
    int level=0;while(level<highest&&(1<<level)<previous)level++;
    if(!previous){level=(int)(pressure*(highest+1));if(level>highest)level=highest;}
    else {
        while(level<highest&&pressure>=(double)(level+1)/(highest+1)+3.0/127.0)level++;
        while(level>0&&pressure<(double)level/(highest+1)-3.0/127.0)level--;
    }
    return 1<<level;
}
#endif
