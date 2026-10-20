#ifndef HB_KEY_VOICING_H
#define HB_KEY_VOICING_H
/* Adapt a complete resolved chord to the SAME assignment used for follower
   Closest/Split. Exact capacities preserve every voice/extension; only its
   assignment to the source register changes. */
static void hb_key_closest_voicing(hb_closest_cache *cache,int *pitches,int count,
                                  const int *reference,int reference_count){
    if(count<1||count>HB_CP_VOICES||reference_count<1)return;
    int nominal[HB_CP_VOICES],mapped[HB_CP_VOICES];unsigned allowed[HB_CP_VOICES],mask=0;
    unsigned char required[12]={0};
    for(int voice=0;voice<count;voice++){
        int pc=hb_cs_mod12(pitches[voice]);required[pc]++;mask|=1u<<pc;
    }
    for(int voice=0;voice<count;voice++){
        nominal[voice]=reference_count==count?reference[voice]:
            reference[0]+(count>1?(reference[reference_count-1]-reference[0])*voice/(count-1):0);
        allowed[voice]=mask;
    }
    if(hb_cached_closest_assignment_required(cache,count,nominal,allowed,required,mapped)){
        for(int voice=0;voice<count;voice++)pitches[voice]=mapped[voice];
        hb_cp_sort(pitches,count);
    }
}
/* Nearest Octave preserves each voice's pitch-class role. Joint allocation
   still prevents two octave-doubled voices from becoming the same MIDI note. */
static void hb_key_travel_voicing(hb_closest_cache *cache,int *pitches,int count,
                                 const int *reference,int reference_count,int policy){
    if(policy<4){hb_key_closest_voicing(cache,pitches,count,reference,reference_count);return;}
    if(count<1||count>HB_CP_VOICES||reference_count<1)return;
    int nominal[HB_CP_VOICES],mapped[HB_CP_VOICES];unsigned allowed[HB_CP_VOICES];
    unsigned char required[12]={0};
    for(int voice=0;voice<count;voice++){
        int pc=hb_cs_mod12(pitches[voice]);required[pc]++;allowed[voice]=1u<<pc;
        int source=reference_count==count?reference[voice]:
            reference[0]+(count>1?(reference[reference_count-1]-reference[0])*voice/(count-1):0);
        nominal[voice]=hb_travel_register(source,pitches[voice],policy);
    }
    if(hb_cached_closest_assignment_required(cache,count,nominal,allowed,required,mapped)){
        memcpy(pitches,mapped,(size_t)count*sizeof(int));hb_cp_sort(pitches,count);
    }
}
#endif
