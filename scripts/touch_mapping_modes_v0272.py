#!/usr/bin/env python3
"""Add fixed K2-K5 touch mapping selectors, Direct travel, and 3-state approach behavior."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"
CANVAS = ROOT / "modules/harmonybus/canvas.js"


def main() -> None:
    module = json.loads(MODULE.read_text())
    module["name"] = "Harmony Bus 0.2.72"
    module["abbrev"] = "HB272"
    module["version"] = "0.2.72"
    module["description"] = (
        "Harmony Bus v0.2.72 — fixed Touch Perf mapping selectors, Direct follower travel, "
        "and Off/Next/Held approach behavior"
    )
    caps = module["capabilities"]
    hierarchy = caps["ui_hierarchy"]
    hierarchy["levels"]["root"]["name"] = "Harmony Bus 0.2.72"

    # Update both hierarchy-local and chain-param metadata so Schwung always sees enums.
    for collection in (caps["chain_params"], hierarchy["levels"]["follower_source"]["params"], hierarchy["levels"]["follower_mod"]["params"]):
        for spec in collection:
            if spec.get("key") == "travel_map":
                spec["type"] = "enum"
                spec["options"] = ["Relative", "Closest", "Upward", "Closest Split", "Downward", "Direct"]
                spec["options_as_string"] = True
            if spec.get("key") == "approach_mode":
                spec["type"] = "enum"
                spec["options"] = ["Next", "Held", "Off"]
                spec["options_as_string"] = True
                spec["default"] = "Next"

    MODULE.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP.read_text()
    source = re.sub(
        r'/\* Harmony Bus v[0-9.]+ — Schwung MIDI FX\. \*/\n#define HB_VERSION "[0-9.]+"',
        '/* Harmony Bus v0.2.72 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.72"',
        source,
        count=1,
    )
    source = source.replace('if(value<0||value>4){', 'if(value<0||value>5){', 1)
    source = source.replace('if(value<0||value>4)value=0;', 'if(value<0||value>5)value=0;', 1)
    source = source.replace(
        'static const char *APPROACH_MODE_OPTS[]={"Next","Held"};',
        'static const char *APPROACH_MODE_OPTS[]={"Next","Held","Off"};',
    )
    source = source.replace(
        '}else if(!strcmp(key,"travel_map")){static const char *opts[]={"Relative","Closest","Upward","Closest Split","Downward"};hb_set_global_travel_map(enum_index(parameter,opts,5,hb_global_travel_map()));}',
        '}else if(!strcmp(key,"travel_map")){static const char *opts[]={"Relative","Closest","Upward","Closest Split","Downward","Direct"};hb_set_global_travel_map(enum_index(parameter,opts,6,hb_global_travel_map()));}',
    )
    source = source.replace(
        '}else if(!strcmp(key,"approach_mode")){g_bus.approach_mode=enum_index(parameter,APPROACH_MODE_OPTS,2,g_bus.approach_mode);instance->approach_pad_armed=1;instance->approach_below_held=0;instance->approach_above_held=0;}',
        '}else if(!strcmp(key,"approach_mode")){g_bus.approach_mode=enum_index(parameter,APPROACH_MODE_OPTS,3,g_bus.approach_mode);instance->approach_pad_armed=1;instance->approach_below_held=0;instance->approach_above_held=0;}',
    )
    source = source.replace(
        'if(!strcmp(key,"approach_mode"))return snprintf(buffer,(size_t)length,"%s",APPROACH_MODE_OPTS[g_bus.approach_mode?1:0]);',
        'if(!strcmp(key,"approach_mode")){int mode=g_bus.approach_mode;if(mode<0||mode>2)mode=0;return snprintf(buffer,(size_t)length,"%s",APPROACH_MODE_OPTS[mode]);}',
    )
    source = source.replace(
        'if(!strcmp(key,"travel_map")){static const char *opts[]={"Relative","Closest","Upward","Closest Split","Downward"};return snprintf(buffer,(size_t)length,"%s",opts[hb_global_travel_map()]);}',
        'if(!strcmp(key,"travel_map")){static const char *opts[]={"Relative","Closest","Upward","Closest Split","Downward","Direct"};int travel=hb_global_travel_map();if(travel<0||travel>5)travel=0;return snprintf(buffer,(size_t)length,"%s",opts[travel]);}',
    )
    # Direct bypasses follower pitch mapping while retaining the normal render path/channel/timing.
    source = source.replace(
        'int travel=hb_global_travel_map();\n    if(travel==0)',
        'int travel=hb_global_travel_map();\n    if(travel==5)return source_note;\n    if(travel==0)',
        1,
    )
    # Third behavior state disables touch approach modifiers completely.
    source = source.replace(
        'static int hb_active_approach(const Inst *instance){\n    if(!instance)return 1;',
        'static int hb_active_approach(const Inst *instance){\n    if(!instance)return 1;\n    if(g_bus.approach_mode==2)return 1;',
        1,
    )
    source = source.replace(
        'if(!strcmp(key,"approach_below_pad")){int down=parameter[0]==\'1\';if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;}else instance->approach_below_held=(uint8_t)down;return;}',
        'if(!strcmp(key,"approach_below_pad")){int down=parameter[0]==\'1\';if(g_bus.approach_mode==2)return;if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;}else instance->approach_below_held=(uint8_t)down;return;}',
    )
    source = source.replace(
        'if(!strcmp(key,"approach_above_pad")){int down=parameter[0]==\'1\';if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;}else instance->approach_above_held=(uint8_t)down;return;}',
        'if(!strcmp(key,"approach_above_pad")){int down=parameter[0]==\'1\';if(g_bus.approach_mode==2)return;if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;}else instance->approach_above_held=(uint8_t)down;return;}',
    )
    DSP.write_text(source)

    canvas = CANVAS.read_text()
    canvas = canvas.replace('const MODES = ["Next", "Held"];', 'const MODES = ["Next", "Held", "Off"];')
    # Add K2-K5 fixed touch selectors. K1 touch remains intentionally unhandled/inert.
    canvas = canvas.replace(
        'const TOUCH_K6 = 5;',
        'const TOUCH_K2 = 1;\nconst TOUCH_K3 = 2;\nconst TOUCH_K4 = 3;\nconst TOUCH_K5 = 4;\nconst TOUCH_K6 = 5;',
    )
    marker = 'function handleTouch(ctx, state, note, down) {\n'
    injected = '''function handleTouch(ctx, state, note, down) {\n    /* K1 touch is intentionally inert; K1 rotation only selects Next/Held/Off. */\n    if (down && note === TOUCH_K2) { ctx.setParam("travel_map", "Direct"); state.lastAction = "DIRECT"; return true; }\n    if (down && note === TOUCH_K3) { ctx.setParam("travel_map", "Relative"); state.lastAction = "RELATIVE"; return true; }\n    if (down && note === TOUCH_K4) { ctx.setParam("travel_map", "Closest"); state.lastAction = "CLOSEST"; return true; }\n    if (down && note === TOUCH_K5) { ctx.setParam("travel_map", "Closest Split"); state.lastAction = "CLOSEST SPLIT"; return true; }\n    if (!down && (note === TOUCH_K2 || note === TOUCH_K3 || note === TOUCH_K4 || note === TOUCH_K5)) return true;\n'''
    if marker in canvas:
        canvas = canvas.replace(marker, injected, 1)
    canvas = canvas.replace(
        '    ctx.print(2, 38, "TOUCH K6  OFF / RESET", 1);\n    ctx.print(2, 46, "TOUCH K7  SCALE ABOVE", 1);\n    ctx.print(2, 54, "TOUCH K8  CHROM BELOW", 1);',
        '    ctx.print(2, 34, "K2 DIRECT  K3 REL  K4 CLOSE", 1);\n    ctx.print(2, 42, "K5 SPLIT   K6 RESET", 1);\n    ctx.print(2, 50, "K7 SCALE+  K8 CHROM-", 1);\n    ctx.print(2, 58, `LAST: ${ctx.state.lastAction}`, 1);',
    )
    CANVAS.write_text(canvas)


if __name__ == "__main__":
    main()
