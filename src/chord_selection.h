#ifndef HB_CHORD_SELECTION_H
#define HB_CHORD_SELECTION_H
/* A silent conductor event in the existing immutable note-action envelope.
   The low word contains root-relative chord, scale, bass and tonic. Opcode 63
   is reserved for this exact tag; it must never be evaluated as an operation. */
#define HB_SELECTION_REGISTER (1ULL<<46)
#define HB_SELECTION_TAG 0xc858001f00000000ULL
#define HB_SELECTION_INTENT_TAG 0xc859001f00000000ULL
/* Versioned top-note range: high byte c9, bit 55 intent, bits 48..54 MIDI top.
   The chord, parent, intent and original note owner retain their old fields. */
static int hb_selection_top(unsigned long long word){
    return (word>>56)==0xc9?(int)((word>>48)&127):-1;
}
static unsigned long long hb_selection_base_word(unsigned long long word){
    return hb_selection_top(word)<0?word:(word&0x0000ffffffffffffULL)|
        ((word&(1ULL<<55))?HB_SELECTION_INTENT_TAG:HB_SELECTION_TAG);
}
static int hb_selection_has_intent(unsigned long long word){
    word=hb_selection_base_word(word);
    return (word&0xffff801f00000000ULL)==HB_SELECTION_INTENT_TAG &&
        ((word>>37)&15)>=1 && ((word>>37)&15)<=10 && ((word>>41)&15)<12;
}
static int hb_selection_word(unsigned long long word){
    if(hb_selection_top(word)>=0&&!(word&HB_SELECTION_REGISTER))return 0;
    word=hb_selection_base_word(word);
    unsigned chord=word&4095u,scale=(word>>12)&4095u;
    return ((word&0xffffbfff00000000ULL)==HB_SELECTION_TAG||hb_selection_has_intent(word)) && (chord&1) &&
        __builtin_popcount(chord)>=2 && scale && ((word>>24)&15)<12 && ((word>>28)&15)<12;
}
static int hb_selection_actions(const unsigned long long *words){
    if(!hb_selection_word(words[0]))return 0;
    for(int index=1;index<=HB_MOTION_LANES;index++)if(words[index])return 0;
    return 1;
}
#endif
