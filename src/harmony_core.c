#include "harmony_core.h"
#ifdef HB_FREESTANDING
typedef __SIZE_TYPE__ size_t;
extern int snprintf(char *, size_t, const char *, ...);
extern void *memset(void *, int, size_t);
extern size_t strlen(const char *);
extern char *strcpy(char *, const char *);
#define INT_MIN (-2147483647 - 1)
#else
#include <stdio.h>
#include <string.h>
#include <limits.h>
#endif
static int mod12(int value) { value %= 12; return value < 0 ? value + 12 : value; }
static int clamp_midi(int note) { return note < 0 ? 0 : (note > 127 ? 127 : note); }
const char *hb_pc_name(int pitch_class) {
    static const char *names[12]={"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
    return names[mod12(pitch_class)];
}
typedef struct { const char *suffix; uint16_t mask; int complexity; int infer_enabled; } chord_template_t;
#define BIT(n) ((uint16_t)(1u << (n)))
static const chord_template_t templates[] = {
    {"",BIT(0)|BIT(4)|BIT(7),3,1},{"min",BIT(0)|BIT(3)|BIT(7),3,1},{"5",BIT(0)|BIT(7),2,1},
    {"sus2",BIT(0)|BIT(2)|BIT(7),3,1},{"sus4",BIT(0)|BIT(5)|BIT(7),3,1},{"dim",BIT(0)|BIT(3)|BIT(6),3,1},
    {"aug",BIT(0)|BIT(4)|BIT(8),3,1},
    {"6",BIT(0)|BIT(4)|BIT(7)|BIT(9),4,0},
    {"min6",BIT(0)|BIT(3)|BIT(7)|BIT(9),4,1},
    {"maj7",BIT(0)|BIT(4)|BIT(7)|BIT(11),4,1},{"7",BIT(0)|BIT(4)|BIT(7)|BIT(10),4,1},{"min7",BIT(0)|BIT(3)|BIT(7)|BIT(10),4,1},
    {"minMaj7",BIT(0)|BIT(3)|BIT(7)|BIT(11),4,1},{"min7b5",BIT(0)|BIT(3)|BIT(6)|BIT(10),4,1},{"dim7",BIT(0)|BIT(3)|BIT(6)|BIT(9),4,1},
    {"add9",BIT(0)|BIT(2)|BIT(4)|BIT(7),4,1},{"minAdd9",BIT(0)|BIT(2)|BIT(3)|BIT(7),4,1},{"maj9",BIT(0)|BIT(2)|BIT(4)|BIT(7)|BIT(11),5,1},
    {"9",BIT(0)|BIT(2)|BIT(4)|BIT(7)|BIT(10),5,1},{"min9",BIT(0)|BIT(2)|BIT(3)|BIT(7)|BIT(10),5,1},
    {"11",BIT(0)|BIT(2)|BIT(4)|BIT(5)|BIT(7)|BIT(10),6,0},{"min11",BIT(0)|BIT(2)|BIT(3)|BIT(5)|BIT(7)|BIT(10),6,0},
    {"13",BIT(0)|BIT(2)|BIT(4)|BIT(7)|BIT(9)|BIT(10),6,0}
}
static const int template_count=(int)(sizeof(templates)/sizeof(templates[0]));
static uint16_t rotate_to_root(uint16_t mask,int root) {
    uint16_t output=0; for(int pitch_class=0;pitch_class<12;++pitch_class) if(mask&BIT(pitch_class)) output|=BIT(mod12(pitch_class-root)); return output;
}
static int popcount12(uint16_t value) { int count=0; for(int index=0;index<12;index++) if(value&BIT(index)) count++; return count; }
hb_harmony_t hb_infer_harmony(const uint8_t *notes,int note_count) {
    hb_harmony_t result; memset(&result,0,sizeof(result)); result.chord_index=-1; strcpy(result.name,"--");
    if(!notes||note_count<=0) return result;
    uint16_t input=0; int bass=127;
    for(int index=0;index<note_count;index++){int note=notes[index];if(note<bass)bass=note;input|=BIT(mod12(note));}
    int input_count=popcount12(input); if(!input_count)return result;
    int best_score=INT_MIN,second_score=INT_MIN,best_root=bass%12,best_template=-1;
    for(int root=0;root<12;root++){uint16_t relative=rotate_to_root(input,root);for(int index=0;index<template_count;index++){if(!templates[index].infer_enabled)continue;
        uint16_t mask=templates[index].mask;
        int matched=popcount12(relative&mask),missing=popcount12(mask&~relative),extras=popcount12(relative&~mask);
        int score=matched*16-missing*12-extras*8;

        int has_root=(relative&BIT(0))!=0;
        int has_m3=(relative&BIT(3))!=0;
        int has_M3=(relative&BIT(4))!=0;
        int has_P5=(relative&BIT(7))!=0;
        int has_b5=(relative&BIT(6))!=0;
        int has_sharp5=(relative&BIT(8))!=0;
        int has_b7=(relative&BIT(10))!=0;
        int has_M7=(relative&BIT(11))!=0;

        int expects_m3=(mask&BIT(3))!=0;
        int expects_M3=(mask&BIT(4))!=0;
        int expects_P5=(mask&BIT(7))!=0;
        int expects_b5=(mask&BIT(6))!=0;
        int expects_sharp5=(mask&BIT(8))!=0;
        int expects_b7=(mask&BIT(10))!=0;
        int expects_M7=(mask&BIT(11))!=0;

        if(has_root)score+=8;

        /* Reward only structural tones that belong to THIS candidate template.
           This is the key inversion fix: unrelated input intervals cannot lend
           structural credibility to a simpler but wrong root. */
        if(expects_P5&&has_P5)score+=20;
        if(((expects_m3&&has_m3)||(expects_M3&&has_M3))&&
           ((expects_b7&&has_b7)||(expects_M7&&has_M7)))score+=20;
        if(expects_b5&&has_b5&&expects_m3&&has_m3)score+=16;
        if(expects_sharp5&&has_sharp5&&expects_M3&&has_M3)score+=16;

        /* Missing structural tones are more costly than missing extensions. */
        if(expects_P5&&!has_P5)score-=12;
        if((expects_m3&&!has_m3)&&(expects_M3&&!has_M3))score-=10;

        /* Sus templates deliberately replace the third, but retain the fifth. */
        if(index==3||index==4){
            if(expects_P5&&has_P5)score+=8;
        }

        /* Exact pitch-class/template identity is decisive. */
        if(missing==0&&extras==0)score+=36;

        /* Bass is inversion information, not primary root evidence. */
        if(root==(bass%12))score+=2;

        score-=(templates[index].complexity>input_count?templates[index].complexity-input_count:input_count-templates[index].complexity)*2;
        if(score>best_score){second_score=best_score;best_score=score;best_root=root;best_template=index;}else if(score>second_score)second_score=score;
    }}
    if(best_template<0)return result; result.valid=1;result.root_pc=best_root;result.bass_pc=bass%12;result.pitch_mask=input;result.chord_index=best_template;
    int theoretical=input_count*18+18;int confidence=theoretical>0?(best_score*100/theoretical):0;int margin=best_score-second_score;if(margin>0)confidence+=margin*2;
    if(confidence<1)confidence=1;if(confidence>100)confidence=100;result.confidence=confidence;
    snprintf(result.name,sizeof(result.name),"%s%s",hb_pc_name(best_root),templates[best_template].suffix);
    if(result.bass_pc!=result.root_pc){size_t used=strlen(result.name);snprintf(result.name+used,sizeof(result.name)-used,"/%s",hb_pc_name(result.bass_pc));}
    return result;
}
hb_harmony_t hb_transpose_harmony(hb_harmony_t harmony,int semitones) {
    if(!harmony.valid||semitones==0)return harmony;harmony.root_pc=mod12(harmony.root_pc+semitones);harmony.bass_pc=mod12(harmony.bass_pc+semitones);
    uint16_t mask=0;for(int pitch_class=0;pitch_class<12;pitch_class++)if(harmony.pitch_mask&BIT(pitch_class))mask|=BIT(mod12(pitch_class+semitones));harmony.pitch_mask=mask;
    if(harmony.chord_index>=0&&harmony.chord_index<template_count){snprintf(harmony.name,sizeof(harmony.name),"%s%s",hb_pc_name(harmony.root_pc),templates[harmony.chord_index].suffix);
        if(harmony.bass_pc!=harmony.root_pc){size_t used=strlen(harmony.name);snprintf(harmony.name+used,sizeof(harmony.name)-used,"/%s",hb_pc_name(harmony.bass_pc));}}
    return harmony;
}
static int nearest_pc_note(int center,uint16_t mask){int best=center,best_distance=999;for(int delta=-12;delta<=12;delta++){int note=center+delta;if(note<0||note>127)continue;if(mask&BIT(mod12(note))){int distance=delta<0?-delta:delta;if(distance<best_distance||(distance==best_distance&&delta>0)){best=note;best_distance=distance;}}}return best;}
int hb_map_note(int midi_note,int reference_root_pc,hb_harmony_t target,hb_map_mode_t mode){
    if(!target.valid)return clamp_midi(midi_note);int reference=mod12(reference_root_pc);int source_interval=mod12(midi_note-reference);int octave_delta=(midi_note-reference)/12;
    int anchor=midi_note+(target.root_pc-mod12(midi_note-source_interval));while(anchor-midi_note>6)anchor-=12;while(midi_note-anchor>6)anchor+=12;
    if(mode==HB_MAP_TRANSPOSE){int delta=mod12(target.root_pc-reference);if(delta>6)delta-=12;return clamp_midi(midi_note+delta);}
    int nominal=anchor+source_interval+octave_delta*12;while(nominal-midi_note>12)nominal-=12;while(midi_note-nominal>12)nominal+=12;
    if(mode==HB_MAP_NEAREST)return clamp_midi(nearest_pc_note(midi_note,target.pitch_mask));
    int exact_pc=mod12(target.root_pc+source_interval);if(target.pitch_mask&BIT(exact_pc)){int note=nominal;int difference=mod12(exact_pc-mod12(note));if(difference>6)difference-=12;return clamp_midi(note+difference);}
    return clamp_midi(nearest_pc_note(nominal,target.pitch_mask));
}
