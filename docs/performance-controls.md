# HarmonyBus 0.2.296

Adds Drum Fill to operation assignments when hosted by the matching Movy release. The assignment stores its MIDI drum note in the existing operation state; it never changes the melody's mapping or runs as an automatic pitched-note modifier. Movy owns the one-shot trigger and ordinary recording behavior.

Adds a bounded harmony-status snapshot for Movy's Harmony & Pads page: Current, Effective, Next, model readiness, pad colors, lookahead timing, pulse rate, and next-tone pulse. Reading the page does not advance harmony, trigger notes, or change pad colors.

Current pad colors now follow the accepted conductor chord. Previously a learned timeline could replace that color before the audible chord changed, causing apparent disagreement until the clip loop restarted. Effective and lookahead colors retain their separate rendering and prediction meanings.

Older hosts keep their existing operation choices. Saved operation IDs remain unchanged; Drum Fill appends ID 66.
