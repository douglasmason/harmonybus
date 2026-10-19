#ifndef HB_DOMINANT_PALETTE_H
#define HB_DOMINANT_PALETTE_H
/* Chord-root collections shared by local and destination-based treatment.
   IDs in the existing destination-family list remain unchanged. */
#define HB_DOMINANT_FAMILY_COUNT 11
static unsigned hb_dominant_palette(int root,int palette){
    static const unsigned masks[]={
        0x6b5u, /* Mixolydian: 1 2 3 4 5 6 b7 */
        0x6d5u, /* Lydian dominant: 1 2 3 #4 5 6 b7 */
        0x55bu, /* Altered: 1 b9 #9 3 b5 b13 b7 */
        0x6dbu, /* Half-whole: 1 b9 #9 3 #11 5 13 b7 */
        0x555u  /* Whole tone: 1 2 3 #4 #5 b7 */
    };
    if(palette<0||palette>=5)palette=0;
    return hb_transpose_mask(masks[palette],root);
}
#endif
