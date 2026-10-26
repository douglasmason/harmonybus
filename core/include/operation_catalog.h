/* Generated from data/operations.json; do not edit. */
#ifndef HB_OPERATION_CATALOG_H
#define HB_OPERATION_CATALOG_H
enum {
    HB_MO_OFF = 0,
    HB_MO_VELOCITY = 1,
    HB_MO_PAN = 2,
    HB_MO_OCTAVE = 3,
    HB_MO_ROTATE = 4,
    HB_MO_GATE = 5,
    HB_MO_SKIP = 6,
    HB_MO_HARMONY = 7,
    HB_MO_BELOW = 8,
    HB_MO_ABOVE = 9,
    HB_MO_ENCLOSE_AB = 10,
    HB_MO_ENCLOSE_BA = 11,
    HB_MO_REPEAT = 12,
    HB_MO_REVERSE = 13,
    HB_MO_TIME_SHIFT = 14,
    HB_MO_SPEED = 15,
    HB_MO_TRANSPOSE = 16,
    HB_MO_RATCHET = 17,
    HB_MO_ECHO = 18,
    HB_MO_CHORD_FORM = 19,
    HB_MO_AUTO_CHORD_REPEAT = 20,
    HB_MO_SECONDARY_II = 21,
    HB_MO_SECONDARY_V = 22,
    HB_MO_SECONDARY_VI = 23,
    HB_MO_BACKDOOR_II = 24,
    HB_MO_BACKDOOR_V = 25,
    HB_MO_CHROM_ABOVE = 26,
    HB_MO_TRITONE_II = 27,
    HB_MO_CADENCE_II_V = 28,
    HB_MO_CADENCE_BACKDOOR = 29,
    HB_MO_CADENCE_TRITONE = 30,
    HB_MO_TRITONE_V = 31,
    HB_MO_SECONDARY_III = 32,
    HB_MO_SECONDARY_IV = 33,
    HB_MO_SECONDARY_VII = 34,
    HB_MO_MIXED_FIRST = 35,
    HB_MO_MIXED_1 = 36,
    HB_MO_MIXED_2 = 37,
    HB_MO_MIXED_3 = 38,
    HB_MO_MIXED_4 = 39,
    HB_MO_MIXED_5 = 40,
    HB_MO_MIXED_6 = 41,
    HB_MO_MIXED_7 = 42,
    HB_MO_MIXED_8 = 43,
    HB_MO_MIXED_9 = 44,
    HB_MO_MIXED_10 = 45,
    HB_MO_MIXED_11 = 46,
    HB_MO_MIXED_12 = 47,
    HB_MO_MIXED_13 = 48,
    HB_MO_MOTIF = 49,
    HB_MO_CHORD_STATE = 50,
    HB_MO_LEADING_TONE = 51,
    HB_MO_UPPER_DIM = 52,
    HB_MO_KEY_CENTER = 53,
    HB_MO_PARALLEL_SCALE = 54,
    HB_MO_LIVE_HARMONY_OVERRIDE = 55,
    HB_MO_HARMONY_OVERRIDE = 56,
    HB_MO_SECONDARY_FIFTH = 57,
    HB_MO_SECONDARY_II_DOM = 58,
    HB_MO_SECONDARY_IV_DOM = 59,
    HB_MO_SECONDARY_VI_DOM = 60,
    HB_MO_RECORD_KEY = 61,
    HB_MO_RELATIVE_KEY = 62,
    HB_MO_KEY_BACK = 63,
    HB_MO_KEY_RETURN = 64,
    HB_MO_WEAVE = 65,
    HB_MO_DRUM_FILL = 66,
    HB_MO_MIXED_LAST = HB_MO_MIXED_13
};
#define HB_MO_OPERATION_COUNT 67
static const char *MO_OPERATIONS[] = {"Off","Velocity","Pan","Octave","Rotate","Gate","Skip","Harmony","Connector Below","Scale Above","Enclose Above Below","Enclose Below Above","Clip Repeat","Clip Reverse","Clip Time Shift","Clip Speed","Transpose","Ratchet","MIDI Echo","Chord Form","Auto Chord Repeat","Secondary II","Secondary V (Dom)","Secondary VI","Backdoor II","Backdoor V","Connector Above","Tritone II","II-V-Target","Backdoor II-V-Target","Tritone II-V-Target","Tritone Sub","Secondary III","Secondary IV","Secondary VII","bVI-bVII-I","bVI-V-I","bIII-IV-I","vi-V-I","iii-vi-ii-V-I","IV-iv-I","ii halfdim-V-i","I-VI7-ii-V-I","V/V-V-I","ii/V-V/V-V-I","V/ii-ii-V-I","V/vi-vi-ii-V-I","vii dim/V-V-I","III7-VI7-II7-V7-I","Play Motif","Chord/Arp State","Leading Tone","Upper Dim","Live Key Center","Parallel Scale","Live Harmony Override","Override Harmony (Live + Recorded)","Secondary Fifth","Secondary II (Dom)","Secondary IV (Dom)","Secondary VI (Dom)","Recordable Key Center","Relative Key Center","Key Step Back","Key Return to Start","Clip Weave","Drum Fill"};
#define HB_AR_CHOICE_COUNT 37
static const char *HB_AR_NAMES[] = {"Connector Below","Connector Above","Scale Above","Secondary II","Secondary V (Dom)","Secondary VI","Backdoor II","Backdoor V","Tritone II","Tritone Sub","Secondary III","Secondary IV","Secondary VII","Motif 1","Motif 2","Motif 3","Motif 4","Motif 5","Motif 6","Motif 7","Motif 8","Motif 9","Motif 10","Motif 11","Motif 12","Motif 13","Motif 14","Motif 15","Motif 16","Leading Tone","Upper Dim","Secondary Fifth","Secondary II (Dom)","Secondary IV (Dom)","Secondary VI (Dom)","Smart Below","Smart Above"};
typedef struct {
    int motion,token,role,modifier,semitones,degree,quality,connector_above,trigger_index;
    int preferred_token,alternate_token;
    const char *name,*short_name;
} hb_ro_definition;
static const hb_ro_definition HB_RELATIVE_OPERATIONS[HB_AR_CHOICE_COUNT] = {
    [0] = {8,1,0,-1,-1,-1,-1,0,0,0,0,"Connector Below","CCB"},
    [2] = {9,3,0,1,0,1,0,0,1,0,0,"Scale Above","Up"},
    [3] = {21,4,1,0,0,1,0,0,2,0,0,"Secondary II","ii"},
    [4] = {22,5,2,0,-5,-1,6,0,3,0,0,"Secondary V (Dom)","V"},
    [5] = {23,6,4,0,-12,5,0,0,4,0,0,"Secondary VI","vi"},
    [6] = {24,7,5,0,5,-1,7,0,5,0,0,"Backdoor II","iv"},
    [7] = {25,8,6,0,-2,-1,6,0,6,0,0,"Backdoor V","bVII"},
    [1] = {26,2,0,2,1,-1,-1,1,7,0,0,"Connector Above","CCA"},
    [8] = {27,9,7,0,-4,-1,7,0,8,0,0,"Tritone II","iiT"},
    [9] = {31,10,0,2,1,-1,6,0,9,0,0,"Tritone Sub","TTS"},
    [10] = {32,11,8,0,0,2,0,0,10,0,0,"Secondary III","iii"},
    [11] = {33,12,9,0,0,3,0,0,11,0,0,"Secondary IV","IV"},
    [12] = {34,13,10,0,-12,6,0,0,12,0,0,"Secondary VII","vii"},
    [29] = {51,14,12,0,-1,-1,9,0,13,0,0,"Leading Tone","LT"},
    [30] = {52,60,13,0,1,-1,9,0,14,0,0,"Upper Dim","UD"},
    [31] = {57,61,14,0,-12,4,0,0,15,0,0,"Secondary Fifth","5th"},
    [32] = {58,62,15,0,0,1,0,0,16,0,0,"Secondary II (Dom)","iiD"},
    [33] = {59,63,16,0,0,3,0,0,17,0,0,"Secondary IV (Dom)","ivD"},
    [34] = {60,59,17,0,-12,5,0,0,18,0,0,"Secondary VI (Dom)","viD"},
    [35] = {-1,4096,0,0,0,-1,9,0,-1,14,60,"Smart Below","S Below"},
    [36] = {-1,4097,0,0,0,-1,9,0,-1,60,14,"Smart Above","S Above"},
};
static const unsigned char HB_RO_BY_MOTION[67] = {0,0,0,0,0,0,0,0,1,3,0,0,0,0,0,0,0,0,0,0,0,4,5,6,7,8,2,9,0,0,0,10,11,12,13,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,30,31,0,0,0,0,32,33,34,35,0,0,0,0,0,0};
static const unsigned char HB_RO_BY_ROLE[18] = {0,4,5,0,6,7,8,9,11,12,13,0,30,31,32,33,34,35};
static const unsigned char HB_RO_BY_TOKEN[64] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,30,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,35,31,32,33,34};
static const unsigned char HB_RO_BY_TRIGGER[19] = {1,3,4,5,6,7,8,2,9,10,11,12,13,30,31,32,33,34,35};
#define HB_RO_SELECTOR_BASE 4096
static const unsigned char HB_RO_BY_SELECTOR[] = {36,37};
typedef struct { const char *alias,*name; } hb_ro_alias;
static const hb_ro_alias HB_RO_ALIASES[] = {
    {"Chrom Below","Connector Below"},
    {"Chromatic Below","Connector Below"},
    {"Secondary LT","Connector Below"},
    {"CCB","Connector Below"},
    {"Secondary V","Secondary V (Dom)"},
    {"Chrom Above","Connector Above"},
    {"Chromatic Above","Connector Above"},
    {"CCA","Connector Above"},
    {"Upper Chromatic Approach","Connector Above"},
    {"UCA","Connector Above"},
    {"Tritone V","Tritone Sub"},
    {"TTS","Tritone Sub"},
    {"LT","Leading Tone"},
};
#endif
