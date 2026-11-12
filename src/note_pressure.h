#ifndef HB_NOTE_PRESSURE_H
#define HB_NOTE_PRESSURE_H
/* Poly pressure addresses the onset's sounding pitch, never a fresh harmony
   lookup. Local and Render To routes may have different channels/voices. */
static void hb_note_pressure_route(Inst *instance,hb_motion_route *route,int channel,int source,int value,int rendered){
    if(!route->owned)return;
    unsigned short targets[16][128]={{0}};
    unsigned channels=0;
    int identity=hb_fv_identity(source,channel,instance->movy_playback!=0);
    int chord=hb_cp_enabled(&instance->player)&&!instance->movy_passthrough;
    unsigned char chord_sources[128]={0};
    int chord_channel=rendered?instance->player.render_channel:channel;
    if(chord)for(int slot=0;slot<HB_CP_KEYS;slot++){
        const hb_cp_key *key=&instance->player.keys[slot];
        if(!key->used||!key->held||key->source!=source||key->channel!=channel||key->playback_origin!=(instance->movy_playback!=0))continue;
        for(int voice=0;voice<key->count;voice++)if(key->notes[voice]>=0&&key->notes[voice]<128)chord_sources[key->notes[voice]]=1;
    }
    for(int index=0;index<HB_MOTION_OWNERS;index++){
        const hb_motion_owner *owner=&route->owners[index];
        if(!owner->used||!owner->sounding||owner->generated)continue;
        int matches=chord?(!owner->input_owner&&owner->channel==chord_channel&&owner->source>=0&&owner->source<128&&chord_sources[owner->source]):owner->input_owner==identity;
        if(matches&&owner->channel>=0&&owner->channel<16&&owner->pitch>=0&&owner->pitch<128){
            targets[owner->channel][owner->pitch]=(unsigned short)(owner->input_owner+1);
            channels|=1u<<owner->channel;
        }
    }
    for(int destination=0;destination<16;destination++)if(channels&(1u<<destination))for(int pitch=0;pitch<128;pitch++)
        if(targets[destination][pitch])hb_mo_push_owned(route,0xa0|destination,pitch,value,targets[destination][pitch]-1);
}
static void hb_note_pressure(Inst *instance,const uint8_t input[3]){
    int channel=input[0]&15,source=input[1]&127,value=input[2]&127;
    int origin=instance->movy_playback!=0;
    if(instance->chord_pair_input&&instance->chord_pair_held[origin][channel][source]&&instance->chord_pair_owner[origin][channel])
        source=instance->chord_pair_owner[origin][channel]-1;
    hb_note_pressure_route(instance,&instance->motion_local,channel,source,value,0);
    hb_note_pressure_route(instance,&instance->motion_render,channel,source,value,1);
    hb_motion_flush_render(instance);
}
#endif
