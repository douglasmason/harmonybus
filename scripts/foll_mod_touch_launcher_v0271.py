#!/usr/bin/env python3
"""Restore a visible Touch Perf launcher at K5 and bump to v0.2.71."""
from __future__ import annotations
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
MODULE=ROOT/'modules/harmonybus/module.json'
DSP=ROOT/'modules/harmonybus/dsp/harmonybus.c'
def main()->None:
    module=json.loads(MODULE.read_text())
    module['name']='Harmony Bus 0.2.71'; module['abbrev']='HB271'; module['version']='0.2.71'
    module['description']='Harmony Bus v0.2.71 — Foll Mod Touch Perf launcher on K5'
    caps=module['capabilities']; hierarchy=caps['ui_hierarchy']; hierarchy['levels']['root']['name']='Harmony Bus 0.2.71'
    params={str(p.get('key')):p for p in caps['chain_params'] if p.get('key')}
    panel=hierarchy['levels']['follower_mod']; panel['name']='Foll Mod'
    keys=['approach_mode','approach','foll_mod_blank3','foll_mod_blank4','follower_mod_canvas','approach_reset','mod_scale_above','mod_chrom_below']
    panel['params']=[dict(params[k]) for k in keys]; panel['knobs']=keys
    MODULE.write_text(json.dumps(module,indent=2)+'\n')
    source=DSP.read_text()
    import re
    source=re.sub(r'/\* Harmony Bus v[0-9.]+ — Schwung MIDI FX\. \*/\n#define HB_VERSION "[0-9.]+"','/* Harmony Bus v0.2.71 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.71"',source,count=1)
    DSP.write_text(source)
if __name__=='__main__': main()
