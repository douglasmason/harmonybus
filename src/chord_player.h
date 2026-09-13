#ifndef HB_CHORD_PLAYER_H
#define HB_CHORD_PLAYER_H
/* Allocation-free chord voicing and owned-note playback. Times are supplied
   by the host adapter; the adapter decides which role owns the output. */
#define HB_CP_KEYS 16
#define HB_CP_VOICES 12
typedef struct {
    int mode, size, inversion, voicing, playback, latch, order, rate, gate, spread, phase;
    int quality, chromatic_quality;
} hb_cp_config;
typedef struct {
    int used, held, source, channel, velocity, count, fresh, root_pc, recordable;
    unsigned sequence, harmony_mask, harmony_sequence;
    int harmony_root;
    int notes[HB_CP_VOICES];
    double due[HB_CP_VOICES];
    unsigned started;
} hb_cp_key;
typedef struct {
    hb_cp_config config;
    hb_cp_key keys[HB_CP_KEYS];
    uint8_t sounding[16][128], retrigger[16][128];
    int sounding_channels[16];
    int sounding_count, flushing, render_channel, step, running, arp_note, arp_channel;
    unsigned random, sequence;
    double seconds, beat, next_beat, gate_beat;
} hb_chord_player;
static int hb_cp_mod(int value){value%=12;return value<0?value+12:value;}
static int hb_cp_abs(int value){return value<0?-value:value;}
static double hb_cp_floor(double value){
    long long whole=(long long)value;return (double)whole-(value<(double)whole);
}
static double hb_cp_division(int index){return 0.0625*(1u<<index);}
static int hb_cp_nearest(int note,unsigned mask){
    for(int distance=0;distance<128;distance++){
        if(note-distance>=0&&(mask&(1u<<hb_cp_mod(note-distance))))return note-distance;
        if(note+distance<128&&(mask&(1u<<hb_cp_mod(note+distance))))return note+distance;
    }
    return note;
}
static void hb_cp_sort(int *notes,int count){
    for(int index=1;index<count;index++){
        int pitch=notes[index],slot=index;
        while(slot>0&&notes[slot-1]>pitch){notes[slot]=notes[slot-1];slot--;}
        notes[slot]=pitch;
    }
}
/* Inversion: Auto, Root, First through Sixth. Auto is From Key for
   Conductor Chord and root position for Scale Root. Preserve the chosen bass
   through spread voicings. Shift the WHOLE voicing at MIDI range edges. */
static int hb_cp_voice(hb_cp_config config,int input,int root,unsigned chord,
                       unsigned scale,int *output){
    if(input<0)input=0;if(input>127)input=127;
    if(config.mode==0){output[0]=input;return 1;}
    int ordered[12],count=0,bass=input,tones[7];
    if(config.mode==1){if(!scale)return 0;root=hb_cp_mod(input);}
    else if(!chord)return 0;
    tones[0]=root;int degree=1;
    for(int offset=1;offset<=24&&degree<7;offset++)
        if(scale&(1u<<hb_cp_mod(root+offset)))tones[degree++]=hb_cp_mod(root+offset);
    if(degree<7){static const int major[7]={0,2,4,5,7,9,11};for(int index=1;index<7;index++)tones[index]=hb_cp_mod(root+major[index]);}
    unsigned relative_scale=0;for(int interval=0;interval<12;interval++)if(scale&(1u<<hb_cp_mod(root+interval)))relative_scale|=1u<<interval;
    if(relative_scale==0x55Bu){static const int altered[7]={0,1,4,6,6,8,10};for(int index=0;index<7;index++)tones[index]=hb_cp_mod(root+altered[index]);}
    int recognized_seventh=config.mode==2&&
        ((chord&(1u<<hb_cp_mod(root+9)))||(chord&(1u<<hb_cp_mod(root+10)))||
         (chord&(1u<<hb_cp_mod(root+11))));
    if(config.mode==2){
        /* Preserve the recognized chord's defining third, fifth and seventh;
           fill newly requested extensions from its parent scale. */
        static const int roles[3]={2,4,6};
        static const int choices[3][4]={{4,3,5,2},{7,6,8,-1},{10,11,-1,-1}};
        for(int role=0;role<3;role++)for(int choice=0;choice<4;choice++){
            int interval=choices[role][choice];
            if(interval>=0&&(chord&(1u<<hb_cp_mod(root+interval)))){tones[roles[role]]=hb_cp_mod(root+interval);break;}
        }
    }
    /* Quality overrides apply to Scale Root gestures. Chromatic keys use a
       selectable triad/seventh family instead of an arbitrary rotated scale.
       Parent-scale upper extensions remain available to Ninth/Add9 etc. */
    if(config.mode==1||config.quality){
        int quality=config.quality;
        if(config.mode==1&&!quality&&!(scale&(1u<<hb_cp_mod(input)))){
            static const int chromatic_qualities[]={0,5,6,9};
            quality=chromatic_qualities[config.chromatic_quality];
        }
        if(quality){
            static const int third[]={0,4,3,3,4,4,4,3,3,3};
            static const int fifth[]={0,7,7,6,8,7,7,7,6,6};
            static const int seventh[]={0,11,10,9,10,11,10,10,10,9};
            tones[2]=hb_cp_mod(root+third[quality]);
            tones[4]=hb_cp_mod(root+fifth[quality]);
            tones[6]=hb_cp_mod(root+seventh[quality]);
        }
    }
    /* Form: Auto, Power, Triad, Seventh, Ninth, Add9, Sixth, 6/9,
       Eleventh, Thirteenth, Sus2, Sus4. Values are zero-based scale degrees. */
    static const int forms[12][7]={
        {0,2,4,-1,-1,-1,-1},{0,4,-1,-1,-1,-1,-1},{0,2,4,-1,-1,-1,-1},
        {0,2,4,6,-1,-1,-1},{0,2,4,6,1,-1,-1},{0,2,4,1,-1,-1,-1},
        {0,2,4,5,-1,-1,-1},{0,2,4,5,1,-1,-1},{0,2,4,6,1,3,-1},
        {0,2,4,6,1,3,5},{0,1,4,-1,-1,-1,-1},{0,3,4,-1,-1,-1,-1}
    };
    unsigned selected=0;
    if(config.mode==2&&config.size==0&&!config.quality){
        static const int order[7]={0,2,4,6,1,3,5};
        for(int index=0;index<7;index++){
            int pitch=tones[order[index]];
            if((chord&(1u<<pitch))&&!(selected&(1u<<pitch))){ordered[count++]=pitch;selected|=1u<<pitch;}
        }
        for(int interval=0;interval<12;interval++){
            int pitch=hb_cp_mod(root+interval);
            if((chord&(1u<<pitch))&&!(selected&(1u<<pitch))){ordered[count++]=pitch;selected|=1u<<pitch;}
        }
    }else for(int index=0;index<7;index++){
        /* Auto on a recognized seventh keeps four voices when changing its
           quality; an explicit form always chooses its own degree set. */
        int form=config.size;
        if(config.mode==2&&config.quality&&form==0){
            form=recognized_seventh?3:2;
        }
        int role=forms[form][index];if(role<0)break;
        int pitch=tones[role];
        if(!(selected&(1u<<pitch))){ordered[count++]=pitch;selected|=1u<<pitch;}
    }
    if(!count)return 0;
    int inversion=0;
    if(config.inversion>0){
        inversion=(config.inversion-1)%count;
        if(config.mode==1){
            bass=input;
            if(inversion)bass-=hb_cp_mod(root-ordered[inversion]);
        }else bass=hb_cp_nearest(input,1u<<ordered[inversion]);
    }else if(config.mode==2){
        bass=hb_cp_nearest(input,selected);
        for(int index=0;index<count;index++)if(ordered[index]==hb_cp_mod(bass))inversion=index;
    }
    int fifth=tones[4];
    if(config.voicing==3){
        /* Shell removes redundant fifths/extensions, but cannot discard the
           explicitly selected bass. Sixth forms retain 6 when there is no 7. */
        unsigned shell=(1u<<root)|(1u<<hb_cp_mod(bass));
        if(selected&(1u<<tones[2]))shell|=1u<<tones[2];
        else shell|=1u<<tones[4];
        if(selected&(1u<<tones[6]))shell|=1u<<tones[6];
        else if(selected&(1u<<tones[5]))shell|=1u<<tones[5];
        int kept=0;
        for(int index=0;index<count;index++)if(shell&(1u<<ordered[index]))ordered[kept++]=ordered[index];
        count=kept;
        for(int index=0;index<count;index++)if(ordered[index]==hb_cp_mod(bass))inversion=index;
    }
    for(int index=0;index<count;index++){
        int pitch=ordered[(inversion+index)%count];
        output[index]=bass+hb_cp_mod(pitch-hb_cp_mod(bass));
        if(index>0&&config.voicing==1&&pitch!=root&&pitch!=fifth)output[index]+=12;
        if(index>0&&config.voicing==2&&(index%2))output[index]+=12;
    }
    hb_cp_sort(output,count);
    while(output[0]<0)for(int index=0;index<count;index++)output[index]+=12;
    while(output[count-1]>127)for(int index=0;index<count;index++)output[index]-=12;
    return output[0]>=0?count:0;
}
static void hb_cp_defaults(hb_cp_config *config){
    memset(config,0,sizeof(*config));config->rate=2;config->gate=1;
}
static int hb_cp_enabled(const hb_chord_player *player){
    return player->config.mode||player->config.playback;
}
static void hb_cp_clear(hb_chord_player *player){
    memset(player->keys,0,sizeof(player->keys));
    memset(player->retrigger,0,sizeof(player->retrigger));
    player->running=0;player->flushing=1;player->step=0;
}
static int hb_cp_held(const hb_chord_player *player){
    int count=0;for(int key=0;key<HB_CP_KEYS;key++)count+=player->keys[key].used&&player->keys[key].held;
    return count;
}
static int hb_cp_on(hb_chord_player *player,int source,int channel,int velocity,
                    const int *notes,int count){
    if(count<=0)return 1;
    if(player->config.latch&&!hb_cp_held(player)){
        memcpy(player->retrigger,player->sounding,sizeof(player->retrigger));
        memset(player->keys,0,sizeof(player->keys));player->running=0;player->step=0;
    }
    int slot=-1;
    for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *key=&player->keys[index];
        if(key->used&&key->source==source&&key->channel==channel){slot=index;break;}
        if(!key->used&&slot<0)slot=index;
    }
    if(slot<0)return 0;
    hb_cp_key *key=&player->keys[slot];
    if(key->used)for(int voice=0;voice<key->count;voice++){
        int pitch=key->notes[voice],shared=0;
        for(int other=0;other<HB_CP_KEYS;other++)if(other!=slot&&player->keys[other].used&&player->keys[other].channel==channel)
            for(int tone=0;tone<player->keys[other].count;tone++)if(player->keys[other].notes[tone]==pitch)shared=1;
        if(!shared)player->retrigger[channel][pitch]=1;
    }
    memset(key,0,sizeof(*key));
    key->used=key->held=key->fresh=1;key->source=source;key->channel=channel;
    key->root_pc=hb_cp_mod(source);key->velocity=velocity;key->count=count;key->sequence=++player->sequence;
    for(int index=0;index<count;index++)key->notes[index]=notes[index];
    return 1;
}
static void hb_cp_off(hb_chord_player *player,int source,int channel){
    for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *key=&player->keys[index];
        if(!key->used||key->source!=source||key->channel!=channel)continue;
        key->held=0;if(!player->config.latch)key->used=0;
    }
}
/* Emit a desired-state difference, OFFs first. Capacity exhaustion leaves the
   remaining differences intact for the next tick, including panic/role changes.
   Multiple owners sharing a pitch generate one ON and one final OFF. */
static int hb_cp_diff(hb_chord_player *player,uint8_t desired[16][128],
                      uint8_t output[][3],int lengths[],int capacity){
    int emitted=0;unsigned channels=0;
    for(int channel=0;channel<16;channel++)if(player->sounding_channels[channel])channels|=1u<<channel;
    for(int key=0;key<HB_CP_KEYS;key++)if(player->keys[key].used)channels|=1u<<player->keys[key].channel;
    for(int pass=0;pass<2;pass++)for(int channel=0;channel<16;channel++){
    if(!(channels&(1u<<channel)))continue;
    for(int note=0;note<128;note++){
        int on=desired[channel][note]!=0,was=player->sounding[channel][note]!=0;
        if(pass==0&&was&&player->retrigger[channel][note])on=0;
        if(on==was||on!=pass)continue;
        if(emitted>=capacity)return emitted;
        output[emitted][0]=(uint8_t)((on?0x90:0x80)|channel);
        output[emitted][1]=(uint8_t)note;output[emitted][2]=on?desired[channel][note]:0;
        lengths[emitted++]=3;player->sounding[channel][note]=(uint8_t)on;
        player->sounding_count+=on?1:-1;player->sounding_channels[channel]+=on?1:-1;player->retrigger[channel][note]=0;
    }}
    return emitted;
}
typedef struct {int key,voice,pitch,channel;} hb_cp_entry;
static int hb_cp_entries(hb_chord_player *player,hb_cp_entry *entries,int fresh){
    int count=0;
    for(int key=0;key<HB_CP_KEYS;key++){
        hb_cp_key *owner=&player->keys[key];
        if(!owner->used||(fresh&&!owner->fresh))continue;
        for(int voice=0;voice<owner->count;voice++){
            entries[count++]=(hb_cp_entry){key,voice,owner->notes[voice],owner->channel};
        }
    }
    for(int index=1;index<count;index++){
        hb_cp_entry entry=entries[index];int slot=index;
        while(slot>0&&(player->config.order==3?player->keys[entries[slot-1].key].sequence>player->keys[entry.key].sequence:(player->config.order==1?entries[slot-1].pitch<entry.pitch:entries[slot-1].pitch>entry.pitch))){
            entries[slot]=entries[slot-1];slot--;
        }
        entries[slot]=entry;
    }
    if(player->config.order==4&&fresh)for(int index=count-1;index>0;index--){
        player->random=player->random*1664525u+1013904223u;
        int other=(int)(player->random%(unsigned)(index+1));
        hb_cp_entry temp=entries[index];entries[index]=entries[other];entries[other]=temp;
    }
    return count;
}
static int hb_cp_tick(hb_chord_player *player,uint8_t output[][3],int lengths[],int capacity){
    uint8_t desired[16][128];memset(desired,0,sizeof(desired));
    if(player->flushing){
        int emitted=hb_cp_diff(player,desired,output,lengths,capacity);
        if(!player->sounding_count)player->flushing=0;
        return emitted;
    }
    hb_cp_entry entries[HB_CP_KEYS*HB_CP_VOICES];
    if(player->config.playback==1){
        int count=hb_cp_entries(player,entries,0);
        int repeat_same=0;
        /* Collapse shared output pitches before arpeggiation. */
        int unique=0;
        for(int index=0;index<count;index++){
            int seen=0;for(int prior=0;prior<unique;prior++)
                if(entries[prior].pitch==entries[index].pitch&&entries[prior].channel==entries[index].channel)seen=1;
            if(!seen)entries[unique++]=entries[index];
        }
        count=unique;
        double rate=hb_cp_division(player->config.rate);
        if(!count)player->running=0;
        if(count&&!player->running&&player->config.phase){
            player->step=0;
            /* Rotate the ordered cycle to the newest gesture's actual root,
               which is not necessarily the bass of an inverted voicing. */
            unsigned newest=0;int root=-1;
            for(int index=0;index<count;index++){
                hb_cp_key *owner=&player->keys[entries[index].key];
                if(hb_cp_mod(entries[index].pitch)==owner->root_pc&&owner->sequence>newest){
                    newest=owner->sequence;root=index;
                }
            }
            if(root>=0)player->step=root;
            player->next_beat=(hb_cp_floor(player->beat/rate)+1.0)*rate;
            player->running=2; /* Armed, no sounding note yet. */
        }
        if(count&&(!player->running||player->beat+1e-9>=player->next_beat)){
            if(!player->running)player->step=0;
            int cycle=player->config.order==2&&count>1?2*count-2:count;
            int ordinal=player->step%cycle;if(ordinal>=count)ordinal=cycle-ordinal;
            if(player->config.order==4&&player->running!=2){player->random=player->random*1664525u+1013904223u;ordinal=(int)(player->random%(unsigned)count);}
            hb_cp_entry entry=entries[ordinal];
            repeat_same=player->sounding[entry.channel][entry.pitch]!=0;
            player->arp_note=entry.pitch;player->arp_channel=entry.channel;
            static const double gates[4]={0.25,0.5,0.75,0.9};
            double onset=player->config.phase?hb_cp_floor((player->beat+1e-9)/rate)*rate:player->beat;
            player->gate_beat=onset+rate*gates[player->config.gate];
            player->next_beat=onset+rate;player->step=(player->step+1)%384;
            player->running=1;
        }
        if(player->running==1&&!repeat_same&&player->beat<player->gate_beat){
            for(int index=0;index<count;index++)if(entries[index].pitch==player->arp_note&&entries[index].channel==player->arp_channel)
                desired[player->arp_channel][player->arp_note]=(uint8_t)player->keys[entries[index].key].velocity;
        }
    }else{
        int count=hb_cp_entries(player,entries,1);
        double duration=player->config.playback==2?(player->config.spread<0?
            hb_cp_division(-player->config.spread-1):player->config.spread/1000.0):0.0;
        double now=player->config.spread<0?player->beat:player->seconds;
        for(int index=0;index<count;index++){
            hb_cp_entry entry=entries[index];
            player->keys[entry.key].due[entry.voice]=now+(count>1?duration*index/(count-1):0.0);
        }
        for(int key=0;key<HB_CP_KEYS;key++){
            hb_cp_key *owner=&player->keys[key];if(!owner->used)continue;
            owner->fresh=0;
            for(int voice=0;voice<owner->count;voice++){
                if(now+1e-9>=owner->due[voice])owner->started|=1u<<voice;
                if(owner->started&(1u<<voice))desired[owner->channel][owner->notes[voice]]=(uint8_t)owner->velocity;
            }
        }
    }
    return hb_cp_diff(player,desired,output,lengths,capacity);
}
#endif
