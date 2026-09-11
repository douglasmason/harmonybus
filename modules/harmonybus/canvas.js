/* HarmonyBus Touch Perf page. */
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

globalThis.canvas_overlays = {
    follower_mod: { onOpen, onMidi, draw, onClose, onExit },
    ui_test: { onOpen: uiTestOpen, onMidi: uiTestMidi, draw: uiTestDraw }
};
