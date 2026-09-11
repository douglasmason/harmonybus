#!/usr/bin/env python3
"""Make Foll Mod K6/K7/K8 true Schwung push buttons and bump to v0.2.77."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"


def main() -> None:
    module = json.loads(MODULE.read_text())
    module["name"] = "Harmony Bus 0.2.77"
    module["abbrev"] = "HB277"
    module["version"] = "0.2.77"
    module["description"] = "Harmony Bus v0.2.77 — Foll Mod K6-K8 are true momentary push buttons"
    module["capabilities"]["ui_hierarchy"]["levels"]["root"]["name"] = "Harmony Bus 0.2.77"

    chain_params = module["capabilities"]["chain_params"]
    params_by_key = {str(param.get("key")): param for param in chain_params if param.get("key")}

    button_specs = {
        "approach_reset": ("Reset", ["-", "Reset"]),
        "approach_scale_next": ("Scale Next", ["-", "Scale +"]),
        "approach_chrom_next": ("Chrom Next", ["-", "Chrom -"]),
    }
    for key, (name, options) in button_specs.items():
        param = params_by_key[key]
        param.clear()
        param.update({
            "key": key,
            "name": name,
            "type": "enum",
            "options": options,
            "options_as_string": True,
            "access": "write",
        })

    # Keep hierarchy copies in sync with canonical chain_params metadata.
    panel = module["capabilities"]["ui_hierarchy"]["levels"]["follower_mod"]
    panel_params_by_key = {
        str(param.get("key")): param for param in panel.get("params", []) if isinstance(param, dict) and param.get("key")
    }
    for key in button_specs:
        if key in panel_params_by_key:
            panel_params_by_key[key].clear()
            panel_params_by_key[key].update(dict(params_by_key[key]))

    MODULE.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP.read_text()
    source = re.sub(
        r'/\* Harmony Bus v[0-9.]+ — Schwung MIDI FX\. \*/\n#define HB_VERSION "[0-9.]+"',
        '/* Harmony Bus v0.2.77 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.77"',
        source,
        count=1,
    )
    DSP.write_text(source)


if __name__ == "__main__":
    main()
