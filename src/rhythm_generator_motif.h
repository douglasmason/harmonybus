/* Materialize bounded rhythm slots into the existing motif scheduler. */
static int hb_rg_motif(const hb_mt_phrase *source,const hb_rg_clock *r,double span,hb_mt_phrase *out,double *gates){
    if(!source->count||span<=0)return 0;
    hb_rg_pattern p=hb_rg_get(r->pattern);*out=*source;
    int anchor=source->anchor,body=anchor>0?anchor:source->count-1;
    int tail=source->count-anchor;if(tail<0||p.count+tail>HB_MT_STEPS+2)return 0;
    int attacks=0;for(int k=0;k<p.count;k++)if(p.steps[k].kind==1)attacks++;
    int attack=0;double unit=span/p.count;
    for(int k=0;k<p.count;k++){
        hb_mt_event e={0};e.kind=p.steps[k].kind==2?2:1;
        if(p.steps[k].kind==1&&unit>=0.0625){
            int index=r->source?anchor:body>0?(attack*body/(attacks?attacks:1)):anchor;
            if(index<source->count)e=source->events[index];
            else {e.kind=0;e.count=1;e.chord_mode=3;e.notes[0].pitch=source->reference_pitch;e.notes[0].velocity=100;}
            for(int n=0;n<e.count;n++){int velocity=e.notes[n].velocity*p.steps[k].accent/100;e.notes[n].velocity=velocity>127?127:velocity<1?1:velocity;}
            attack++;
        }
        out->events[k]=e;out->durations[k]=unit;gates[k]=hb_rg_gate(&p,k);
    }
    for(int k=0;k<tail;k++){out->events[p.count+k]=source->events[anchor+k];out->durations[p.count+k]=hb_mt_duration(source,anchor+k,0,0);gates[p.count+k]=1;}
    out->count=p.count+tail;out->anchor=p.count;out->timed=1;return 1;
}
