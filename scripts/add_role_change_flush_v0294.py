#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
C_PATH = ROOT / "modules/harmonybus/dsp/harmonybus.c"
MODULE_PATH = ROOT / "modules/harmonybus/module.json"


def replace_once(source: str, before: str, after: str, label: str) -> str:
    if after in source:
        return source
    count = source.count(before)
    if count != 1:
        raise RuntimeError(f"{label}: expected one seam, found {count}")
    return source.replace(before, after, 1)


def main() -> None:
    source = C_PATH.read_text()
    source = source.replace("Harmony Bus v0.2.93", "Harmony Bus v0.2.94")
    source = source.replace('#define HB_VERSION "0.2.93"', '#define HB_VERSION "0.2.94"')

    old_struct = "int local_sense_count; uint8_t local_sense_notes[64]; int global_timing_restored; } Inst;"
    new_struct = (
        "int local_sense_count; uint8_t local_sense_notes[64]; int global_timing_restored; "
        "uint8_t role_flush_pending[128]; int role_flush_cursor; } Inst;"
    )
    source = replace_once(source, old_struct, new_struct, "Inst role-flush state")

    seam = "static void hb_clear_instance_note_state(Inst *instance){\n"
    helpers = r'''static void hb_inject_note_off_to_render(Inst *instance,int pitch){
    if(!instance||pitch<0||pitch>127||instance->render_channel<0||!g_host||!g_host->midi_inject_to_move)return;
    uint8_t packet[4];
    packet[0]=0x28; /* cable 2 + note-off CIN */
    packet[1]=(uint8_t)(0x80 | (instance->render_channel & 0x0F));
    packet[2]=(uint8_t)(pitch & 0x7F);
    packet[3]=0;
    int sent=g_host->midi_inject_to_move(packet,4);
    if(sent==4){instance->render_count++;instance->render_last_note=pitch;}
    else instance->render_fail_count++;
}
static void hb_prepare_role_change_flush(Inst *instance){
    if(!instance)return;
    memset(instance->role_flush_pending,0,sizeof(instance->role_flush_pending));
    instance->role_flush_cursor=0;
    for(int source_note=0;source_note<128;source_note++){
        int local_pitch=-1;
        int render_pitch=-1;
        if(instance->role==0 && (instance->held_count[source_note]>0 || instance->pending_off_frames[source_note]>0)){
            local_pitch=instance->mapped[source_note];
            if(local_pitch<0){
                local_pitch=source_note+g_bus.global_transpose;
                if(local_pitch<0)local_pitch=0;
                if(local_pitch>127)local_pitch=127;
            }
            /* Conductor Render To Ch is a pass-through monitor copy. */
            render_pitch=source_note;
        }else if(instance->role==1 && instance->follower_sounding[source_note]){
            local_pitch=instance->mapped[source_note];
            if(local_pitch<0)local_pitch=source_note;
            render_pitch=local_pitch;
        }
        if(local_pitch>=0&&local_pitch<128)instance->role_flush_pending[local_pitch]=1;
        if(render_pitch>=0&&render_pitch<128)hb_inject_note_off_to_render(instance,render_pitch);
    }
}
static int hb_emit_role_change_flush(Inst *instance,uint8_t output[][3],int lengths[],int max_output){
    if(!instance||!output||!lengths||max_output<=0)return 0;
    int emitted=0;
    for(int pitch=instance->role_flush_cursor;pitch<128&&emitted<max_output;pitch++){
        instance->role_flush_cursor=pitch+1;
        if(!instance->role_flush_pending[pitch])continue;
        instance->role_flush_pending[pitch]=0;
        output[emitted][0]=0x80;
        output[emitted][1]=(uint8_t)pitch;
        output[emitted][2]=0;
        lengths[emitted]=3;
        emitted++;
    }
    if(instance->role_flush_cursor>=128)instance->role_flush_cursor=0;
    return emitted;
}
'''
    source = replace_once(source, seam, helpers + seam, "role-flush helpers")

    old_role = '''    if(new_role!=instance->role){
        hb_clear_instance_note_state(instance);
        memset(instance->source_seen,0,sizeof(instance->source_seen));
        instance->role=new_role;
        instance->resolved_source_channel=-1;
    }
'''
    new_role = '''    if(new_role!=instance->role){
        /* Role changes are a hard voice boundary. Flush what the OLD role
           actually sounded before clearing its ledgers, otherwise an ON can
           survive after the instance becomes Follower/Conductor/Off. */
        hb_prepare_role_change_flush(instance);
        hb_clear_instance_note_state(instance);
        memset(instance->source_seen,0,sizeof(instance->source_seen));
        instance->role=new_role;
        instance->resolved_source_channel=-1;
    }
'''
    source = replace_once(source, old_role, new_role, "role transition flush")

    old_tick = '''    g_bus.global_tick_count++;
    if(instance->role==1)hb_sync_from_monitor(instance);
'''
    new_tick = '''    g_bus.global_tick_count++;
    /* set_param cannot return local MIDI, so role-change OFFs are drained on
       the next audio tick before the new role can synthesize anything. */
    int role_flush_emitted=hb_emit_role_change_flush(instance,output,lengths,max_output);
    if(role_flush_emitted>0)return role_flush_emitted;
    if(instance->role==1)hb_sync_from_monitor(instance);
'''
    source = replace_once(source, old_tick, new_tick, "tick role flush")

    C_PATH.write_text(source)
    module = json.loads(MODULE_PATH.read_text())
    module["version"] = "0.2.94"
    levels = module.get("capabilities", {}).get("ui_hierarchy", {}).get("levels", {})
    root = levels.get("root")
    if isinstance(root, dict):
        root["name"] = "Harmony Bus 0.2.94"
    MODULE_PATH.write_text(json.dumps(module, indent=2) + "\n")


if __name__ == "__main__":
    main()
