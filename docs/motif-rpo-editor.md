# Motif Relative Operation editor

HarmonyBus 0.2.310 and Movy 0.34.1-hbclean.284 let an authored motif step use any supported Relative Pitch Operation (RPO): LT, Upper Dim, Connector Below/Above, parent secondaries, dominant approaches, substitutions, Smart Below, or Smart Above.

Open a Play Motif lane's editor, begin editing, select a note step, and turn **Relative Operation** in **Motif Tools**. A choice edits the selected step and becomes the choice for newly entered notes. On a rest, tie, or empty position it only changes the next-entry choice. Finish with Done. Undo restores the previous draft.

**Captured** restores the original event exactly, even after saving and reopening the motif. The header identifies a representable capture, such as `1 Captured: LT`; a composite or cadence retains its broader identity. Authored operations show their catalog short label. Timing, velocity, polyphony, transposition relation, voicing settings, and unrelated recorded actions remain intact.

LT moves below and supplies diminished harmony. Upper Dim moves above and supplies diminished harmony. Connector Below/Above retain the configured Connector Harmony policy; Tritone Sub retains its dominant recipe. Smart Below/Above choose an entire LT/Upper Dim recipe in the destination collection. The legacy Scale Above operation remains readable in captures; new choices use Secondary II, as in the approach menus.

Fixed clip export resolves Smart choices when exported; Adaptive keeps them for playback. Both modes export explicitly chosen fixed operations as those fixed recipes. Polyphonic fixed operations remain one simultaneous event. A key change now gives raw conductor secondary operations the destination pitch collection, matching chord and motif playback.

## Implementation

The existing `data/operations.json` generates the motif menu, native token mapping, manifest choices, and Movy presentation order. Saved `mf3` events use their existing 16-bit override field for any catalog operation; `mf1`, `mf2`, and existing Smart `mf3` libraries remain readable. An older HB that does not recognize a new fixed override rejects that library atomically. Numeric operation IDs and recorded clip encodings are unchanged.

`src/motif_operations.h` applies a complete operation only to a rendering/export copy. It clears conflicting relative lanes and source intent while preserving unrelated actions and the original saved capture. Native playback and clip export share this helper; captured labels use the same catalog. Movy displays native status through its existing polling path and does not resolve harmony.

## Verification

Native regressions exercise every selectable recipe, Captured restoration, Undo, invalid/no-op edits, note/rest entry, polyphonic save/reload, invalid-ID atomic restore, distinct diminished/major/dominant masks, and matching native versus Fixed/Adaptive playback for conductor/follower, raw/chord, and unchanged/changed keys. Browser/controller tests cover menu compatibility and native captured labels. Full native, browser, and production Movy–Schwung–HB audio checks run before release.

Physical hardware audition remains outstanding.
