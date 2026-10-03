/* Offer already resolved intent; scheduled motifs use this at their onset. */
static void hb_override_offer(hb_override_owner *owner,unsigned mask,int root,int semantic){
    uint8_t notes[12];int note_count=0;
    for(int pitch=0;pitch<12;pitch++)if(mask&(1u<<pitch))notes[note_count++]=(uint8_t)(60+pitch);
    hb_harmony_t harmony=hb_infer_harmony(notes,note_count);
    if(!harmony.valid)return;
    if(semantic&&harmony.root_pc!=root){harmony.root_pc=root;harmony.chord_index=HB_HARMONY_EXPLICIT_TONES;harmony.pitch_mask=(uint16_t)mask;}
    owner->next=harmony;owner->next_order=++g_override_order;owner->pending=1;
}
/* Capture resolved intent once per input, before arp/strum schedules its voices. */
static void hb_override_capture(Inst *instance,int source,int on,int root,const int *pitches,int count,unsigned semantic){
    int index=hb_override_index(instance);if(index<0)return;
    hb_override_owner *owner=&g_override[index];
    int origin=instance->movy_playback!=0;
    if(!owner->active||(owner->active==1&&origin)||source<0||source>127||!owner->eligible[origin][source])return;
    if(!on){owner->held[origin][source]=owner->eligible[origin][source]=0;return;}
    /* Revoicing a held chord is not another press and must not reclaim
       latest-owner priority from a different track. */
    if(owner->eligible[origin][source]!=2)return;
    if(count<=0)return;
    unsigned mask=0;int resolved_root=root;
    /* Use the same pure pitch-operation resolver as normal output. No MIDI,
       repeats, arp attacks or generated note-offs are fed back into detection. */
    for(int voice=0;voice<count;voice++){
        uint8_t message[3]={0x90,(uint8_t)pitches[voice],100};int pitch,velocity,pan,skip;double off;
        hb_motion_resolve_output(instance,message,&pitch,&velocity,&pan,&off,&skip);
        if(!skip)mask|=1u<<mod12(pitch);
    }
    if(!mask)return;
    for(int pitch_class=0;pitch_class<12;pitch_class++)if((semantic&(1u<<pitch_class))||pitch_class==root){
        uint8_t message[3]={0x90,(uint8_t)(60+pitch_class),100};int pitch,velocity,pan,skip;double off;
        hb_motion_resolve_output(instance,message,&pitch,&velocity,&pan,&off,&skip);
        if(pitch_class==root)resolved_root=mod12(pitch);
        if(!skip&&(semantic&(1u<<pitch_class)))mask|=1u<<mod12(pitch);
    }
    owner->masks[origin][source]=(uint16_t)mask;
    owner->eligible[origin][source]=1;
    owner->held[origin][source]=1;
    unsigned combined=0;
    for(int input_origin=0;input_origin<2;input_origin++)for(int note=0;note<128;note++)
        if(owner->held[input_origin][note])combined|=owner->masks[input_origin][note];
    hb_override_offer(owner,combined,resolved_root,semantic!=0);
}
