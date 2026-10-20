#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
C_PATH = ROOT / "modules/harmonybus/dsp/harmonybus.c"
MODULE_PATH = ROOT / "modules/harmonybus/module.json"


def replace_once(source: str, before: str, after: str, label: str) -> str:
    """Replace one exact source seam, while remaining idempotent."""
    if after in source:
        return source
    count: int = source.count(before)
    if count != 1:
        raise RuntimeError(f"{label}: expected one seam, found {count}")
    return source.replace(before, after, 1)


def main() -> None:
    """Publish v0.2.95 global master transpose semantics and UI."""
    source: str = C_PATH.read_text()
    source = source.replace("Harmony Bus v0.2.94", "Harmony Bus v0.2.95")
    source = source.replace('#define HB_VERSION "0.2.94"', '#define HB_VERSION "0.2.95"')

    # Conductor Render To Ch must hear the same global transpose as the local
    # conductor output instead of mirroring the original pre-transpose pitch.
    old_conductor_render: str = '''    /* Conductor rendering is monitoring only: mirror the original played pitch
       while the local event continues through normal harmony inference. */
    uint8_t packet[4];
    packet[0]=(uint8_t)(0x20 | (is_on?0x09:0x08));
    packet[1]=(uint8_t)((is_on?0x90:0x80) | (instance->render_channel & 0x0F));
    packet[2]=(uint8_t)(note & 0x7F);
    packet[3]=(uint8_t)(is_on?velocity:0);
    int sent=g_host->midi_inject_to_move(packet,4);
    if(sent==4){instance->render_count++;instance->render_last_note=note;}
'''
    new_conductor_render: str = '''    /* Global Transpose is the master musical transpose, so every rendered
       destination must hear the same pitch shift as the local conductor. */
    int rendered_note=note+g_bus.global_transpose;
    if(rendered_note<0)rendered_note=0;
    if(rendered_note>127)rendered_note=127;
    uint8_t packet[4];
    packet[0]=(uint8_t)(0x20 | (is_on?0x09:0x08));
    packet[1]=(uint8_t)((is_on?0x90:0x80) | (instance->render_channel & 0x0F));
    packet[2]=(uint8_t)(rendered_note & 0x7F);
    packet[3]=(uint8_t)(is_on?velocity:0);
    int sent=g_host->midi_inject_to_move(packet,4);
    if(sent==4){instance->render_count++;instance->render_last_note=rendered_note;}
'''
    source = replace_once(source, old_conductor_render, new_conductor_render, "conductor rendered transpose")

    # Role-change cleanup has to send its OFF at the same transposed pitch that
    # conductor rendering used for the matching ON.
    old_flush: str = '''            /* Conductor Render To Ch is a pass-through monitor copy. */
            render_pitch=source_note;
'''
    new_flush: str = '''            /* Match the globally transposed conductor render note exactly. */
            render_pitch=source_note+g_bus.global_transpose;
            if(render_pitch<0)render_pitch=0;
            if(render_pitch>127)render_pitch=127;
'''
    source = replace_once(source, old_flush, new_flush, "conductor role-flush transpose")

    # Direct intentionally bypasses harmony remapping, but it must not bypass a
    # master/global transpose. Every follower destination therefore moves too.
    old_direct: str = '    if(travel==5)return source_note;\n'
    new_direct: str = '''    if(travel==5){
        int direct=source_note+g_bus.global_transpose;
        if(direct<0)direct=0;
        if(direct>127)direct=127;
        return direct;
    }
'''
    source = replace_once(source, old_direct, new_direct, "Direct honors global transpose")

    # Make the relationship visible: Pre-X Root is the currently effective
    # harmony root before the master transpose; Final Root/Harmony are exactly
    # what all followers/render destinations are following.
    get_param_seam: str = 'if(!strcmp(key,"next_lookahead")){int index=g_bus.next_lookahead;'
    get_param_prefix: str = '''if(!strcmp(key,"pretranspose_root")){
    hb_harmony_t final_harmony=bus_read();
    if(!final_harmony.valid)return snprintf(buffer,(size_t)length,"--");
    hb_harmony_t pre=hb_transpose_harmony(final_harmony,-g_bus.global_transpose);
    return snprintf(buffer,(size_t)length,"%s",hb_pc_display(pre.root_pc,pre));
}
if(!strcmp(key,"final_root")){
    hb_harmony_t final_harmony=bus_read();
    return snprintf(buffer,(size_t)length,"%s",final_harmony.valid?hb_pc_display(final_harmony.root_pc,final_harmony):"--");
}
if(!strcmp(key,"final_harmony"))return hb_format_harmony(buffer,length,bus_read());
'''
    source = replace_once(source, get_param_seam, get_param_prefix + get_param_seam, "transpose readouts")

    C_PATH.write_text(source)

    module: dict = json.loads(MODULE_PATH.read_text())
    module["version"] = "0.2.95"
    module["name"] = "Harmony Bus 0.2.95"
    module["abbrev"] = "HB295"
    module["description"] = "Harmony Bus v0.2.95 — global transpose with explicit final harmony/root"
    levels: dict = module["capabilities"]["ui_hierarchy"]["levels"]
    root: dict = levels["root"]
    root["name"] = "Harmony Bus 0.2.95"

    levels["global_transpose"] = {
        "name": "Global",
        "params": [
            {
                "key": "transpose",
                "name": "Global Xpose",
                "type": "int",
                "min": -24,
                "max": 24,
                "step": 1,
                "default": 0,
            },
            {
                "key": "pretranspose_root",
                "name": "Pre-X Root",
                "type": "string",
                "access": "read",
            },
            {
                "key": "final_root",
                "name": "Final Root",
                "type": "string",
                "access": "read",
            },
            {
                "key": "final_harmony",
                "name": "Final Harmony",
                "type": "string",
                "access": "read",
            },
        ],
        "knobs": ["transpose", "pretranspose_root", "final_root", "final_harmony"],
    }

    if not any(isinstance(item, dict) and item.get("level") == "global_transpose" for item in root["params"]):
        insert_at: int = next(
            (index for index, item in enumerate(root["params"])
             if isinstance(item, dict) and item.get("level") == "next_harm"),
            len(root["params"]),
        )
        root["params"].insert(insert_at, {"level": "global_transpose", "label": "Global"})

    MODULE_PATH.write_text(json.dumps(module, indent=2) + "\n")


if __name__ == "__main__":
    main()
