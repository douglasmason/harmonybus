#ifndef HB_APPROACH_ROWS_H
#define HB_APPROACH_ROWS_H
/* Slots reference the existing motif library/presets; no phrase copy is saved. */
#define HB_AR_SHIFT 43
#define HB_AR_MASK (2047ULL<<HB_AR_SHIFT)
static const char *HB_AR_NAMES[]={"Connector Below","Connector Above","Scale Above","Secondary II","Secondary V (Dom)","Secondary VI","Backdoor II","Backdoor V","Tritone II","Tritone Sub","Secondary III","Secondary IV","Secondary VII","Motif 1","Motif 2","Motif 3","Motif 4","Motif 5","Motif 6","Motif 7","Motif 8","Motif 9","Motif 10","Motif 11","Motif 12","Motif 13","Motif 14","Motif 15","Motif 16","Leading Tone","Upper Dim","Secondary Fifth","Secondary II (Dom)","Secondary IV (Dom)","Secondary VI (Dom)"};
/* Index 2 remains a legacy Scale Above assignment; new choices use Secondary II. */
static const char *hb_ar_name(int choice){return HB_AR_NAMES[choice==2?3:choice];}
typedef struct {
    int row_slots[3],row_steps[3];
    int sequence_slots[8],sequence_count,sequence_cursor,sequence_event;
    unsigned sequence_pad;
    int row_preset,row_event,pending_row,preview_row,performance,latch,motif_latch,used;
    double touched_at[16];
    int target_placement;
    int enabled,knobs[8],bank[16],order[8],order_slot[8],count,cursor,event,bank_armed;
    unsigned latch_slots,turned;
    unsigned down,knob_down,step_down,selected;unsigned long long saved_word;int restore_word;unsigned short tokens[128];unsigned char swallow[16][128];
} hb_ar_state;
/* Seven common operations, then seven familiar cadences; slots 8/16 remain
   available to steps while knob 8 on each panel controls Chord + Arp. */
static const int HB_AR_DEFAULT_BANK[16]={-5,-4,-1,-2,-30,-10,-8,-31,2,3,5,36,37,14,10,15};
static void hb_ar_init(hb_ar_state *state){memset(state,0,sizeof(*state));for(int index=0;index<8;index++)state->knobs[index]=13+index;for(int index=0;index<16;index++)state->bank[index]=HB_AR_DEFAULT_BANK[index];state->order[0]=1;for(int index=0;index<8;index++)state->order_slot[index]=-1;state->count=1;state->bank_armed=-1;state->pending_row=state->preview_row=-1;for(int row=0;row<3;row++)state->row_slots[row]=2-row;}
static int hb_ar_choice_code(const hb_ar_state *state,int choice){int reference=choice>=29?-(choice+1):choice<13?-(choice+1):state->bank[choice-13];return reference==-30?14:reference==-31?60:reference==-32?61:reference==-33?62:reference==-34?63:reference==-35?59:reference<0?-reference:15+reference;}
static int hb_ar_code(const hb_ar_state *state,int slot){int reference=state->bank[slot];return reference==-30?14:reference==-31?60:reference==-32?61:reference==-33?62:reference==-34?63:reference==-35?59:reference<0?-reference:15+reference;}
static int hb_ar_alias_shift(unsigned long long word){int marker=(word>>2)&3;return marker==1?-36:marker==2?36:marker==3?(int)(signed char)(word>>55):0;}
static unsigned long long hb_ar_alias_word(int shift){return shift==-36?4:shift==36?8:shift?12|((unsigned long long)(unsigned char)shift<<55):0;}
static int hb_ar_alias_valid(unsigned long long word){int marker=(word>>2)&3,extra=(word>>55)&255;if(marker!=3)return extra==0;int shift=(signed char)extra;return shift==32||shift==-32||shift==64||shift==-64||shift==96||shift==-96;}
static const hb_mt_phrase *hb_ar_phrase(int code,hb_mt_phrase *builtin){int reference=code-15;if(code==15){memset(builtin,0,sizeof(*builtin));builtin->count=1;builtin->events[0]=(hb_mt_event){.duration=24,.count=1,.chord_mode=3,.scale=0xAB5,.notes={{60,100}}};return builtin;}if(hb_mt_reference_preset(reference)){hb_mt_preset(hb_mt_reference_preset(reference),builtin);return builtin;}if(reference>=20&&reference<36){hb_mt_library_view(reference-20,builtin);return builtin;}return 0;}
/* Live cursors select stable legacy tokens. Placement never changes the
   meaning of a token already written into a clip. Code 15 is the neutral
   target for motifs which never contained a boundary target. */
static int hb_ar_live_steps(int code,int choice,int spatial,unsigned *tokens){
    hb_mt_phrase builtin;const hb_mt_phrase *phrase=hb_ar_phrase(code,&builtin);
    if(!phrase){tokens[0]=(unsigned)code;return 1;}
    int first=0,last=phrase->count,start=-1,end=-1,count=0;
    if(choice||spatial){
        const hb_mt_event *reference=&phrase->events[phrase->anchor];
        while(first<last){
            int tail=last-1;while(tail>first&&phrase->events[tail].kind==2)tail--;
            if(!hb_mt_is_target(&phrase->events[tail],reference))break;
            if(end<0)end=tail;last=tail;
        }
        while(first<last&&hb_mt_is_target(&phrase->events[first],reference)){
            if(start<0)start=first;first++;
            while(first<last&&phrase->events[first].kind==2)first++;
        }
        int placement=hb_mt_placement(choice,0);
        if(placement&HB_MT_TARGET_START){int index=start>=0?start:end;tokens[count++]=index<0?15:(unsigned)code|((unsigned)index<<6);}
        for(int index=first;index<last;index++)if(phrase->events[index].kind!=2)
            tokens[count++]=(unsigned)code|((unsigned)index<<6);
        if(placement&HB_MT_TARGET_END){int index=end>=0?end:start;tokens[count++]=index<0?15:(unsigned)code|((unsigned)index<<6);}
    }else for(int index=0;index<phrase->count;index++)if(phrase->events[index].kind!=2)
        tokens[count++]=(unsigned)code|((unsigned)index<<6);
    return count;
}
static unsigned hb_ar_live_step(int code,int choice,int spatial,int step){
    unsigned tokens[HB_MT_STEPS+2];int count=hb_ar_live_steps(code,choice,spatial,tokens);
    return count?tokens[step>=0&&step<count?step:0]:0;
}
static unsigned hb_ar_peek(const hb_ar_state *state){return state->count?
    hb_ar_live_step(state->order[state->cursor],state->target_placement,0,state->event):1;}
static void hb_ar_advance(hb_ar_state *state){
    unsigned tokens[HB_MT_STEPS+2];int count=hb_ar_live_steps(state->order[state->cursor],state->target_placement,0,tokens);
    if(++state->event<count)return;
    state->event=0;state->cursor=(state->cursor+1)%state->count;if(!state->cursor&&!state->motif_latch&&(state->count>1||(!state->down&&!state->latch)))state->performance=0;
}
/* Last three control touches form a persistent FIFO, independent of holds. */
static void hb_ar_row_touch(hb_ar_state *state,int slot){
    for(int row=2;row>0;row--){state->row_slots[row]=state->row_slots[row-1];state->row_steps[row]=state->row_steps[row-1];}
    state->row_slots[0]=state->row_preset=slot;state->row_steps[0]=state->row_event=0;
}
/* Continue only consecutive presses of the same spatial approach identity. */
static void hb_ar_pad_press(hb_ar_state *state,int source,int row,int shift){
    unsigned pad=row==3?((unsigned)(shift+128)<<8)|(unsigned)(source+1):0;
    if(!pad||pad!=state->sequence_pad)state->sequence_cursor=state->sequence_event=state->row_event=0;
    state->sequence_pad=pad;
}
static unsigned hb_ar_sequence_peek(const hb_ar_state *state){return hb_ar_live_step(hb_ar_code(state,state->sequence_slots[state->sequence_cursor]),state->target_placement,1,state->sequence_event);}
static void hb_ar_sequence_touch(hb_ar_state *state,int slot){
    if(!state->knob_down)state->sequence_count=0;
    if(state->sequence_count<8)state->sequence_slots[state->sequence_count++]=slot;
    state->sequence_cursor=state->sequence_event=0;
}
static unsigned hb_ar_row_peek(const hb_ar_state *state,int row){if(row==3&&state->sequence_count)return hb_ar_sequence_peek(state);unsigned code=hb_ar_code(state,row==3?state->row_preset:state->row_slots[row]);unsigned step=row==3?state->row_event:state->row_steps[row];return hb_ar_live_step((int)code,state->target_placement,1,(int)step);}
static unsigned hb_ar_live_peek(const hb_ar_state *state,int row){return row>=0?hb_ar_row_peek(state,row):state->performance?hb_ar_peek(state):0;}
/* Rows are spatial keys. Their next press repeats the assigned step; only
   the separate performance bank consumes a sequence. */
static void hb_ar_live_advance(hb_ar_state *state,int row){
    if(row<0){state->used=1;hb_ar_advance(state);return;}
    if(row!=3||!state->sequence_count)return;
    unsigned tokens[HB_MT_STEPS+2];
    int count=hb_ar_live_steps(hb_ar_code(state,state->sequence_slots[state->sequence_cursor]),state->target_placement,1,tokens);
    if(++state->sequence_event<count)return;
    state->sequence_event=0;state->sequence_cursor=(state->sequence_cursor+1)%state->sequence_count;
}
static unsigned long long hb_ar_intent(unsigned token){
    int code=token&63;
    if(code==62)return hb_mo_role_word(15);if(code==63)return hb_mo_role_word(16);if(code==59)return hb_mo_role_word(17);if(code==61)return hb_mo_role_word(14);if(code==14)return hb_mo_role_word(12);if(code==60)return hb_mo_role_word(13);
    if(code==1)return 1;if(code==2)return 3|HB_MO_CONNECTOR_ABOVE;if(code==3)return 2;
    static const int roles[]={1,2,4,5,6,7,0,8,9,10};
    if(code==10)return 3; /* Tritone V uses the established chromatic-above intent. */
    return code>=4&&code<=13?hb_mo_role_word(roles[code-4]):0;
}
#endif
