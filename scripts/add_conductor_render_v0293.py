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
    source = source.replace("Harmony Bus v0.2.92", "Harmony Bus v0.2.93")
    source = source.replace('#define HB_VERSION "0.2.92"', '#define HB_VERSION "0.2.93"')

    follower_helper = "static void hb_render_follower_event(Inst *instance,int mapped_note,int velocity,int is_on,int is_off,int recv_channel){\n"
    conductor_helper = (
        "static void hb_render_conductor_event(Inst *instance,int note,int velocity,int is_on,int is_off,int recv_channel){\n"
        "    if(!instance||instance->role!=0||instance->render_channel<0||!g_host||!g_host->midi_inject_to_move)return;\n"
        "    if(instance->render_channel==recv_channel)return; /* avoid self-echo loops */\n"
        "    if(!(is_on||is_off)||note<0||note>127)return;\n"
        "    /* Conductor rendering is monitoring only: mirror the original played pitch\n"
        "       while the local event continues through normal harmony inference. */\n"
        "    uint8_t packet[4];\n"
        "    packet[0]=(uint8_t)(0x20 | (is_on?0x09:0x08));\n"
        "    packet[1]=(uint8_t)((is_on?0x90:0x80) | (instance->render_channel & 0x0F));\n"
        "    packet[2]=(uint8_t)(note & 0x7F);\n"
        "    packet[3]=(uint8_t)(is_on?velocity:0);\n"
        "    int sent=g_host->midi_inject_to_move(packet,4);\n"
        "    if(sent==4){instance->render_count++;instance->render_last_note=note;}\n"
        "    else instance->render_fail_count++;\n"
        "}\n\n"
        + follower_helper
    )
    source = replace_once(source, follower_helper, conductor_helper, "conductor render helper")

    process_seam = (
        "if(instance->role==0){\n"
        "    if(is_on){"
    )
    process_after = (
        "if(instance->role==0){\n"
        "    if(instance->render_channel>=0)\n"
        "        hb_render_conductor_event(instance,note,length>=3?input[2]:0,is_on,is_off,input_channel);\n"
        "    if(is_on){"
    )
    source = replace_once(source, process_seam, process_after, "conductor process render")
    C_PATH.write_text(source)

    module = json.loads(MODULE_PATH.read_text())
    module["version"] = "0.2.93"
    hierarchy = module.get("capabilities", {}).get("ui_hierarchy", {}).get("levels", {})
    root = hierarchy.get("root")
    if isinstance(root, dict):
        root["name"] = "Harmony Bus 0.2.93"
    MODULE_PATH.write_text(json.dumps(module, indent=2) + "\n")


if __name__ == "__main__":
    main()
