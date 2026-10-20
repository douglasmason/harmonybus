/* Register policies keep the chosen destination pitch class intact. At a MIDI
   boundary use the only available octave rather than changing chord identity. */
static int hb_travel_register(int source,int mapped,int policy){
    if(policy!=4&&policy!=5&&policy!=6)return mapped;
    int note=hb_cs_nearest_pc(source,mod12(mapped));
    if(policy==5&&note<source&&note+12<=127)note+=12;
    if(policy==6&&note>source&&note-12>=0)note-=12;
    return note;
}
/* Key changes act on parent-key coordinates before approach construction.
   All three playback contexts share one mapper; no policy is baked into notes. */
static int hb_key_follower_travel(const Inst *instance){
    (void)instance;
    int policy=g_key_follower_travel;
    return policy<0?g_key_conductor_travel:policy;
}
static int hb_key_class_pitch(Inst *instance,int pitch,hb_key_context context,hb_harmony_t harmony,int split){
    int nominal[12],mapped[12],source_degree=-1,count=0,target_count=0,target_pc[12];
    unsigned allowed[12],chord=hb_harmony_chord_mask(harmony),active=0;
    for(int interval=0;interval<12;interval++)if(context.target_mask&(1u<<mod12(context.target_root+interval)))
        target_pc[target_count++]=mod12(context.target_root+interval);
    if(!target_count)return pitch;
    if(split==3){
        uint8_t notes[64];int voices=hb_observed_notes(0,notes,64);
        for(int index=0;index<voices;index++)active|=1u<<mod12(hb_key_map(context,notes[index]));
    }
    unsigned degree_group=split==2?0x55u:0x15u;
    unsigned target_group=0;
    for(int degree=0;degree<target_count;degree++)if(degree_group&(1u<<degree))target_group|=1u<<target_pc[degree];
    for(int interval=0;interval<12;interval++)if(context.source_mask&(1u<<mod12(context.source_root+interval))){
        int pc=mod12(context.source_root+interval),on=(degree_group&(1u<<count))!=0;
        unsigned pool=split==1||split==2?target_group:chord;
        if(split==3){pool=active;on=(active&(1u<<target_pc[count%target_count]))!=0;}
        allowed[count]=on?pool:(context.target_mask&~pool);
        if(!allowed[count])allowed[count]=context.target_mask;
        nominal[count]=48+context.source_root+interval;
        if(pc==mod12(pitch))source_degree=count;
        count++;
    }
    if(source_degree<0)return hb_cs_nearest(pitch,context.target_mask);
    if(!hb_cached_closest_assignment(&instance->closest_assignments,count,nominal,allowed,mapped))return hb_cs_nearest(pitch,allowed[source_degree]);
    return hb_cs_nearest_pc(pitch,mod12(mapped[source_degree]));
}
static int hb_key_travel_pitch(Inst *instance,int pitch,hb_harmony_t harmony,int policy,int active_family){
    hb_key_context context=hb_key_for(instance);
    if(!context.active||policy==0)
        return active_family?hb_key_active_pitch(instance,pitch,harmony):hb_key_map(context,pitch);
    if(policy==4||policy==5||policy==6){
        int mapped=active_family?hb_key_active_pitch(instance,pitch,harmony):hb_key_map(context,pitch);
        return hb_travel_register(pitch,mapped,policy);
    }
    if(active_family&&!context.blues){
        unsigned active=hb_dominant_scale_mask(instance,harmony,context.target_root);
        if(active)context.target_mask=active;
    }
    /* Key-center Closest Chord Tone is inversion-like travel within input
       classes, not a chord-only quantizer for every degree. */
    if((policy==1||policy==3)&&harmony.valid)
        return hb_key_class_pitch(instance,pitch,context,harmony,policy==1?0:instance->follower_split_map);
    unsigned target=policy==1&&harmony.valid?hb_harmony_chord_mask(harmony):context.target_mask;
    if(!target)return hb_key_map(context,pitch);
    return hb_closest_diverse(instance,pitch,context.source_root,context.source_mask,target);
}
