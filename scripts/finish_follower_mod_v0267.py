#!/usr/bin/env python3
"""Finish the Follower Mod panel and bump HarmonyBus to v0.2.67."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_JSON = ROOT / "modules/harmonybus/module.json"
DSP_C = ROOT / "modules/harmonybus/dsp/harmonybus.c"


def upsert_param(params: list[dict[str, object]], spec: dict[str, object]) -> None:
    key = spec["key"]
    for index, item in enumerate(params):
        if item.get("key") == key:
            params[index] = spec
            return
    params.append(spec)


def main() -> None:
    module = json.loads(MODULE_JSON.read_text())
    module["name"] = "Harmony Bus 0.2.67"
    module["abbrev"] = "HB267"
    module["version"] = "0.2.67"
    module["description"] = (
        "Harmony Bus v0.2.67 — completed follower modifier panel with Next/Held, "
        "manual approach, and dedicated K6/K7/K8 performance controls"
    )

    hierarchy = module["capabilities"]["ui_hierarchy"]
    hierarchy["levels"]["root"]["name"] = "Harmony Bus 0.2.67"

    follower_mod = hierarchy["levels"]["follower_mod"]
    follower_mod["name"] = "Follower Mod"
    follower_mod["params"] = [
        {"key": "approach_mode"},
        {"key": "approach"},
        {"key": "follower_mod_canvas"},
        {"key": "approach_status"},
        {"key": "touch_hint"},
        {"key": "approach_reset"},
        {"key": "mod_scale_above"},
        {"key": "mod_chrom_below"},
    ]
    follower_mod["knobs"] = [
        "approach_mode",
        "approach",
        "follower_mod_canvas",
        "approach_status",
        "touch_hint",
        "approach_reset",
        "mod_scale_above",
        "mod_chrom_below",
    ]

    chain_params: list[dict[str, object]] = module["capabilities"]["chain_params"]
    upsert_param(
        chain_params,
        {
            "key": "approach_mode",
            "name": "Behavior",
            "type": "enum",
            "options": ["Next", "Held"],
            "options_as_string": True,
            "default": "Next",
        },
    )
    upsert_param(
        chain_params,
        {
            "key": "approach",
            "name": "Manual",
            "type": "enum",
            "options": ["Chrom Below", "Off", "Scale Above"],
            "options_as_string": True,
            "default": "Off",
        },
    )
    upsert_param(
        chain_params,
        {
            "key": "follower_mod_canvas",
            "name": "Touch Perf",
            "type": "canvas",
            "canvas_script": "canvas.js",
            "canvas_overlay": "follower_mod",
            "show_value": False,
        },
    )
    upsert_param(
        chain_params,
        {
            "key": "approach_status",
            "name": "Armed",
            "type": "string",
            "access": "read",
        },
    )
    upsert_param(
        chain_params,
        {
            "key": "touch_hint",
            "name": "Touch",
            "type": "string",
            "access": "read",
        },
    )
    upsert_param(
        chain_params,
        {
            "key": "approach_reset",
            "name": "K6 Reset",
            "type": "enum",
            "options": ["Off", "Reset"],
            "options_as_string": True,
            "default": "Off",
        },
    )
    upsert_param(
        chain_params,
        {
            "key": "mod_scale_above",
            "name": "K7 Scale Up",
            "type": "enum",
            "options": ["Off", "On"],
            "options_as_string": True,
            "default": "Off",
        },
    )
    upsert_param(
        chain_params,
        {
            "key": "mod_chrom_below",
            "name": "K8 Chrom Dn",
            "type": "enum",
            "options": ["Off", "On"],
            "options_as_string": True,
            "default": "Off",
        },
    )

    MODULE_JSON.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP_C.read_text()
    source = source.replace(
        "/* Harmony Bus v0.2.65 — Schwung MIDI FX. */\n#define HB_VERSION \"0.2.65\"",
        "/* Harmony Bus v0.2.67 — Schwung MIDI FX. */\n#define HB_VERSION \"0.2.67\"",
    )
    source = source.replace(
        "/* Harmony Bus v0.2.66 — Schwung MIDI FX. */\n#define HB_VERSION \"0.2.66\"",
        "/* Harmony Bus v0.2.67 — Schwung MIDI FX. */\n#define HB_VERSION \"0.2.67\"",
    )

    marker = 'if(!strcmp(key,"approach_above_pad")){int down=parameter[0]==\'1\';if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;}else instance->approach_above_held=(uint8_t)down;return;}if(!strcmp(key,"track_role")'
    replacement = 'if(!strcmp(key,"approach_above_pad")){int down=parameter[0]==\'1\';if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;}else instance->approach_above_held=(uint8_t)down;return;}if(!strcmp(key,"approach_reset")){if(parameter[0]==\'1\'||!strcmp(parameter,"Reset")){instance->approach_pad_armed=1;instance->approach_below_held=0;instance->approach_above_held=0;g_bus.approach_control=1;}return;}if(!strcmp(key,"mod_scale_above")){int down=parameter[0]==\'1\'||!strcmp(parameter,"On");if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;}else instance->approach_above_held=(uint8_t)down;return;}if(!strcmp(key,"mod_chrom_below")){int down=parameter[0]==\'1\'||!strcmp(parameter,"On");if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;}else instance->approach_below_held=(uint8_t)down;return;}if(!strcmp(key,"track_role")'
    if marker in source:
        source = source.replace(marker, replacement, 1)

    get_marker = 'if(!strcmp(key,"version"))return snprintf(buffer,(size_t)length,"%s",HB_VERSION);if(!strcmp(key,"approach"))'
    get_replacement = 'if(!strcmp(key,"version"))return snprintf(buffer,(size_t)length,"%s",HB_VERSION);if(!strcmp(key,"approach_status")){int active=hb_active_approach(instance);return snprintf(buffer,(size_t)length,"%s",APPROACH_OPTS[(active>=0&&active<3)?active:1]);}if(!strcmp(key,"touch_hint"))return snprintf(buffer,(size_t)length,"K6 OFF K7 UP K8 DN");if(!strcmp(key,"approach_reset"))return snprintf(buffer,(size_t)length,"Off");if(!strcmp(key,"mod_scale_above")){int on=g_bus.approach_mode==0?(instance->approach_pad_armed==2):(instance->approach_above_held!=0);return snprintf(buffer,(size_t)length,"%s",on?"On":"Off");}if(!strcmp(key,"mod_chrom_below")){int on=g_bus.approach_mode==0?(instance->approach_pad_armed==0):(instance->approach_below_held!=0);return snprintf(buffer,(size_t)length,"%s",on?"On":"Off");}if(!strcmp(key,"approach"))'
    if get_marker in source:
        source = source.replace(get_marker, get_replacement, 1)

    DSP_C.write_text(source)


if __name__ == "__main__":
    main()
