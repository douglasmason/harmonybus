#!/usr/bin/env python3
"""Add an isolated as_page canvas diagnostic and bump HarmonyBus to v0.2.78."""
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
    module["name"] = "Harmony Bus 0.2.78"
    module["abbrev"] = "HB278"
    module["version"] = "0.2.78"
    module["description"] = "Harmony Bus v0.2.78 — adds isolated Schwung as_page/touch UI diagnostic"

    levels = module["capabilities"]["ui_hierarchy"]["levels"]
    levels["root"]["name"] = "Harmony Bus 0.2.78"

    # Add a root navigation entry immediately after Foll Mod, once.
    root_params = levels["root"]["params"]
    root_params[:] = [p for p in root_params if not (isinstance(p, dict) and p.get("level") == "ui_test")]
    insert_at = next(
        (index + 1 for index, entry in enumerate(root_params)
         if isinstance(entry, dict) and entry.get("level") == "follower_mod"),
        len(root_params),
    )
    root_params.insert(insert_at, {"level": "ui_test", "label": "UI Test"})

    # Deliberately reuse real, harmless follower-map params. On a working
    # as_page host these three remain ordinary knobs while canvas.js owns the
    # page body. On an old host ui_test_canvas appears as a one-cell page.
    levels["ui_test"] = {
        "name": "UI Test",
        "params": [
            {"key": "content_map"},
            {"key": "travel_map"},
            {"key": "split_map"},
            {"key": "ui_test_canvas"},
        ],
        "knobs": ["content_map", "travel_map", "split_map"],
    }

    chain_params = module["capabilities"]["chain_params"]
    chain_params[:] = [p for p in chain_params if p.get("key") != "ui_test_canvas"]
    chain_params.append({
        "key": "ui_test_canvas",
        "name": "UI Test",
        "type": "canvas",
        "canvas_script": "canvas.js",
        "canvas_overlay": "ui_test",
        "as_page": True,
    })
    MODULE.write_text(json.dumps(module, indent=2) + "\n")

    source = DSP.read_text()
    source = re.sub(
        r'/\* Harmony Bus v[0-9.]+ — Schwung MIDI FX\. \*/\n#define HB_VERSION "[0-9.]+"',
        '/* Harmony Bus v0.2.78 — Schwung MIDI FX. */\n#define HB_VERSION "0.2.78"',
        source,
        count=1,
    )
    DSP.write_text(source)

    canvas = CANVAS.read_text()
    marker = "function uiTestOpen(ctx)"
    if marker not in canvas:
        diagnostic = r'''

/* Isolated host capability diagnostic for Schwung as_page canvases. */
function uiTestOpen(ctx) {
    ctx.state.lastTouch = "NONE";
    ctx.state.touchDown = false;
}

function uiTestMidi(ctx, payload) {
    const data = payload && (payload.data || payload.midi || payload.message || payload);
    if (!data || data.length < 3) return;
    const status = data[0] & 0xF0;
    if (status !== 0x90 && status !== 0x80) return;
    const number = data[1] | 0;
    const velocity = data[2] | 0;
    const down = status === 0x90 && velocity >= 64;
    if (number >= 0 && number < 8) {
        ctx.state.lastTouch = `K${number + 1}`;
        ctx.state.touchDown = down;
    }
}

function uiTestDraw(ctx) {
    ctx.clear();
    const content = readString(ctx, "content_map", "?");
    const travel = readString(ctx, "travel_map", "?");
    const split = readString(ctx, "split_map", "?");
    const touch = ctx.state.lastTouch || "NONE";
    const edge = ctx.state.touchDown ? "DOWN" : "UP";
    ctx.print(2, 3, "AS_PAGE UI TEST", 1);
    ctx.print(2, 15, `K1 Content ${content}`, 1);
    ctx.print(2, 27, `K2 Travel ${travel}`, 1);
    ctx.print(2, 39, `K3 Split ${split}`, 1);
    ctx.print(2, 52, `Touch ${touch} ${edge}`, 1);
}
'''
        canvas = canvas.replace("\nglobalThis.canvas_overlays = {", diagnostic + "\nglobalThis.canvas_overlays = {")

    canvas = re.sub(
        r'globalThis\.canvas_overlays = \{\s*follower_mod: \{ onOpen, onMidi, draw, onClose, onExit \}(?:,\s*ui_test: \{[^}]+\})?\s*\};',
        'globalThis.canvas_overlays = {\n    follower_mod: { onOpen, onMidi, draw, onClose, onExit },\n    ui_test: { onOpen: uiTestOpen, onMidi: uiTestMidi, draw: uiTestDraw }\n};',
        canvas,
        count=1,
        flags=re.S,
    )
    if "ui_test:" not in canvas:
        raise RuntimeError("failed to register ui_test canvas overlay")
    CANVAS.write_text(canvas)


if __name__ == "__main__":
    main()
