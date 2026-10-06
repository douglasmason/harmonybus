#define HB_SECONDARY_FIXTURE
#include "secondary_chord_test.c"
static Inst *conductor(int track){
    Inst *instance=API.create_instance("",0);API.set_param(instance,"role","Conductor");instance->movy_track=track;return instance;
}
static void replay(Inst *instance,int track,int kind,int on,int a,int b,int c,unsigned order){
    char message[128];snprintf(message,sizeof(message),"%d,%d,%d,%d,%d,%d,%u",track,kind,on,a,b,c,order);
    API.set_param(instance,"hb_shared_context",message);
}
int main(void){
    Inst *first=fixture();API.set_param(first,"role","Conductor");first->movy_track=0;
    Inst *second=conductor(1),*follower=conductor(2);API.set_param(follower,"role","Follower");
    API.set_param(first,"parallel_scale","Natural Minor");API.set_param(first,"parallel_mode","Down");
    hb_key_context destination=hb_key_baseline(second);destination.target_root=2;destination.target_mask=hb_explicit_scale_mask(2,1);
    hb_key_commit(second,destination);
    assert(g_key_context.target_root==2&&g_key_context.target_mask==hb_explicit_scale_mask(2,2));
    API.set_param(first,"parallel_mode","Up");
    assert(g_key_context.target_root==2&&g_key_context.target_mask==hb_explicit_scale_mask(2,1));
    API.set_param(second,"key_center","Off");assert(!g_key_context.active);
    replay(first,0,HB_SC_PARALLEL,1,2,0,0,10);
    replay(first,1,HB_SC_KEY,1,2,hb_explicit_scale_mask(2,1),0,20);
    assert(!g_key_context.active); /* Updates stay staged until the barrier. */
    API.set_param(first,"hb_shared_flush","");assert(g_key_context.target_root==2&&g_key_context.target_mask==hb_explicit_scale_mask(2,2));
    API.set_param(follower,"parallel_scale","Major");API.set_param(follower,"parallel_mode","Down");
    assert(g_key_context.target_mask==hb_explicit_scale_mask(2,1));
    replay(first,1,HB_SC_KEY,1,4,hb_explicit_scale_mask(4,1),0,30);API.set_param(first,"hb_shared_flush","");
    API.set_param(follower,"parallel_mode","Up");assert(g_key_context.target_root==4&&g_key_context.target_mask==hb_explicit_scale_mask(4,2));
    replay(first,0,HB_SC_PARALLEL,0,0,0,0,40);API.set_param(first,"hb_shared_flush","");
    assert(g_key_context.target_root==4&&g_key_context.target_mask==hb_explicit_scale_mask(4,1));
    API.set_param(first,"transpose","2");assert(g_key_context.target_root==6);
    replay(first,1,HB_SC_KEY,1,2,hb_explicit_scale_mask(2,1),0,50);API.set_param(first,"hb_shared_flush","");assert(g_key_context.target_root==4);
    char view[2048];API.get_param(first,"shared_context_snapshot",view,sizeof(view));assert(strstr(view,"T2 REC")&&strstr(view,"|+2|"));
    replay(first,1,HB_SC_KEY,0,0,0,0,60);API.set_param(first,"hb_shared_flush","");assert(!g_key_context.active);
    g_sc_count=g_sc_head=0;
    API.set_param(follower,"parallel_mode","Down");API.set_param(follower,"parallel_mode","Up");assert(!g_sc_count);
    API.set_param(first,"parallel_mode","Down");API.set_param(first,"parallel_mode","Up");assert(g_sc_count==2);
    API.get_param(first,"follower_input_context_v2",view,sizeof(view));assert(strstr(view,"|sc1,0,1,1,")&&strstr(view,"|sc1,0,1,0,"));assert(!g_sc_count);
    API.set_param(first,"hb_shared_record","0");API.set_param(first,"follower_scale","Dorian");assert(g_sc_live[0][HB_SC_PARENT].on);
    API.set_param(first,"hb_shared_handoff","0");API.set_param(first,"hb_shared_flush","");assert(!g_key_context.active);
    API.set_param(first,"hb_shared_reset","All");API.set_param(first,"transpose","0");
    first->player.config.mode=0;first->movy_playback=1;
    API.set_param(first,"hb_shared_context","0,0,1,2,2774,0,100");API.set_param(first,"hb_shared_flush","");
    char anchor[128];snprintf(anchor,sizeof(anchor),"0,62,%d,%d",2774|(2741<<16),126|(2741<<13));API.set_param(first,"hb_shared_anchor",anchor);
    assert(played(first,62)==(1u<<2));release(first,62); /* Landing D is not remapped to E. */
    assert(played(first,60)==(1u<<2));release(first,60); /* Following C input uses D context. */
    /* Undoing a new take removes its replay contribution, but a key selected
       before Record remains a live performance choice. */
    first->movy_playback=0;API.set_param(first,"hb_shared_reset","All");
    destination=hb_key_baseline(first);destination.target_root=2;destination.target_mask=hb_explicit_scale_mask(2,3);
    hb_key_commit(first,destination);
    API.set_param(first,"hb_shared_record","0");
    replay(first,0,HB_SC_KEY,1,2,destination.target_mask,0,200);
    API.set_param(first,"hb_shared_handoff","0");API.set_param(first,"hb_shared_record","-1");
    API.set_param(first,"hb_shared_flush","");assert(g_key_context.target_root==2);
    API.set_param(first,"hb_shared_reset",""); /* Clip Undo */
    assert(g_key_context.active&&g_key_context.target_root==2&&g_key_context.target_mask==destination.target_mask);
    assert(g_sc_live[0][HB_SC_KEY].on&&!g_sc_recorded[HB_SC_KEY]);
    replay(first,0,HB_SC_KEY,1,2,destination.target_mask,0,201);API.set_param(first,"hb_shared_flush",""); /* Redo */
    assert(g_key_context.target_root==2);
    /* Live changes made DURING Record remain live too. */
    API.set_param(first,"hb_shared_record","0");g_sc_count=g_sc_head=0;
    destination.target_root=4;destination.target_mask=hb_explicit_scale_mask(4,1);hb_key_commit(first,destination);
    assert(!g_sc_count);
    API.set_param(first,"hb_shared_handoff","0");API.set_param(first,"hb_shared_record","-1");
    API.set_param(first,"hb_shared_reset","");assert(g_sc_live[0][HB_SC_KEY].on&&g_key_context.target_root==4);
    /* Recordable actions have their own owner and cannot displace live. */
    API.set_param(first,"hb_shared_record","0");
    first->key_action=1;destination.target_root=7;destination.target_mask=hb_explicit_scale_mask(7,2);hb_key_commit(first,destination);
    assert(g_sc_count==1&&g_key_context.target_root==4&&g_sc_key_audition[0].on);
    API.set_param(first,"key_center","Off");assert(g_key_context.target_root==7);
    first->key_action=2;first->key_shift=1;first->key_return_after=0;hb_key_commit(first,destination);
    assert(g_key_context.target_root==8&&g_key_context.target_mask==hb_explicit_scale_mask(8,2));
    for(int poll=0;poll<20;poll++){API.get_param(first,"shared_context_snapshot",view,sizeof(view));hb_key_operations_sync();}
    assert(g_sc_key_sequence[0].count==2&&g_key_context.target_root==8);
    first->key_action=3;hb_key_commit(first,destination);assert(g_key_context.target_root==7&&g_key_context.target_mask==hb_explicit_scale_mask(7,2));
    first->key_action=4;hb_key_commit(first,destination);assert(g_key_context.target_root==4&&g_key_context.target_mask==hb_explicit_scale_mask(4,1));
    first->key_action=2;first->key_shift=-1;first->key_return_after=2;
    hb_key_commit(first,destination);assert(g_key_context.target_root==3);
    hb_key_commit(first,destination);assert(g_key_context.target_root==2);
    hb_key_commit(first,destination);assert(g_key_context.target_root==4&&g_sc_key_sequence[0].count==0);
    API.set_param(first,"hb_shared_handoff","0");assert(!g_sc_key_audition[0].on);
    /* Operation assignment/state retains IDs above the note-action bitfield. */
    API.set_param(first,"motion_operation","Key Return to Start");assert(first->motion.lanes[0].operation==HB_MO_KEY_RETURN);
    char state[32768];API.get_param(first,"state",state,sizeof(state));API.set_param(first,"motion_operation","Off");
    g_motion_settings_restored=0;API.set_param(first,"state",state);assert(first->motion.lanes[0].operation==HB_MO_KEY_RETURN);
    API.set_param(first,"motion_operation","Relative Key Center");API.set_param(first,"motion_amount","2");
    API.set_param(first,"motion_hold_1","On");hb_key_operations_sync();
    int root=g_key_context.target_root;for(int tick=0;tick<50;tick++)hb_key_operations_sync();assert(root==g_key_context.target_root);
    API.set_param(first,"motion_hold_1","Off");hb_key_operations_sync();
    API.set_param(first,"motion_hold_1","On");hb_key_operations_sync();assert(g_key_context.target_root==mod12(root+2));
    API.set_param(first,"motion_hold_1","Off");hb_key_operations_sync();
    root=g_key_context.target_root;
    API.set_param(first,"motion_gesture_1","Touch,1000");API.set_param(first,"motion_gesture_1","Up,50,1050");
    assert(g_key_context.target_root==mod12(root+2)&&!first->motion.held);
    API.set_param(first,"motion_gesture_1","Touch,1100");API.set_param(first,"motion_gesture_1","Up,50,1150");
    assert(g_key_context.target_root==mod12(root+4)&&!first->motion.held);
    first->key_action=0;
    API.set_param(first,"hb_shared_reset","All");assert(!g_key_context.active);
    API.destroy_instance(follower);API.destroy_instance(second);API.destroy_instance(first);
    puts("shared context: ownership, overlap, atomic aggregation, transpose, follower exclusion, capture, display and handoff pass");
}
