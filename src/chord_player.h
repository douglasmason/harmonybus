#ifndef HB_CHORD_PLAYER_H
#define HB_CHORD_PLAYER_H
/* Allocation-free chord voicing and owned-note playback. Times are supplied
   by the host adapter; the adapter decides which role owns the output. */
#define HB_CP_KEYS 16
#define HB_CP_VOICES 12
#define HB_CP_FORMS 18
#define HB_CP_FOLLOW_DETECTED 17
static const char *CP_CHORD_FORM[]={"Auto","Power","Triad","Seventh","Ninth","Add9","Sixth","6/9","Eleventh","Thirteenth","Sus2","Sus4","Shell 7","Shell 9","Shell 6/9","Rootless 7","Rootless 9","Follow Detected"};
typedef struct {
    int mode, size, inversion, voicing, playback, latch, order, rate, gate, spread, phase;
    int quality, chromatic_quality, note_phase, clear_harmony, start;
} hb_cp_config;
typedef struct {
    int used, held, source, channel, velocity, count, fresh, root_pc, recordable, played_pitch;
    int intent_kind,intent_target,intent_minor;unsigned intent_scale,gap_mask;
    unsigned sequence, harmony_mask, harmony_sequence, semantic_mask;
    hb_cp_config onset_config;
    int harmony_root, playback_origin, range;
    unsigned transform_revision;
    int notes[HB_CP_VOICES];
    double due[HB_CP_VOICES];
    unsigned started;
} hb_cp_key;
typedef struct {
    hb_cp_config config;
    int state_override;hb_cp_config state_config;
    int repeat_override; /* Runtime operation; never written into saved panel settings. */
    hb_cp_key keys[HB_CP_KEYS];
    uint8_t sounding[16][128], retrigger[16][128];
    int sounding_channels[16];
    int sounding_count, flushing, render_channel, step, running, arp_note, arp_channel, arp_velocity;
    unsigned random, sequence, shuffle_signature;
    int shuffle_count, shuffle_position, anchor_pending;
    unsigned anchor_harmony_mask;int anchor_harmony_root,anchor_harmony_valid;
    int shuffle_order[HB_CP_KEYS*HB_CP_VOICES*4];
    double seconds, beat, next_beat, gate_beat;
} hb_chord_player;
static const hb_cp_config *hb_cp_settings(const hb_chord_player *player){return player->state_override?&player->state_config:&player->config;}
static int hb_cp_mode(const hb_chord_player *player){int mode=hb_cp_settings(player)->mode;return player->repeat_override&&!mode?2:mode;}
static int hb_cp_playback(const hb_chord_player *player){return player->repeat_override?1:hb_cp_settings(player)->playback;}
static hb_cp_config hb_cp_effective_config(const hb_chord_player *player){
    hb_cp_config config=*hb_cp_settings(player);config.mode=hb_cp_mode(player);config.playback=hb_cp_playback(player);return config;
}
static int hb_cp_mod(int value){value%=12;return value<0?value+12:value;}
static int hb_cp_clamp(int value,int low,int high){return value<low?low:value>high?high:value;}
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
/* Played Top Note fixes the melody register. Repack selected pitch classes below it,
   retaining an outside melody note without substituting it for a chord tone.
   At the MIDI floor omit unavailable lower voices instead of moving melody. */
static int hb_cp_top_note(int *notes,int count,int top,int root,int fifth,int voicing){
    if(!count)return 0;
    top=hb_cp_clamp(top,0,127);
    unsigned selected=0;
    for(int index=0;index<count;index++)selected|=1u<<hb_cp_mod(notes[index]);
    int kept=0;
    for(int distance=11;distance>0;distance--){
        int note=top-distance,pitch=hb_cp_mod(note);
        if(!(selected&(1u<<pitch)))continue;
        notes[kept++]=note;
    }
    for(int index=0;index<kept;index++){
        int pitch=hb_cp_mod(notes[index]);
        if((voicing==1&&(pitch==root||pitch==fifth))||
           (voicing==2&&!(index%2)))notes[index]-=12;
    }
    int available=0;
    for(int index=0;index<kept;index++)if(notes[index]>=0)notes[available++]=notes[index];
    notes[available++]=top;
    hb_cp_sort(notes,available);
    return available;
}
/* Inversion: Auto, Root, First through Sixth. Auto is From Key for
   Conductor Chord and root position for Scale Root. Preserve the chosen bass
   through spread voicings. Shift the WHOLE voicing at MIDI range edges. */
/* Rootless V9 versus V7b9 follows the target collection's sixth, not
   its third: melodic minor and Dorian keep the natural sixth. */
static int hb_cp_auto_leading_quality(int target,unsigned scale){
    unsigned sixth=1u<<hb_cp_mod(target+9),flat_sixth=1u<<hb_cp_mod(target+8);
    if(scale&sixth)return 8;
    if(scale&flat_sixth)return 9;
    return 8;
}
static int hb_cp_chromatic_quality(int selection){
    static const int qualities[]={0,5,6,9,7,8};
    return qualities[hb_cp_clamp(selection,0,5)];
}
/* Translate detected chord members to degree roles before changing target root.
   The destination scale/quality supplies pitches; detection supplies the form. */
static int hb_cp_interval_role(int interval,unsigned relative){
    int role=0;
    if(interval==1||interval==2)role=1;
    else if(interval==3)role=(relative&(1u<<4))?1:2;
    else if(interval==4)role=2;
    else if(interval==5)role=3;
    else if(interval==6)role=(relative&(1u<<7))?3:4;
    else if(interval==7)role=4;
    else if(interval==8)role=(relative&(1u<<7))?5:4;
    else if(interval==9)role=(relative&(1u<<3))&&(relative&(1u<<6))&&!(relative&((1u<<7)|(1u<<10)|(1u<<11)))?6:5;
    else if(interval>=10)role=6;
    return role;
}
static unsigned hb_cp_detected_roles(int root,unsigned chord){
    if(!chord)return (1u<<0)|(1u<<2)|(1u<<4);
    unsigned relative=0,roles=0;
    for(int interval=0;interval<12;interval++)if(chord&(1u<<hb_cp_mod(root+interval)))relative|=1u<<interval;
    for(int interval=0;interval<12;interval++)if(relative&(1u<<interval)){
        int role=hb_cp_interval_role(interval,relative);
        roles|=1u<<role;
    }
    return roles;
}
static int hb_cp_voice_semantic(hb_cp_config config,int input,int root,unsigned chord,
                       unsigned scale,int *output,unsigned *semantic){
    unsigned detected_roles=hb_cp_detected_roles(root,chord);
    int follows_detected=config.size==HB_CP_FOLLOW_DETECTED;
    if(semantic)*semantic=0;
    config.size=hb_cp_clamp(config.size,0,HB_CP_FORMS-1);
    if(input<0)input=0;if(input>127)input=127;
    if(config.mode==0){output[0]=input;return 1;}
    int ordered[12],count=0,bass=input,tones[7];
    if(config.mode==1){if(!scale)return 0;root=hb_cp_mod(input);}
    else if(!chord)return 0;
    if(follows_detected&&config.mode==2)config.size=0;
    tones[0]=root;int degree=1;
    for(int offset=1;offset<=24&&degree<7;offset++)
        if(scale&(1u<<hb_cp_mod(root+offset)))tones[degree++]=hb_cp_mod(root+offset);
    if(degree<7){static const int major[7]={0,2,4,5,7,9,11};for(int index=1;index<7;index++)tones[index]=hb_cp_mod(root+major[index]);}
    unsigned relative_scale=0;for(int interval=0;interval<12;interval++)if(scale&(1u<<hb_cp_mod(root+interval)))relative_scale|=1u<<interval;
    if(relative_scale==0x55Bu){static const int altered[7]={0,1,4,6,6,8,10};for(int index=0;index<7;index++)tones[index]=hb_cp_mod(root+altered[index]);}
    /* Six-note symmetric collections have harmonic roles, not seven ordinal
       degrees. Whole tone supplies #5/b7; augmented supplies #5/M7 where
       present. On its other three roots there is no seventh: omit that voice. */
    int symmetric_no_seventh=0;
    if(relative_scale==0x555u||relative_scale==0x999u||relative_scale==0x333u){
        tones[2]=hb_cp_mod(root+4);tones[4]=hb_cp_mod(root+8);
        if(relative_scale&(1u<<11))tones[6]=hb_cp_mod(root+11);
        else if(relative_scale&(1u<<10))tones[6]=hb_cp_mod(root+10);
        else {tones[6]=root;symmetric_no_seventh=1;}
    }
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
        /* A diminished seventh is the ninth semitone, not a scale-derived b7.
           Require the diminished triad so ordinary sixth chords stay sixths. */
        unsigned diminished=(1u<<root)|(1u<<hb_cp_mod(root+3))|
            (1u<<hb_cp_mod(root+6))|(1u<<hb_cp_mod(root+9));
        if((chord&diminished)==diminished&&
           !(chord&((1u<<hb_cp_mod(root+10))|(1u<<hb_cp_mod(root+11)))))
            tones[6]=hb_cp_mod(root+9);
    }
    /* Quality overrides apply to Scale Root gestures. Chromatic keys use a
       selectable triad/seventh family instead of an arbitrary rotated scale.
       Parent-scale upper extensions remain available to Ninth/Add9 etc. */
    if(config.mode==1||config.quality){
        int quality=config.quality;
        if(config.mode==1&&!quality&&!(scale&(1u<<hb_cp_mod(input)))){
            if(config.chromatic_quality==6){
                int target=input+1;
                while(target<input+12&&!(scale&(1u<<hb_cp_mod(target))))target++;
                quality=hb_cp_auto_leading_quality(target,scale);
            }else quality=hb_cp_chromatic_quality(config.chromatic_quality);
        }
        if(quality){
            static const int third[]={0,4,3,3,4,4,4,3,3,3,3,4,4};
            static const int fifth[]={0,7,7,6,8,7,7,7,6,6,7,8,6};
            static const int seventh[]={0,11,10,9,10,11,10,10,10,9,11,11,10};
            tones[2]=hb_cp_mod(root+third[quality]);
            tones[4]=hb_cp_mod(root+fifth[quality]);
            tones[6]=hb_cp_mod(root+seventh[quality]);
        }
    }
    /* Form: Auto, Power, Triad, Seventh, Ninth, Add9, Sixth, 6/9,
       Eleventh, Thirteenth, Sus2, Sus4. Values are zero-based scale degrees. */
    static const int forms[HB_CP_FORMS][7]={
        {0,2,4,-1,-1,-1,-1},{0,4,-1,-1,-1,-1,-1},{0,2,4,-1,-1,-1,-1},
        {0,2,4,6,-1,-1,-1},{0,2,4,6,1,-1,-1},{0,2,4,1,-1,-1,-1},
        {0,2,4,5,-1,-1,-1},{0,2,4,5,1,-1,-1},{0,2,4,6,1,3,-1},
        {0,2,4,6,1,3,5},{0,1,4,-1,-1,-1,-1},{0,3,4,-1,-1,-1,-1},
        {0,2,6,-1,-1,-1,-1},{0,2,6,1,-1,-1,-1},{0,2,5,1,-1,-1,-1},
        {2,6,-1,-1,-1,-1,-1},{2,6,1,-1,-1,-1,-1}
    };
    unsigned selected=0;
    if(follows_detected&&(config.mode==1||config.quality)){
        static const int order[7]={0,2,4,6,1,3,5};
        for(int index=0;index<7;index++){
            int role=order[index];if(!(detected_roles&(1u<<role)))continue;
            if(role==6&&symmetric_no_seventh&&!config.quality)continue;
            int pitch=tones[role];
            if(!(selected&(1u<<pitch))){ordered[count++]=pitch;selected|=1u<<pitch;}
        }
    }else if(config.mode==2&&config.size==0&&!config.quality){
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
        if(role==6&&symmetric_no_seventh&&config.mode==1&&!config.quality)continue;
        int pitch=tones[role];
        if(!(selected&(1u<<pitch))){ordered[count++]=pitch;selected|=1u<<pitch;}
    }
    if(!count)return 0;
    /* Omitted roots/fifths are voicing choices, not changes of chord identity.
       Power keeps its scale-derived quality; suspensions remain suspensions. */
    if(semantic){
        *semantic=selected;
        if(config.size==1||(config.size>=12&&config.size<HB_CP_FOLLOW_DETECTED))
            *semantic|=(1u<<root)|(1u<<tones[2])|(1u<<tones[4]);

    }
    int inversion=0;
    if(config.inversion>0&&config.inversion!=8){
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
    if(config.inversion==8)return hb_cp_top_note(output,count,input,root,fifth,config.voicing);
    hb_cp_sort(output,count);
    while(output[0]<0)for(int index=0;index<count;index++)output[index]+=12;
    while(output[count-1]>127)for(int index=0;index<count;index++)output[index]-=12;
    return output[0]>=0?count:0;
}
static int hb_cp_voice(hb_cp_config config,int input,int root,unsigned chord,
                       unsigned scale,int *output){
    return hb_cp_voice_semantic(config,input,root,chord,scale,output,0);
}
static void hb_cp_defaults(hb_cp_config *config){
    memset(config,0,sizeof(*config));config->rate=2;config->gate=1;config->phase=2;config->start=5;
}
static int hb_cp_enabled(const hb_chord_player *player){
    return hb_cp_mode(player)||hb_cp_playback(player);
}
static void hb_cp_clear(hb_chord_player *player){
    memset(player->keys,0,sizeof(player->keys));
    memset(player->retrigger,0,sizeof(player->retrigger));
    player->running=0;player->flushing=1;player->step=0;
    player->shuffle_count=player->shuffle_position=0;player->anchor_pending=0;player->anchor_harmony_valid=0;
}
static int hb_cp_held(const hb_chord_player *player){
    int count=0;for(int key=0;key<HB_CP_KEYS;key++)count+=player->keys[key].used&&player->keys[key].held;
    return count;
}
/* Toggle ownership by the original input key, independent of rendered pitch.
   Shared rendered tones remain owned by any other retained input keys. */
static int hb_cp_toggle_off(hb_chord_player *player,int source,int channel){
    if(hb_cp_settings(player)->latch!=2&&hb_cp_settings(player)->latch!=3&&hb_cp_settings(player)->latch!=5)return 0;
    for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *key=&player->keys[index];
        if(key->used&&key->source==source&&key->channel==channel){
            memset(key,0,sizeof(*key));
            return 1;
        }
    }
    return 0;
}
static int hb_cp_on(hb_chord_player *player,int source,int channel,int velocity,
                    const int *notes,int count){
    if(hb_cp_toggle_off(player,source,channel))return 1;
    if(count<=0)return 1;
    if(hb_cp_settings(player)->latch==4||hb_cp_settings(player)->latch==5||((hb_cp_settings(player)->latch==1||hb_cp_settings(player)->latch==2)&&!hb_cp_held(player))){
        /* A latched replacement changes the pitch pool, not the running clock.
           Re-arming Auto here can postpone every division under rapid input. */
        if(hb_cp_playback(player)!=1){
            memcpy(player->retrigger,player->sounding,sizeof(player->retrigger));
            player->running=0;player->step=0;
        }
        memset(player->keys,0,sizeof(player->keys));
    }
    int slot=-1;
    for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *key=&player->keys[index];
        if(key->used&&key->source==source&&key->channel==channel){slot=index;break;}
        if(!key->used&&slot<0)slot=index;
    }
    if(slot<0)return 0;
    hb_cp_key *key=&player->keys[slot];
    if(key->used&&hb_cp_playback(player)!=1)for(int voice=0;voice<key->count;voice++){
        int pitch=key->notes[voice],shared=0;
        for(int other=0;other<HB_CP_KEYS;other++)if(other!=slot&&player->keys[other].used&&player->keys[other].channel==channel)
            for(int tone=0;tone<player->keys[other].count;tone++)if(player->keys[other].notes[tone]==pitch)shared=1;
        if(!shared)player->retrigger[channel][pitch]=1;
    }
    memset(key,0,sizeof(*key));
    key->used=key->held=key->fresh=1;key->source=source;key->channel=channel;
    key->played_pitch=source;key->root_pc=hb_cp_mod(source);key->velocity=velocity;key->count=count;key->sequence=++player->sequence;
    for(int index=0;index<count;index++)key->notes[index]=notes[index];
    return 1;
}
static void hb_cp_off(hb_chord_player *player,int source,int channel){
    for(int index=0;index<HB_CP_KEYS;index++){
        hb_cp_key *key=&player->keys[index];
        if(!key->used||key->source!=source||key->channel!=channel)continue;
        key->held=0;if(!hb_cp_settings(player)->latch)key->used=0;
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
    uint8_t seen[16][128]={{0}};
    for(int key=0;key<HB_CP_KEYS;key++){
        hb_cp_key *owner=&player->keys[key];
        if(!owner->used||(fresh&&!owner->fresh))continue;
        for(int voice=0;voice<owner->count;voice++){
            int octaves=hb_cp_playback(player)==1?hb_cp_clamp(owner->range,1,4):1;
            for(int octave=0;octave<octaves;octave++){
                int pitch=owner->notes[voice]+12*octave;
                if(pitch>127)continue;
                if(hb_cp_playback(player)==1){
                    if(seen[owner->channel][pitch])continue;
                    seen[owner->channel][pitch]=1;
                }
                entries[count++]=(hb_cp_entry){key,voice,pitch,owner->channel};
            }
        }
    }
    for(int index=1;index<count;index++){
        hb_cp_entry entry=entries[index];int slot=index;
        while(slot>0&&(hb_cp_settings(player)->order==3?player->keys[entries[slot-1].key].sequence>player->keys[entry.key].sequence:(hb_cp_settings(player)->order==1?entries[slot-1].pitch<entry.pitch:entries[slot-1].pitch>entry.pitch))){
            entries[slot]=entries[slot-1];slot--;
        }
        entries[slot]=entry;
    }
    if(hb_cp_settings(player)->order==4&&fresh)for(int index=count-1;index>0;index--){
        player->random=player->random*1664525u+1013904223u;
        int other=(int)(player->random%(unsigned)(index+1));
        hb_cp_entry temp=entries[index];entries[index]=entries[other];entries[other]=temp;
    }
    return count;
}
/* Duration of one rendered arp step, including cycle-length rates. */
static double hb_cp_step_beats(hb_chord_player *player){
    double rate=hb_cp_division(hb_cp_settings(player)->rate%9);
    if(hb_cp_settings(player)->rate>=9){
        hb_cp_entry entries[HB_CP_KEYS*HB_CP_VOICES*4];
        int count=hb_cp_entries(player,entries,0);
        int cycle=hb_cp_settings(player)->order==2&&count>1?2*count-2:count;
        if(cycle>0)rate/=cycle;
    }
    return rate;
}
static int hb_cp_tick(hb_chord_player *player,uint8_t output[][3],int lengths[],int capacity){
    uint8_t desired[16][128];memset(desired,0,sizeof(desired));
    if(player->flushing){
        int emitted=hb_cp_diff(player,desired,output,lengths,capacity);
        if(!player->sounding_count)player->flushing=0;
        return emitted;
    }
    hb_cp_entry entries[HB_CP_KEYS*HB_CP_VOICES*4];
    if(hb_cp_playback(player)==1){
        int count=hb_cp_entries(player,entries,0);
        int cycle=hb_cp_settings(player)->order==2&&count>1?2*count-2:count;
        double rate=hb_cp_division(hb_cp_settings(player)->rate%9);
        if(hb_cp_settings(player)->rate>=9&&cycle>0)rate/=cycle;
        if(!count)player->running=0;
        if(count&&!player->running&&hb_cp_settings(player)->phase==1){
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
            if(root>=0&&!hb_cp_settings(player)->start)player->step=root;
            if(!hb_cp_settings(player)->start)player->step-=hb_cp_settings(player)->note_phase;
            player->next_beat=(hb_cp_floor(player->beat/rate)+1.0)*rate;
            player->running=2; /* Armed, no sounding note yet. */
        }
        if(count&&(!player->running||player->beat+1e-9>=player->next_beat)){
            int first=!player->running;
            if(first)player->step=hb_cp_settings(player)->start?0:-hb_cp_settings(player)->note_phase;
            /* Preserve the first-hit anchor in Free, including late callbacks.
               Other modes use transport grid after their opening note. */
            double onset=first?player->beat:player->next_beat;
            if(!first){
                int missed=(int)hb_cp_floor((player->beat-onset+1e-9)/rate);
                if(missed>0){onset+=missed*rate;player->step+=missed;}
            }
            int start_anchor=first||player->running==2||player->anchor_pending;
            if(hb_cp_settings(player)->start&&player->anchor_pending){player->step=0;player->anchor_pending=0;player->shuffle_count=0;}
            int extreme=0;
            if(hb_cp_settings(player)->start){
                if(hb_cp_settings(player)->start>=5){
                    unsigned newest=0;int target=entries[0].pitch;
                    for(int index=0;index<count;index++){
                        hb_cp_key *owner=&player->keys[entries[index].key];
                        if(owner->sequence>newest){newest=owner->sequence;target=owner->played_pitch;}
                    }
                    int best=100000;
                    for(int index=0;index<count;index++){
                        int distance=entries[index].pitch-target;if(distance<0)distance=-distance;
                        /* Prefer the played pitch class in the voiced pool;
                           rootless voicings fall back to the nearest voice. */
                        int score=distance+(hb_cp_mod(entries[index].pitch)!=hb_cp_mod(target)?1000:0);
                        if(score<best){best=score;extreme=index;}
                    }
                }else for(int index=1;index<count;index++)
                    if((hb_cp_settings(player)->start==1||hb_cp_settings(player)->start==3)?entries[index].pitch<entries[extreme].pitch:entries[index].pitch>entries[extreme].pitch)extreme=index;

            }
            int cycle_position=player->step%cycle;if(cycle_position<0)cycle_position+=cycle;
            int ordinal=(cycle_position+(hb_cp_settings(player)->start?extreme:0))%cycle;
            if(ordinal>=count)ordinal=cycle-ordinal;
            if(hb_cp_settings(player)->order==4&&player->running!=2){player->random=player->random*1664525u+1013904223u;ordinal=(int)(player->random%(unsigned)count);}
            if(hb_cp_settings(player)->order>=5){
                unsigned signature=2166136261u;
                for(int index=0;index<count;index++)signature=(signature^(unsigned)(entries[index].pitch+128*entries[index].channel))*16777619u;
                if(player->shuffle_count!=count||player->shuffle_signature!=signature||(hb_cp_settings(player)->start?!cycle_position:player->shuffle_position>=count)){
                    player->shuffle_count=count;player->shuffle_position=0;player->shuffle_signature=signature;
                    for(int index=0;index<count;index++)player->shuffle_order[index]=index;
                    for(int index=count-1;index>0;index--){
                        player->random=player->random*1664525u+1013904223u;
                        int other=(int)(player->random%(unsigned)(index+1));
                        int saved=player->shuffle_order[index];player->shuffle_order[index]=player->shuffle_order[other];player->shuffle_order[other]=saved;
                    }
                }
                ordinal=player->shuffle_order[player->shuffle_position++];
            }
            if(hb_cp_settings(player)->start&&hb_cp_settings(player)->order==4){
                if(!cycle_position)ordinal=extreme;
                else if(cycle_position==1&&count>1&&ordinal==extreme)ordinal=(ordinal+1)%count;
            }
            if(hb_cp_settings(player)->start&&hb_cp_settings(player)->order>=5){
                /* First-pinned Shuffle anchors only a new start/reanchor.
                   Cycle-pinned Shuffle anchors every cycle. Swap rather than
                   overwrite so each voice still occurs exactly once. */
                if(start_anchor||hb_cp_settings(player)->order==6)
                for(int index=0;index<count;index++)if(player->shuffle_order[index]==extreme){
                    int saved=player->shuffle_order[0];player->shuffle_order[0]=extreme;player->shuffle_order[index]=saved;break;
                }
                ordinal=player->shuffle_order[cycle_position];
            }
            hb_cp_entry entry=entries[ordinal];
            if(player->sounding[entry.channel][entry.pitch])player->retrigger[entry.channel][entry.pitch]=1;
            player->arp_note=entry.pitch;player->arp_channel=entry.channel;
            player->arp_velocity=player->keys[entry.key].velocity;
            static const double gates[4]={0.25,0.5,0.75,0.9};
            player->gate_beat=onset+rate*gates[hb_cp_settings(player)->gate];
            player->next_beat=first&&hb_cp_settings(player)->phase==2?
                (hb_cp_floor(player->beat/rate)+1.0)*rate:onset+rate;
            player->step=(player->step+1)%cycle;
            player->running=1;
        }
        if(player->running==1&&player->beat<player->gate_beat){
            /* A replacement pool may no longer contain this already-emitted
               latched hit. Let it finish its gate; use the new pool next step. */
            if((hb_cp_settings(player)->latch==1||hb_cp_settings(player)->latch==4)&&count)
                desired[player->arp_channel][player->arp_note]=(uint8_t)player->arp_velocity;
            for(int index=0;index<count;index++)if(entries[index].pitch==player->arp_note&&entries[index].channel==player->arp_channel)
                desired[player->arp_channel][player->arp_note]=(uint8_t)player->keys[entries[index].key].velocity;
        }
    }else{
        int count=hb_cp_entries(player,entries,1);
        double duration=hb_cp_playback(player)==2?(hb_cp_settings(player)->spread<0?
            hb_cp_division(-hb_cp_settings(player)->spread-1):hb_cp_settings(player)->spread/1000.0):0.0;
        double now=hb_cp_settings(player)->spread<0?player->beat:player->seconds;
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
