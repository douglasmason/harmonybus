#ifndef HB_CHORD_SELECTION_H
#define HB_CHORD_SELECTION_H
/* A silent conductor event in the existing immutable note-action envelope.
   The low word contains root-relative chord, scale, bass and tonic. Opcode 63
   is reserved for this exact tag; it must never be evaluated as an operation. */
#define HB_SELECTION_TAG 0xc858001f00000000ULL
static int hb_selection_word(unsigned long long word){
    unsigned chord=word&4095u,scale=(word>>12)&4095u;
    return (word&0xffffffff00000000ULL)==HB_SELECTION_TAG && (chord&1) &&
        __builtin_popcount(chord)>=2 && scale && ((word>>24)&15)<12 && ((word>>28)&15)<12;
}
static int hb_selection_actions(const unsigned long long *words){
    if(!hb_selection_word(words[0]))return 0;
    for(int index=1;index<=HB_MOTION_LANES;index++)if(words[index])return 0;
    return 1;
}
#endif
