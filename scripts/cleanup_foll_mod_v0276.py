#!/usr/bin/env python3
"""Clean accumulated Foll Mod getter migrations and bump HarmonyBus to v0.2.76."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"


def main() -> None:
    module = json.loads(MODULE.read_text())
    module["name"] = "Harmony Bus 0.2.76"
    module["abbrev"] = "HB276"
    module["version"] = "0.2.76"
    module["description"] = "Harmony Bus v0.2.76 — repaired standard Foll Mod build and cleaned accumulated getter migrations"
    module["capabilities"]["ui_hierarchy"]["levels"]["root"]["name"] = "Harmony Bus 0.2.76"
    MODULE.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP.read_text()
    source = re.sub(
        r'/\* Harmony Bus v[0-9.]+ — Schwung MIDI FX\. \*/\n#define HB_VERSION "[0-9.]+"',
        '/* Harmony Bus v0.2.76 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.76"',
        source,
        count=1,
    )

    # Older migrations repeatedly inserted this same blank-cell getter.
    blank_getter = 'if(!strcmp(key,"foll_mod_blank3")||!strcmp(key,"foll_mod_blank4")||!strcmp(key,"foll_mod_blank5"))return snprintf(buffer,(size_t)length,"");'
    source = re.sub(r'(?:' + re.escape(blank_getter) + r')+', blank_getter, source)

    # v0.2.75 accidentally closed get_param immediately after the Chrom Below getter.
    broken = 'if(!strcmp(key,"mod_chrom_below"))return snprintf(buffer,(size_t)length,"%s",g_bus.approach_control==0?"On":"Off");}if(!strcmp(key,"approach"))'
    fixed = 'if(!strcmp(key,"mod_chrom_below"))return snprintf(buffer,(size_t)length,"%s",g_bus.approach_control==0?"On":"Off");if(!strcmp(key,"approach"))'
    if broken in source:
        source = source.replace(broken, fixed, 1)

    # Guard against this class of migration corruption recurring silently.
    if source.count(blank_getter) != 1:
        raise RuntimeError("duplicate Foll Mod blank getters remain")
    if '"Off");}if(!strcmp(key,"approach"))' in source:
        raise RuntimeError("premature get_param close remains")
    if fixed not in source:
        raise RuntimeError("expected repaired Foll Mod getter sequence not found")

    DSP.write_text(source)


if __name__ == "__main__":
    main()
