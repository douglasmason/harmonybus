#ifndef HB_APPROACH_ROWS_H
#define HB_APPROACH_ROWS_H
/* Slots reference the existing motif library/presets; no phrase copy is saved. */
#define HB_AR_SHIFT 43
#define HB_AR_MASK (2047ULL<<HB_AR_SHIFT)
static const char *HB_AR_NAMES[]={"Secondary LT","Chromatic Above","Scale Above","Secondary II","Secondary V","Secondary VI","Backdoor II","Backdoor V","Tritone II","Tritone V","Secondary III","Secondary IV","Secondary VII","Motif 1","Motif 2","Motif 3","Motif 4","Motif 5","Motif 6","Motif 7","Motif 8","Motif 9","Motif 10","Motif 11","Motif 12","Motif 13","Motif 14","Motif 15","Motif 16"};
/* Index 2 remains a legacy Scale Above assignment; new choices use Secondary II. */
static const char *hb_ar_name(int choice){return HB_AR_NAMES[choice==2?3:choice];}
typedef struct {
    int enabled,knobs[8],bank[16],order[8],order_slot[8],count,cursor,event,bank_armed;
    unsigned down,selected;unsigned long long saved_word;int restore_word;unsigned short tokens[128];unsigned char swallow[16][128];
} hb_ar_state;
static void hb_ar_init(hb_ar_state *state){memset(state,0,sizeof(*state));for(int index=0;index<8;index++)state->knobs[index]=13+index;for(int index=0;index<16;index++)state->bank[index]=index+1;state->order[0]=1;for(int index=0;index<8;index++)state->order_slot[index]=-1;state->count=1;state->bank_armed=-1;}
static int hb_ar_code(const hb_ar_state *state,int slot){int choice=state->knobs[slot];return choice<13?choice+1:15+state->bank[choice-13];}
static const hb_mt_phrase *hb_ar_phrase(int code,hb_mt_phrase *builtin){int reference=code-15;if(reference>0&&reference<20){hb_mt_preset(reference,builtin);return builtin;}return reference>=20&&reference<36?&g_motifs[reference-20]:0;}
static unsigned hb_ar_peek(const hb_ar_state *state){return state->count?((unsigned)state->order[state->cursor]|((unsigned)state->event<<6)):1;}
static void hb_ar_advance(hb_ar_state *state){
    hb_mt_phrase builtin;const hb_mt_phrase *phrase=hb_ar_phrase(state->order[state->cursor],&builtin);
    if(phrase){
        do{state->event++;}while(state->event<phrase->count&&phrase->events[state->event].kind==2);
        if(state->event<phrase->count)return;
    }
    state->event=0;state->cursor=(state->cursor+1)%state->count;
}
static unsigned long long hb_ar_intent(unsigned token){
    int code=token&63;
    if(code==1)return 1;if(code==2)return 3|HB_MO_CONNECTOR_ABOVE;if(code==3)return 2;
    static const int roles[]={1,2,4,5,6,7,0,8,9,10};
    if(code==10)return 3; /* Tritone V uses the established chromatic-above intent. */
    return code>=4&&code<=13?hb_mo_role_word(roles[code-4]):0;
}
#endif
