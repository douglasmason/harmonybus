#define main existing_chord_tests
#include "chord_player_test.c"
#undef main

int main(void){
    Inst *instance=fixture();hb_mt_phrase phrase;
    API.set_param(instance,"motif_preset","I-VI7-ii-V-I");
    static const int counts[]={5,4,4,5,3},anchors[]={4,3,0,4,3};
    for(int choice=0;choice<5;choice++){
        API.set_param(instance,"motif_placement",HB_MT_PLACEMENTS[choice]);
        hb_mt_selected(instance,&phrase);
        assert(phrase.count==counts[choice]&&phrase.anchor==anchors[choice]);
        assert(phrase.reference_valid&&phrase.reference_pitch==60);
    }
    /* A target-free body is timed toward a virtual arrival; no implicit
       target attack is introduced at that boundary. */
    API.set_param(instance,"motif_preset","V-Target");
    API.set_param(instance,"motif_arrival","Next Bar");
    position=0;assert(hb_mt_launch(instance,60,100,0));
    int attacks=0;
    for(int index=0;index<HB_MT_SCHEDULE;index++){
        hb_mt_scheduled *event=&instance->motif.events[index];
        if(event->used){assert(event->on==3&&event->off<=4);attacks++;}
    }
    assert(attacks>0);
    char state[131072];assert(API.get_param(instance,"state",state,sizeof(state))>0);
    API.destroy_instance(instance);
    instance=API.create_instance("",NULL);API.set_param(instance,"state",state);
    assert(instance->motif.editor.placement==4);
    /* Old snapshots have Saved behavior, not whatever the last loaded
       snapshot selected on this instance. */
    char *marker=strstr(state,";mt1,");assert(marker);memmove(marker,marker+6,strlen(marker+6)+1);
    API.set_param(instance,"state",state);assert(instance->motif.editor.placement==0);
    API.set_param(instance,"motif_preset","ii-V-LT");
    API.set_param(instance,"motif_placement","Start");hb_mt_selected(instance,&phrase);
    assert(phrase.count==4&&phrase.anchor==0&&phrase.events[0].modifier==0);
    assert(phrase.events[3].modifier==-1);
    unsigned tokens[HB_MT_STEPS+2];
    assert(hb_ar_live_steps(28,0,1,tokens)==3); /* I-VI7-ii-V-I body only */
    assert(tokens[0]==(28|(1<<6))&&tokens[2]==(28|(3<<6)));
    assert(hb_ar_live_steps(28,2,1,tokens)==4);
    assert(tokens[0]==28&&tokens[1]==(28|(1<<6)));
    assert(hb_ar_live_steps(52,0,1,tokens)==3); /* ii-V-LT: retain LT */
    assert(tokens[2]==(52|(2<<6)));
    assert(hb_ar_live_steps(52,2,1,tokens)==4&&tokens[0]==15);
    /* The generated opening target can be recorded using the same stable
       token format, independently of subsequent placement changes. */
    hb_mt_phrase neutral;assert(hb_ar_phrase(15,&neutral)==&neutral);
    assert(neutral.count==1&&neutral.events[0].notes[0].pitch==60);
    API.destroy_instance(instance);
    puts("motif placement: separate targets, silent arrival, state migration pass");
}
