/* Render the same rhythm vocabulary into explicit clip attacks and rests. */
static int hb_rg_clip(hb_mc_plan *plan,const hb_rg_clock *r,const hb_mt_event *target){
    hb_mc_plan source=*plan,out={.target_first=-1,.target_last=-1};hb_rg_pattern p=hb_rg_get(r->pattern);
    int total=(int)(hb_rg_span(r)*24),body=source.target_last>=0?source.target_last:source.count;
    int first=source.target_first>=0?source.target_first+1:0;
    if(source.target_first>=0){hb_mc_event e=source.events[source.target_first];out.target_first=0;if(!hb_mc_append(&out,&e.event,e.source_step,e.duration))return 0;}
    body-=first;
    int attacks=0;for(int k=0;k<p.count;k++)if(p.steps[k].kind==1)attacks++;
    int ordinal=0;
    for(int k=0;k<p.count;k++){
        int onset=total*k/p.count,end=total*(k+1)/p.count;
        hb_mt_event event={.kind=1};int source_step=-2,gate=end-onset;
        if(p.steps[k].kind==1&&end-onset>=2){
            int selected=r->source?(source.target_last>=0?source.target_last:source.target_first):(body>0?first+ordinal*body/(attacks?attacks:1):-1);ordinal++;
            if(selected<0||selected>=source.count){if(!target)return 0;event=*target;source_step=-1;}
            else {event=source.events[selected].event;source_step=source.events[selected].source_step;}
            for(int voice=0;voice<event.count;voice++){int v=event.notes[voice].velocity*p.steps[k].accent/100;event.notes[voice].velocity=v>127?127:v<1?1:v;}
            double duration=hb_rg_gate(&p,k)*total/p.count;gate=(int)(duration+0.5);if(gate<1)gate=1;
            while(k+1<p.count&&p.steps[k+1].kind==2)k++;
            end=total*(k+1)/p.count;if(gate>end-onset)gate=end-onset;
        }
        if(!hb_mc_append(&out,&event,source_step,gate))return 0;
        if(gate<end-onset){hb_mt_event rest={.kind=1};if(!hb_mc_append(&out,&rest,-2,end-onset-gate))return 0;}
    }
    if(source.target_last>=0){out.target_last=out.count;hb_mc_event e=source.events[source.target_last];if(!hb_mc_append(&out,&e.event,e.source_step,e.duration))return 0;}
    for(int k=0;k<out.count;k++){out.events[k].onset=out.duration;out.duration+=out.events[k].duration;}
    *plan=out;return 1;
}
