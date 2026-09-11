#!/usr/bin/env python3
"""Make Touch Perf an as_page canvas adjacent to Foll Mod and bump to v0.2.73."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"


def main() -> None:
    module = json.loads(MODULE.read_text())
    module["name"] = "Harmony Bus 0.2.73"
    module["abbrev"] = "HB273"
    module["version"] = "0.2.73"
    module["description"] = "Harmony Bus v0.2.73 — Touch Perf is an adjacent jog-accessible canvas page"

    caps = module["capabilities"]
    hierarchy = caps["ui_hierarchy"]
    hierarchy["levels"]["root"]["name"] = "Harmony Bus 0.2.73"

    params_by_key = {str(param.get("key")): param for param in caps["chain_params"] if param.get("key")}
    canvas_param = params_by_key["follower_mod_canvas"]
    canvas_param.update({
        "name": "Touch Perf",
        "type": "canvas",
        "canvas_script": "canvas.js",
        "canvas_overlay": "follower_mod",
        "as_page": True,
        "show_value": False,
    })

    panel = hierarchy["levels"]["follower_mod"]
    panel["name"] = "Foll Mod"

    # The normal Foll Mod page keeps K5 visually empty. The Touch Perf canvas is
    # a separate page in this level's jog rotation, not a fake knob/cell.
    blank5 = params_by_key.get("foll_mod_blank5")
    if blank5 is None:
        blank5 = {"key": "foll_mod_blank5", "name": "", "type": "string", "access": "read"}
        caps["chain_params"].append(blank5)
        params_by_key["foll_mod_blank5"] = blank5

    standard_keys = [
        "approach_mode",
        "approach",
        "foll_mod_blank3",
        "foll_mod_blank4",
        "foll_mod_blank5",
        "approach_reset",
        "mod_scale_above",
        "mod_chrom_below",
    ]
    panel["params"] = [dict(params_by_key[key]) for key in standard_keys] + [dict(canvas_param)]
    panel["knobs"] = standard_keys

    MODULE.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP.read_text()
    source = re.sub(
        r'/\* Harmony Bus v[0-9.]+ — Schwung MIDI FX\. \*/\n#define HB_VERSION "[0-9.]+"',
        '/* Harmony Bus v0.2.73 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.73"',
        source,
        count=1,
    )
    DSP.write_text(source)


if __name__ == "__main__":
    main()
