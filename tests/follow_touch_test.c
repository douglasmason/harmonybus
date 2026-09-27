#define main reference_suite_main
#include "follower_reference_test.c"
#undef main
int main(void){
    Inst *i=fixture();char text[8192],saved[8192];
    Inst *fresh=API.create_instance("",0);assert(fresh->chromatic_map==1);
    for(int k=0;k<8;k++)assert(fresh->touch_lanes[k]==(k<4?k+1:k+9));
    assert(fresh->motion.lanes[14].operation==HB_MO_ABOVE&&fresh->motion.lanes[15].operation==HB_MO_BELOW);
    for(int mode=0;mode<8;mode++)for(int scale=1;scale<=15;scale++)for(int root=0;root<12;root++){
        i->travel_map=mode;i->chromatic_map=1;hb_set_shared_follower_scale(scale);
        hb_harmony_t h=chord(root,root&1,0);hb_effective_write(h);
        uint16_t parent=hb_follower_input_scale(i,0);hb_harmony_t target=hb_follower_scale_target(i,h);
        for(int note=36;note<84;note++){
            int next=note;
            if(!(parent&(1u<<mod12(note))))while(next<127&&!(parent&(1u<<mod12(next))))next++;
            int expected=hb_map_follower_base_note(i,next,h,target);
            if(next!=note)expected=expected>0?expected-1:0;
            assert(hb_map_follower_reference_note(i,note,h,target)==expected);
        }
    }
    API.set_param(i,"travel_map","Closest Split Chromatic");API.get_param(i,"travel_map",text,sizeof(text));assert(!strcmp(text,"Closest Split"));
    API.set_param(i,"travel_map","Relative");assert(hb_chromatic_travel(i));
    API.set_param(i,"chromatic_map","Off");assert(!hb_chromatic_travel(i));
    API.set_param(i,"chromatic_map","On");API.set_param(i,"follow_touch_1","16");
    API.get_param(i,"state",saved,sizeof(saved));API.set_param(i,"follow_touch_1","2");API.set_param(i,"state",saved);
    assert(i->chromatic_map==1&&i->touch_lanes[0]==2);
    char *marker=strstr(saved,";ct1,");assert(marker);*marker=0;API.set_param(i,"state",saved);assert(!i->chromatic_map&&i->touch_lanes[0]==2);
    for(int reverse=0;reverse<2;reverse++){
        hb_mo_defaults(&i->motion);
        const char *first=reverse?"motion_gesture_16":"motion_gesture_15",*second=reverse?"motion_gesture_15":"motion_gesture_16";
        i->motion.lanes[14].touch_mode=0;i->motion.lanes[15].touch_mode=1;
        API.set_param(i,first,"Touch");API.set_param(i,second,"Touch");
        API.set_param(i,second,"Up,60");API.set_param(i,first,"Up,100");
        assert(i->motion.enclosure==(reverse?2:1)); /* down order, not release order */
        for(int n=0;n<3;n++){hb_mo_input(&i->motion,60,n,0.01);int expected=n==2?0:((n==0)!=reverse?2:1);assert(i->motion.events[16]==(unsigned)expected);}
        API.set_param(i,first,"Touch");API.set_param(i,first,"Up,500");assert(!i->motion.gesture_down&&!i->motion.enclosure);
        API.set_param(i,second,"Touch");API.set_param(i,second,"Cancel");assert(!i->motion.enclosure);
        assert(i->motion.lanes[14].touch_mode==0&&i->motion.lanes[15].touch_mode==1);
    }
    puts("Follow Touch: defaults, independent chromatic travel, save/legacy restore, touch order and forced tap/hold pass");
}
