#!/usr/bin/env python3
"""Fix Foll Mod Off semantics and bump HarmonyBus to v0.2.69."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_JSON = ROOT / "modules/harmonybus/module.json"
DSP_C = ROOT / "modules/harmonybus/dsp/harmonybus.c"


def main() -> None:
    module = json.loads(MODULE_JSON.read_text())
    module["name"] = "Harmony Bus 0.2.69"
    module["abbrev"] = "HB269"
    module["version"] = "0.2.69"
    module["description"] = (
        "Harmony Bus v0.2.69 — fixes Foll Mod discrete Off semantics for "
        "Scale Above and Chrom Below controls"
    )
    module["capabilities"]["ui_hierarchy"]["levels"]["root"]["name"] = "Harmony Bus 0.2.69"
    module["capabilities"]["ui_hierarchy"]["levels"]["follower_mod"]["name"] = "Foll Mod"
    MODULE_JSON.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP_C.read_text()
    source = source.replace(
        "/* Harmony Bus v0.2.68 — Schwung MIDI FX. */\n#define HB_VERSION \"0.2.68\"",
        "/* Harmony Bus v0.2.69 — Schwung MIDI FX. */\n#define HB_VERSION \"0.2.69\"",
    )

    old_scale = (
        'if(!strcmp(key,"mod_scale_above")){int down=parameter[0]==\'1\'||!strcmp(parameter,"On");'
        'if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;}'
        'else instance->approach_above_held=(uint8_t)down;return;}'
    )
    new_scale = (
        'if(!strcmp(key,"mod_scale_above")){int down=parameter[0]==\'1\'||!strcmp(parameter,"On");'
        'if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;'
        'else if(instance->approach_pad_armed==2)instance->approach_pad_armed=1;}'
        'else instance->approach_above_held=(uint8_t)down;return;}'
    )
    old_chrom = (
        'if(!strcmp(key,"mod_chrom_below")){int down=parameter[0]==\'1\'||!strcmp(parameter,"On");'
        'if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;}'
        'else instance->approach_below_held=(uint8_t)down;return;}'
    )
    new_chrom = (
        'if(!strcmp(key,"mod_chrom_below")){int down=parameter[0]==\'1\'||!strcmp(parameter,"On");'
        'if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;'
        'else if(instance->approach_pad_armed==0)instance->approach_pad_armed=1;}'
        'else instance->approach_below_held=(uint8_t)down;return;}'
    )

    if old_scale not in source or old_chrom not in source:
        raise SystemExit("Expected v0.2.68 follower modifier handlers not found")
    source = source.replace(old_scale, new_scale, 1)
    source = source.replace(old_chrom, new_chrom, 1)
    DSP_C.write_text(source)


if __name__ == "__main__":
    main()
