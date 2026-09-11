#!/usr/bin/env python3
"""Repair Touch Perf as-page metadata and rewrite canvas.js cleanly for v0.2.74."""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / "modules/harmonybus/module.json"
DSP = ROOT / "modules/harmonybus/dsp/harmonybus.c"
CANVAS = ROOT / "modules/harmonybus/canvas.js"

CANVAS_SOURCE = r'''/* HarmonyBus Touch Perf page. */
const CC_KNOB_1 = 71;
const CC_KNOB_2 = 72;

const TOUCH_K2 = 1;
const TOUCH_K3 = 2;
const TOUCH_K4 = 3;
const TOUCH_K5 = 4;
const TOUCH_K6 = 5;
const TOUCH_K7 = 6;
const TOUCH_K8 = 7;

const APPROACHES = ["Chrom Below", "Off", "Scale Above"];
const MODES = ["Next", "Held", "Off"];

function relativeDelta(value) {
    if (value === 0 || value === 64) return 0;
    return value < 64 ? value : value - 128;
}

function readString(ctx, key, fallback) {
    try {
        const value = ctx.getParam(key);
        if (typeof value === "string" && value.length) return value;
    } catch (_) {}
    return fallback;
}

function setMode(ctx, state, index) {
    const clamped = Math.max(0, Math.min(MODES.length - 1, index));
    state.modeIndex = clamped;
    ctx.setParam("approach_mode", MODES[clamped]);
}

function setApproach(ctx, state, index) {
    const clamped = Math.max(0, Math.min(APPROACHES.length - 1, index));
    state.approachIndex = clamped;
    ctx.setParam("approach", APPROACHES[clamped]);
}

function resetModifier(ctx, state) {
    ctx.setParam("approach_below_pad", "0");
    ctx.setParam("approach_above_pad", "0");
    ctx.setParam("approach_mode", MODES[state.modeIndex]);
    setApproach(ctx, state, 1);
    state.lastAction = "RESET";
}

function selectTravel(ctx, state, value, label) {
    ctx.setParam("travel_map", value);
    state.lastAction = label;
}

function handleTouch(ctx, state, note, down) {
    /* K1 touch is intentionally inert. */
    if (note === TOUCH_K2) {
        if (down) selectTravel(ctx, state, "Direct", "DIRECT");
        return true;
    }
    if (note === TOUCH_K3) {
        if (down) selectTravel(ctx, state, "Relative", "RELATIVE");
        return true;
    }
    if (note === TOUCH_K4) {
        if (down) selectTravel(ctx, state, "Closest", "CLOSEST");
        return true;
    }
    if (note === TOUCH_K5) {
        if (down) selectTravel(ctx, state, "Closest Split", "CLOSEST SPLIT");
        return true;
    }
    if (note === TOUCH_K6) {
        if (down) resetModifier(ctx, state);
        return true;
    }
    if (note === TOUCH_K7) {
        ctx.setParam("approach_above_pad", down ? "1" : "0");
        if (down) state.lastAction = "SCALE ABOVE";
        return true;
    }
    if (note === TOUCH_K8) {
        ctx.setParam("approach_below_pad", down ? "1" : "0");
        if (down) state.lastAction = "CHROM BELOW";
        return true;
    }
    return false;
}

function onOpen(ctx) {
    const mode = readString(ctx, "approach_mode", "Next");
    const approach = readString(ctx, "approach", "Off");
    ctx.state.modeIndex = Math.max(0, MODES.indexOf(mode));
    ctx.state.approachIndex = Math.max(0, APPROACHES.indexOf(approach));
    ctx.state.lastAction = "READY";
}

function onMidi(ctx, payload) {
    const data = payload && (payload.data || payload.midi || payload.message || payload);
    if (!data || data.length < 3) return;

    const status = data[0] & 0xF0;
    const number = data[1] | 0;
    const value = data[2] | 0;

    if (status === 0x90 || status === 0x80) {
        const down = status === 0x90 && value >= 64;
        if (handleTouch(ctx, ctx.state, number, down)) return;
    }

    if (status !== 0xB0) return;
    const delta = relativeDelta(value);
    if (!delta) return;

    if (number === CC_KNOB_1) {
        setMode(ctx, ctx.state, ctx.state.modeIndex + (delta > 0 ? 1 : -1));
    } else if (number === CC_KNOB_2) {
        setApproach(ctx, ctx.state, ctx.state.approachIndex + (delta > 0 ? 1 : -1));
    }
}

function draw(ctx) {
    ctx.clear();
    ctx.print(2, 2, "TOUCH PERF", 1);
    ctx.print(2, 13, `K1 MODE ${MODES[ctx.state.modeIndex]}`, 1);
    ctx.print(2, 22, `K2 MAN ${APPROACHES[ctx.state.approachIndex]}`, 1);
    ctx.print(2, 33, "TOUCH: 2 DIR 3 REL 4 CLOSE", 1);
    ctx.print(2, 42, "5 SPLIT 6 RESET", 1);
    ctx.print(2, 51, "7 SCALE+ 8 CHROM-", 1);
    ctx.print(2, 60, ctx.state.lastAction, 1);
}

function onClose(ctx) {
    ctx.setParam("approach_below_pad", "0");
    ctx.setParam("approach_above_pad", "0");
}

function onExit(ctx) {
    onClose(ctx);
}

globalThis.canvas_overlays = {
    follower_mod: { onOpen, onMidi, draw, onClose, onExit }
};
'''


def main() -> None:
    module = json.loads(MODULE.read_text())
    module["name"] = "Harmony Bus 0.2.74"
    module["abbrev"] = "HB274"
    module["version"] = "0.2.74"
    module["description"] = "Harmony Bus v0.2.74 — repaired Touch Perf as-page canvas and touch handlers"

    caps = module["capabilities"]
    hierarchy = caps["ui_hierarchy"]
    hierarchy["levels"]["root"]["name"] = "Harmony Bus 0.2.74"

    params_by_key = {str(param.get("key")): param for param in caps["chain_params"] if param.get("key")}
    canvas_param = params_by_key["follower_mod_canvas"]
    canvas_param.update({
        "name": "Touch Perf",
        "type": "canvas",
        "canvas_script": "canvas.js",
        "canvas_overlay": "follower_mod",
        "as_page": True,
        "show_value": False,
        "extra_keys": ["approach_mode", "approach", "travel_map"],
    })

    panel = hierarchy["levels"]["follower_mod"]
    panel["name"] = "Foll Mod"
    standard_keys = [
        "approach_mode", "approach", "foll_mod_blank3", "foll_mod_blank4",
        "foll_mod_blank5", "approach_reset", "mod_scale_above", "mod_chrom_below",
    ]
    panel["knobs"] = standard_keys
    panel["params"] = [dict(params_by_key[key]) for key in standard_keys] + [dict(canvas_param)]

    MODULE.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP.read_text()
    source = re.sub(
        r'/\* Harmony Bus v[0-9.]+ — Schwung MIDI FX\. \*/\n#define HB_VERSION "[0-9.]+"',
        '/* Harmony Bus v0.2.74 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.74"',
        source,
        count=1,
    )
    DSP.write_text(source)
    CANVAS.write_text(CANVAS_SOURCE)


if __name__ == "__main__":
    main()
