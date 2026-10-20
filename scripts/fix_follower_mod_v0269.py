#!/usr/bin/env python3
"""Fix Foll Mod interaction/state semantics and bump HarmonyBus to v0.2.69."""
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
    module["description"] = "Harmony Bus v0.2.69 — simplified Foll Mod panel and corrected modifier off/reset state"
    caps = module["capabilities"]
    hierarchy = caps["ui_hierarchy"]
    hierarchy["levels"]["root"]["name"] = "Harmony Bus 0.2.69"
    params = {str(p.get("key")): p for p in caps["chain_params"] if p.get("key")}
    keys = ["approach_mode", "approach", "approach_reset", "mod_scale_above", "mod_chrom_below"]
    panel = hierarchy["levels"]["follower_mod"]
    panel["name"] = "Foll Mod"
    # Keep the three performance controls adjacent at K6/K7/K8; empty slots are intentional.
    panel["params"] = [dict(params["approach_mode"]), dict(params["approach"]), {"key":"foll_mod_blank3","name":"","type":"string","access":"read"}, {"key":"foll_mod_blank4","name":"","type":"string","access":"read"}, {"key":"foll_mod_blank5","name":"","type":"string","access":"read"}, dict(params["approach_reset"]), dict(params["mod_scale_above"]), dict(params["mod_chrom_below"])]
    panel["knobs"] = ["approach_mode", "approach", "foll_mod_blank3", "foll_mod_blank4", "foll_mod_blank5", "approach_reset", "mod_scale_above", "mod_chrom_below"]
    for key in ("foll_mod_blank3","foll_mod_blank4","foll_mod_blank5"):
        if key not in params:
            caps["chain_params"].append({"key":key,"name":"","type":"string","access":"read"})
    MODULE_JSON.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP_C.read_text().replace('/* Harmony Bus v0.2.68 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.68"','/* Harmony Bus v0.2.69 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.69"')
    # Off must really disarm. Reset is a command and always returns to Off.
    source = source.replace('if(!strcmp(key,"mod_scale_above")){int down=parameter[0]==\'1\'||!strcmp(parameter,"On");if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;}else instance->approach_above_held=(uint8_t)down;return;}', 'if(!strcmp(key,"mod_scale_above")){int down=parameter[0]==\'1\'||!strcmp(parameter,"On");if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=2;else if(instance->approach_pad_armed==2)instance->approach_pad_armed=1;}else instance->approach_above_held=(uint8_t)down;return;}')
    source = source.replace('if(!strcmp(key,"mod_chrom_below")){int down=parameter[0]==\'1\'||!strcmp(parameter,"On");if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;}else instance->approach_below_held=(uint8_t)down;return;}', 'if(!strcmp(key,"mod_chrom_below")){int down=parameter[0]==\'1\'||!strcmp(parameter,"On");if(g_bus.approach_mode==0){if(down)instance->approach_pad_armed=0;else if(instance->approach_pad_armed==0)instance->approach_pad_armed=1;}else instance->approach_below_held=(uint8_t)down;return;}')
    # Blank cells are display-only and deliberately empty.
    marker = 'if(!strcmp(key,"version"))return snprintf(buffer,(size_t)length,"%s",HB_VERSION);'
    source = source.replace(marker, marker+'if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");', 1)
    DSP_C.write_text(source)

if __name__ == "__main__":
    main()
