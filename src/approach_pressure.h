/* Optional row pressure controls existing operation amounts, without changing
   the row's pitch assignment or swallowing its note edges. No allocation. */
static void hb_ap_refresh(Inst *instance){
    hb_ar_state *state=&instance->approach_rows;
    instance->motion.pressure_active=0;
    if(!state->pressure[0].active&&!state->pressure[1].active&&
       !state->pressure[2].active&&!state->pressure[3].active)return;
    for(int lane=0;lane<16;lane++){
        int winner=-1;
        for(int row=0;row<4;row++)if(state->pressure_lane[row]==lane+1&&
            state->pressure_mode[row]&&state->pressure[row].active&&
            (winner<0||state->pressure_order[row]>state->pressure_order[winner]))winner=row;
        if(winner<0||!hb_mo_pressure_supported(instance->motion.lanes[lane].operation))continue;
        instance->motion.pressure_active|=1ULL<<lane;
        instance->motion.pressure_value[lane]=hb_pressure_value(&state->pressure[winner],state->pressure_reverse[winner]);
    }
}
static void hb_ap_clear(Inst *instance){
    hb_ar_state *state=&instance->approach_rows;
    memset(state->pressure,0,sizeof(state->pressure));
    memset(state->pressure_contacts,0,sizeof(state->pressure_contacts));
    instance->motion.pressure_active=0;
}
/* Consume only mapped pressure, so one squeeze cannot also drive arp pressure. */
static int hb_ap_input(Inst *instance,const uint8_t *input,int length,int row){
    if(length<3||instance->movy_playback||instance->synthetic_advance)return 0;
    int type=input[0]&240,source=input[1]&127,channel=input[0]&15;
    int down=type==0x90&&input[2],up=type==0x80||(type==0x90&&!input[2]);
    if(!down&&!up&&type!=0xA0)return 0;
    hb_ar_state *state=&instance->approach_rows;
    if(!(state->pressure_mode[0]|state->pressure_mode[1]|state->pressure_mode[2]|state->pressure_mode[3]))return 0;
    int owner=-1,free_slot=-1;
    for(int index=0;index<64;index++){
        hb_ap_contact *contact=&state->pressure_contacts[index];
        if(!contact->count){if(free_slot<0)free_slot=index;}
        else if(contact->source==source&&contact->channel==channel){owner=index;break;}
    }
    if(down){
        if(owner>=0){if(state->pressure_contacts[owner].count<255)state->pressure_contacts[owner].count++;return 0;}
        if(row<0||row>=4||!state->pressure_mode[row]||!state->pressure_lane[row]||free_slot<0)return 0;
        owner=free_slot;
        state->pressure_contacts[owner]=(hb_ap_contact){source,channel,row,1,0};
        if(!state->pressure[row].down){
            hb_pressure_begin(&state->pressure[row],state->pressure_clock);
            state->pressure_order[row]=++state->pressure_serial;
        }
    }else{
        if(owner<0)return 0;
        hb_ap_contact *contact=&state->pressure_contacts[owner];row=contact->row;
        if(up){if(--contact->count)return 0;}
        else contact->value=input[2]&127;
    }
    int contacts=0,maximum=0;
    for(int index=0;index<64;index++){
        hb_ap_contact *contact=&state->pressure_contacts[index];
        if(contact->count&&contact->row==row){contacts++;if(contact->value>maximum)maximum=contact->value;}
    }
    if(contacts)hb_pressure_update(&state->pressure[row],state->pressure_mode[row],maximum/127.0);
    else hb_pressure_end(&state->pressure[row],state->pressure_mode[row],state->pressure_clock,g_hb_hold_ms,state->pressure_prepared[row]*.01);
    hb_ap_refresh(instance);
    return type==0xA0&&state->pressure_lane[row]>0&&
        hb_mo_pressure_supported(instance->motion.lanes[state->pressure_lane[row]-1].operation);
}
