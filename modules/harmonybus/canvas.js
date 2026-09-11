/* HarmonyBus follower modifier performance page.
 *
 * Knob touch assignments while this page is open:
 *   K6 touch: Off/reset
 *   K7 touch: scale approach above
 *   K8 touch: chromatic approach below
 *
 * K1 rotation selects modifier behavior (Next / Held).
 * K2 rotation provides the retained manual Approach selector.
 */

const CC_KNOB_1 = 71;
const CC_KNOB_2 = 72;
const TOUCH_K2 = 1;
const TOUCH_K3 = 2;
const TOUCH_K4 = 3;
const TOUCH_K5 = 4;
const TOUCH_K2 = 1;
const TOUCH_K3 = 2;
const TOUCH_K4 = 3;
const TOUCH_K5 = 4;
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
    /* Re-writing the current mode deliberately clears any armed one-shot state
     * in the DSP, so K6 is a real reset rather than merely setting the manual
     * Approach knob to Off. */
    ctx.setParam("approach_mode", MODES[state.modeIndex]);
    setApproach(ctx, state, 1);
    state.lastAction = "OFF / RESET";
}

function handleTouch(ctx, state, note, down) {
    /* K1 touch is intentionally inert; K1 rotation only selects Next/Held/Off. */
    if (down && note === TOUCH_K2) { ctx.setParam("travel_map", "Direct"); state.lastAction = "DIRECT"; return true; }
    if (down && note === TOUCH_K3) { ctx.setParam("travel_map", "Relative"); state.lastAction = "RELATIVE"; return true; }
    if (down && note === TOUCH_K4) { ctx.setParam("travel_map", "Closest"); state.lastAction = "CLOSEST"; return true; }
    if (down && note === TOUCH_K5) { ctx.setParam("travel_map", "Closest Split"); state.lastAction = "CLOSEST SPLIT"; return true; }
    if (!down && (note === TOUCH_K2 || note === TOUCH_K3 || note === TOUCH_K4 || note === TOUCH_K5)) return true;
    /* K1 touch is intentionally inert; K1 rotation only selects Next/Held/Off. */
    if (down && note === TOUCH_K2) { ctx.setParam("travel_map", "Direct"); state.lastAction = "DIRECT"; return true; }
    if (down && note === TOUCH_K3) { ctx.setParam("travel_map", "Relative"); state.lastAction = "RELATIVE"; return true; }
    if (down && note === TOUCH_K4) { ctx.setParam("travel_map", "Closest"); state.lastAction = "CLOSEST"; return true; }
    if (down && note === TOUCH_K5) { ctx.setParam("travel_map", "Closest Split"); state.lastAction = "CLOSEST SPLIT"; return true; }
    if (!down && (note === TOUCH_K2 || note === TOUCH_K3 || note === TOUCH_K4 || note === TOUCH_K5)) return true;
    /* K1 touch is intentionally inert; K1 rotation only selects Next/Held/Off. */
    if (down && note === TOUCH_K2) { ctx.setParam("travel_map", "Direct"); state.lastAction = "DIRECT"; return true; }
    if (down && note === TOUCH_K3) { ctx.setParam("travel_map", "Relative"); state.lastAction = "RELATIVE"; return true; }
    if (down && note === TOUCH_K4) { ctx.setParam("travel_map", "Closest"); state.lastAction = "CLOSEST"; return true; }
    if (down && note === TOUCH_K5) { ctx.setParam("travel_map", "Closest Split"); state.lastAction = "CLOSEST SPLIT"; return true; }
    if (!down && (note === TOUCH_K2 || note === TOUCH_K3 || note === TOUCH_K4 || note === TOUCH_K5)) return true;
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

    /* Capacitive knob touches are MIDI note events 0..7. Schwung represents
     * release either as note-off or note-on with a non-pressed velocity. */
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
    ctx.print(2, 2, "FOLLOWER MOD", 1);
    ctx.print(2, 14, `K1 MODE: ${MODES[ctx.state.modeIndex]}`, 1);
    ctx.print(2, 24, `K2 MANUAL: ${APPROACHES[ctx.state.approachIndex]}`, 1);
    ctx.print(2, 34, "K2 DIRECT  K3 REL  K4 CLOSE", 1);
    ctx.print(2, 42, "K5 SPLIT   K6 RESET", 1);
    ctx.print(2, 50, "K7 SCALE+  K8 CHROM-", 1);
    ctx.print(2, 58, `LAST: ${ctx.state.lastAction}`, 1);
}

function onClose(ctx) {
    ctx.setParam("approach_below_pad", "0");
    ctx.setParam("approach_above_pad", "0");
}

function onExit(ctx) {
    onClose(ctx);
}

globalThis.canvas_overlays = {
    follower_mod: {
        onOpen,
        onMidi,
        draw,
        onClose,
        onExit
    }
};
