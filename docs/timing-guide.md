# HarmonyBus timing guide

## Which time determines the rendered note?

**HarmonyBus 0.2.169 / Movy 0.34.1-hbclean.77.** Playback time and harmony-selection time are separate. With locked lookahead, harmonic-buffer notes play immediately using the upcoming harmony. Explicit Quant Grid can still delay playback. During learning, choose a release time and map using the effective harmony at release. The diagrams below use Anti Buffer = 0 ms, 120 BPM and 4/4; the separate anti-buffer section describes the new default 25 ms guard. Times are musical targets, subject to sequencer and audio callback resolution.

![Conductor harmony, effective harmony, capture window and follower release on a shared time axis](timing/overview.svg)

**Example:** the conductor changes from C to D at 2000 ms. A locked model with 1/8-note lookahead makes D effective at 1750 ms. A keypress at 1600 ms falls inside the 350 ms pre-boundary window and waits until 1750 ms. This wait is caused by the eighth-note Quant Grid. With Quant Grid Off, the same keypress plays D immediately at 1600 ms, looking up the harmony at 1750 ms.

**Defaults and scope:** Follower Buffer is per track, initially **1/16 note** (124 ms capture at 120 BPM), following tempo. The early-lookahead diagrams explicitly use a 350 ms example buffer to illustrate wider capture windows. Changing it affects that follower only. Lookahead is per track and defaults to **Off**. Chord Grid and Quant Grid are global. Musical buffer choices run from 1/64 through 4 Bars; millisecond choices are 0, 25, 50, then 100 to 1000 ms in 50 ms steps.

**Saved sets:** saved follower-buffer values restore per track. Global operation configuration and grids use the first restored shared settings, so later track restores cannot overwrite them. Movy hbclean.68 stores new sets and preferences outside its replaceable module folder, under `/data/UserData/movy/`; older module-local data is not imported.

## Follower Buffer chooses among boundaries

While lookahead is off or learning, the buffer is a pre-boundary capture window, not a fixed delay. Candidate release points come from the chord schedule and the follower's Quant Grid. **The earliest eligible boundary wins.** A boundary exactly at arrival is eligible and does not defer the note to the next point.

**Musical-window margin:** division-based buffers subtract 1 ms, excluding their nominal leading edge. This applies to every schedule. Grid targets and millisecond buffers stay exact. Already-on-grid notes stay there.

![Chord-only capture, an earlier competing quant boundary and immediate release outside the window](timing/buffer.svg)

**A: chord schedule only.** Chord Grid = 1 Bar, Anticipation = On Grid, Quant Grid = Off, Lookahead = Off. A keypress at 1700 ms waits 300 ms for the 2000 ms chord boundary, then maps using D.

**B: add Quant Grid = 1/8.** The same keypress waits only 50 ms for 1750 ms. That earlier point wins, so the note still maps using C. A finer Quant Grid does not guarantee alignment to the next chord.

**C: outside the window.** With Quant Grid Off, 1500 ms is outside the 350 ms window before 2000 ms; the note plays without intentional musical delay. With no enabled grids and no usable active lookahead schedule, the buffer causes no grid capture.

## Anticipation and lookahead do different jobs

The examples use C to D at 2000 ms, a keypress at 1600 ms, a 350 ms buffer, Chord Grid = 1 Bar and Quant Grid = Off.

![Anticipation changes capture timing, lookahead changes effective harmony, and the offsets do not add](timing/lookahead.svg)

**Anticipation only: 1/8 Early.** The periodic chord capture boundary moves to 1750 ms, but the effective harmony is still C. Anticipation does not predict a future chord.

**Lookahead only: 1/8.** A usable learned schedule makes D effective at 1750 ms and supplies that shifted boundary for capture. Inside the capture window, the follower plays immediately using D; it does not wait for 1750 ms. The chosen recognized harmony persists until the next shifted transition.

**Both enabled:** lookahead supplies the chord schedule while prediction is usable; Chord Grid and Anticipation do not add another offset. Quant Grid remains an independent playback-timing control. The harmonic capture window selects the upcoming harmony without adding playback delay. While the model is unavailable, ordinary Chord Grid / Anticipation is the fallback.

Only contributing conductor clips enter the prediction cycle. A 3-bar and a 4-bar conductor jointly repeat after 12 bars, including their launch phases and effective playback speeds. Editing content or changing contributing clips invalidates the model; unchanged loops retain it. Non-repeating conditional clips and unsupported cycle sizes fall back to observed harmony.

## Negative lookahead moves the buffer window too

**Lookahead = -1/4, Follower Buffer = 1/16, Quant Grid = Off.** The conductor changes C to D at the bar line (2000 ms). The learned effective harmony changes at 2500 ms, one quarter note later. This shifts harmony selection; it does not postpone the conductor MIDI.

![Negative lookahead shifts harmony to one quarter note after the bar line, with the buffer immediately before that shifted boundary](timing/negative-lookahead.svg)

The capture window is **2376 to 2500 ms**: the nominal 2375 ms leading edge is shortened by 1 ms. A note arriving at 2300 ms plays with C without intentional delay. A note arriving at 2400 ms plays immediately using D from the 2500 ms boundary. A note arriving exactly at 2500 ms plays with D immediately. Quant Grid, if enabled, still delays to its eligible grid point; harmony selection uses the later of that playback time and the captured harmonic boundary.

Positive values advance each learned harmony transition; negative values postpone it. Choices are 1/32, 1/16, 1/8, 1/4, 3/8, 1/2, 3/4, 1 Bar, 1.5 Bars, 2 Bars and 3 Bars in either direction, plus Off. In 4/4, 3/8 is 1.5 beats, 3/4 is 3 beats, 1.5 Bars is 6 beats, 2 Bars is 8 beats, and 3 Bars is 12 beats. Positive values bring the next harmony forward; negative values retain the previous harmony longer for late phrasing. The same signed shift applies around loop wrap. The Shift display reads Early, Late or Live according to the effective schedule.

Both directions require a usable learned model. During learning or after invalidation, HB uses observed harmony and the ordinary Chord Grid / Anticipation capture schedule. Lookahead remains Off by default.

## At release: conductors first, followers second

This example uses Chord Grid = 1 Bar, Quant Grid = Off, Lookahead = Off, buffer = 350 ms and held-note harmonic retrigger = Off.

![Raw note-on and note-off shifted equally, with conductor harmony resolved before rendered note-on](timing/note-off.svg)

At the boundary, Movy delivers the complete conductor MIDI batch, updates the shared harmony, and then releases and maps due follower notes before chain audio rendering. Track index and local audio mute do not change this order. A new voicing excludes already released conductor notes. Recognized two-note voicings in the completed Movy batch also commit before due followers; a lone unresolved pitch keeps the last recognized harmony.

**Articulation survives the delay:** the raw note lasts 125 ms, from 1750 to 1875 ms. Delaying its note-on by 250 ms also delays its note-off by 250 ms, producing 2000 to 2125 ms. Releasing the key before its queued note-on does not cancel the note.

Note-off uses the pitch actually emitted by note-on, even if harmony changes again before release. Harmony persistence keeps the last recognized harmonic reference through gaps; it does not extend the conductor's MIDI notes or keep synthesizer keys held.

## Quantize + Fill Gaps edits the clip

Open **Clip Params with Shift + Step 3**. **Knob 5: GRID** selects 1/16, 1/8, 1/4, 1/2 or 1 Bar. **Knob 6: EDIT**, or the main wheel, selects Fill Gaps or Quantize + Fill. **Press the main wheel to apply.** One Undo restores the complete edit. The grid starts at 1/8.

![Recorded chord groups, snapped starts and filled note lengths on a shared time axis](timing/clip-edit.svg)

**Fill Gaps** preserves note starts and sets each onset group's lengths to reach the next distinct playback onset. It accounts for the clip's current quantization and scaled swing. The last group ends at the loop end. Existing overlaps are trimmed to that next onset; this is a legato-style length edit, not an extend-only operation.

**Quantize + Fill** first snaps stored starts to the nearest selected straight grid, updating their step anchors, then performs Fill Gaps using the resulting playback times. The existing clip quantization percentage remains unchanged; snapped notes already sit on their anchors. Swing remains active. Starts near the loop end are kept on the last in-window grid point rather than wrapped onto the first chord.

The action edits the **selected melodic clip's current loop**; notes outside it stay unchanged. It is unavailable while recording and on drum tracks. Automation and trig conditions stay at their existing step positions. Notes already sounding keep their scheduled note-offs; edited gates apply to subsequent note-ons. Later changes to swing, quantization or loop bounds can reopen gaps; reapply Fill Gaps when needed.

The clip-edit grid is distinct from HB Quant Grid: a clip edit can move starts earlier or later, while the live follower buffer delays only while learning or when explicit Quant Grid captures the note. This action does not add synth glide; portamento and envelope legato remain instrument controls.

## Auto Chord: root, inversion and register

Open **Auto Chord** in the HB menu on a conductor or follower. Chord Mode defaults to Off. **Scale Degree** treats the played note as the literal root and builds the selected Chord Form upward from the parent scale selected by HB's Foll Root / Follower Scale controls. A detected chord alone does not uniquely identify a parent scale; select one explicitly when needed. A chromatic played root stays unchanged; Chromatic Quality chooses its family, with Scale retaining the original scale-stacking behavior. Global transpose moves the completed voicing as a unit.

**Conductor Chord** uses the recognized effective conductor chord, including its quality and implied tones. Chord Form Auto preserves it; other forms select degrees while retaining recognized third/fifth/seventh alterations and taking added extensions from the parent scale. The played key supplies the desired bass pitch and register. Auto inversion snaps to the nearest chord-tone bass (ties downward); exact chord tones select their inversions directly. With Cmaj7, C3 produces C3-E3-G3-B3, E3 produces E3-G3-B3-C4, and E4 produces E4-G4-B4-C5.

| Control | Choices and effect |
| --- | --- |
| Inversion | Auto, Root, First through Sixth. Auto means root position in Scale Degree and bass-from-key in Conductor Chord. Unavailable inversions wrap by chord size. |
| Close | All chord tones within an octave above the chosen bass. |
| Root + Fifth Low | Keep the chosen bass, root and chord fifth low; raise remaining tones an octave. Uses the chord's actual fifth, including altered fifths. |
| Alternate Up | Starting with the closed inversion, raise every other upper voice one octave, then sort by pitch. Preserve the chosen bass. |
| Shell | Retain root, third and seventh when available; use the sixth if there is no seventh. Keep the chosen bass even if it is an omitted extension. A power shell retains root and fifth. |

Explicit Scale Degree inversions place the selected bass degree below the played root; the played note still determines the chord identity. Explicit Conductor Chord inversions choose the nearest occurrence of the selected bass degree. At MIDI range edges, shift the whole voicing by octaves instead of clipping or merging its tones. First-inversion Cmaj7 with Root + Fifth Low is E3-G3-C4-B4.

Auto-chords are constructed **at playback**, using the per-note harmony selection. Locked lookahead can select the upcoming harmony immediately inside the harmonic capture window; learning uses the effective harmony at delayed release. Their voicing then stays fixed for that gesture, including an ongoing arpeggio. No recognized harmony means no auto-chord until harmony becomes available and a new gesture starts. Chord Mode Off plus Together retains ordinary follower mapping. When either generator is active, it owns pitch construction: Content, Travel, Approach and held-note harmonic retrigger do not remap its output. Arp with Chord Mode Off uses literal input pitches plus global transpose.

## Chord forms and shell examples

Chord Form chooses the degree set. With Quality set to Auto, the conductor chord and active parent scale determine alterations; these are scale-compatible forms, not fixed major/minor interval presets. For example, Sixth in natural minor uses the scale's lowered sixth. Use Melodic Minor when you want the raised sixth of that collection. Auto means Triad in Scale Degree, and the recognized chord in Conductor Chord.

| Chord Form | Degrees before inversion and voicing |
| --- | --- |
| Power / Triad | 1-5 / 1-3-5. Power uses the harmonic fifth, including an altered fifth when applicable. |
| Seventh | 1-3-5-7: a four-note seventh chord (quadrad). |
| Ninth / Add9 | 1-3-5-7-9 / 1-3-5-9. Add9 does not add a seventh. |
| Sixth / 6/9 | 1-3-5-6 / 1-3-5-6-9. Neither adds a seventh. |
| Eleventh / Thirteenth | 1-3-5-7-9-11 / 1-3-5-7-9-11-13. |
| Sus2 / Sus4 | 1-2-5 / 1-4-5, replacing the third. |

Close folds extensions into the selected one-octave position. Cmaj9 in root-position Close is C-D-E-G-B; Root + Fifth Low places C-G below D-E-B in the upper octave. Inversion names follow harmonic degree order, so First still selects the third, not the folded ninth. Fourth can select the ninth of a ninth chord. The selected form defines which bass tones are available.

Shell is a separate voicing reduction. Root-position Cmaj9 becomes C-E-B; C6 becomes C-E-A; C minor seventh becomes C-Eb-Bb. A triad shell is root and third. Choosing D as the bass of a Cmaj9 shell preserves that bass and produces D-E-B-C above it. Shared pitches from overlapping gestures have shared lifetime: a tone releases only after its last owner releases it.

The form is built first, inversion selects the bass second, voicing distributes or removes voices third, and arpeggiation/strum schedules the resulting pitches last. No new chord tones are inferred from the arpeggiator output.

## Arpeggiation, latch and trigger strums

Open **Arp / Strum** on a conductor or follower. Playback defaults to Together, Hold to Momentary, Rate to 1/16, Gate to 50%, and Strum Spread to 0 ms. All chord/player controls are per instance.

![Chord generation at release followed by a trigger strum, with cancellation at the source release](timing/chord-player.svg)

**Together** starts the chord simultaneously. **Repeat Arp** cycles through unique held/latched pitches. Rate spans 1/64 through 4 Bars; Gate selects 25%, 50%, 75% or 90% of that interval. **Once** starts each voice once, sustains until its owning source note-off, and cancels unplayed voices on release.

**Strum Spread** is the total first-to-last span for Once: 0, 25, 50, then 100-1000 ms in 50 ms steps, plus 1/64 through 4 Bars. Four voices at 150 ms start at 0/50/100/150 ms. Together and Repeat Arp ignore Spread. Raw notes released in one callback form one group; later keys form new groups.

**Order:** Up, Down, Up-Down, Played or Random. Up-Down repeats without doubling the endpoints; in Once it makes one ascending pass so each voice starts only once. Played follows source gesture arrival order, with generated chord tones low-to-high inside each gesture. Random chooses each repeated pitch independently, or shuffles a Once group.

**Momentary** follows source releases. **Latch** holds the pool; after all keys release, the next gesture replaces it. Overlapping held keys add pitches. Clear Notes, Stop, role/routing changes and player-control changes release the voices; edits require a fresh gesture. Existing stored clip notes are unchanged; currently generated synth voices are released.

In Free mode, Repeat Arp starts at note-down on both followers and conductors and bypasses Follower Buffer. Auto waits for the next arp-rate division. Musical intervals follow tempo; ms spreads use audio time. Stopped playback uses a free-running tempo clock; rewind clears the gesture. Buffered non-arp note-offs retain their inherited delay. Capacity is 16 source keys with up to 12 tones each; excess keys are ignored. Physical device timing needs verification.

## Dominant scale substitution

**Dominant Scale** appears in Foll Root and Auto Chord. It is global and defaults to Off. It changes the output pitch collection used by ordinary follower scale/extension mapping and by auto-chord construction. It does not change the source-root policy, the input key's degree label, or the conductor's recognized chord.

The trigger is a major-third V chord without a major seventh, or a diminished leading-tone chord rooted a semitone below the configured tonic. In C minor, G major/G7 and B diminished qualify. G minor, Gmaj7, Bb major and Bb7 do not. This first implementation recognizes V and raised-vii function relative to the configured tonic; it does not infer secondary dominants or backdoor cadences.

| Setting | Output collection while the trigger is active |
| --- | --- |
| Off | Use the existing base-scale behavior. |
| Harmonic Minor | Harmonic minor on the configured tonic. Over G7 in C: G-Ab-B-C-D-Eb-F, giving b9 and b13. |
| Melodic Minor | Ascending melodic minor on the configured tonic. Over G7 in C: G-A-B-C-D-Eb-F, giving natural 9 and b13. |
| Altered V | Melodic minor a semitone above V. Over G7: G-Ab-Bb-B-Db-Eb-F. For a leading-tone diminished chord, use tonic harmonic minor. |

G altered is the seventh mode of Ab melodic minor, not a mode of C melodic minor. Its b9, #9, b5/#11 and b13 provide altered tensions; see [Jens Larsen's altered-scale lesson](https://jenslarsen.nl/melodic-minor-altered-scale/). The other two options are tonic-rooted collections. There is no single tonic-rooted C melodic-minor mode that describes that same G-altered collection.

Recognized chord tones remain legal in ordinary Scale mapping; Conductor Chord forms preserve the detected third/fifth/seventh. Therefore an unaltered G7 can retain D even under Altered V. Requested ninths use b9, elevenths use #11 and thirteenths use b13; #9 is available in the scale collection for nearest-note mapping. Root-derived Scale Degree chords use the selected collection directly.

The substitution follows the effective harmony, including positive or negative lookahead, and stops applying when the trigger disappears. Buffered inputs use the collection selected for their harmony target, including immediate predicted renders. Already-generated chord/arp gestures retain their original pitches until a new gesture starts; changing Dominant Scale explicitly clears currently sounding follower notes.

## Recommended diagnostics and maintenance

| Control or signal | Scope and purpose |
| --- | --- |
| Follower Buffer | Global width of pre-boundary capture; 1/16 note for new settings (tempo-relative). |
| Quant Grid | Per-follower periodic release points, independent of the clip-edit grid. |
| Chord Grid / Anticipation | Global periodic chord capture schedule when prediction is unavailable or Off. |
| Lookahead | Global signed learned-harmony shift (early or late); Off by default. |
| Harmony persistence | Normal behavior: retain the last recognized harmony through gaps. |
| Timing diagnostic | Recommended future addition: arrival, chosen target/reason, actual delay and harmony at release. |

**Fixed in 0.2.104:** recognized two-note Movy voicings no longer wait for live grouping/confirmation while a due follower maps to the previous harmony. Tests cover both callback orders and paired note-offs.

**Fixed in 0.2.102:** a buffer wider than a grid interval no longer pushes an exactly aligned note to the following grid point. This applies to regular grids and shifted learned boundaries. Conductor-first processing still applies to notes released immediately on the boundary.

**Keep visible:** combined conductor-cycle position, learning/locked model status and effective harmony. These make it possible to distinguish a wrong boundary from a wrong chord. Chord grouping and release grace are analysis controls, not additional follower-delay controls. The proposed timing diagnostic above is not yet implemented.

**Canonical files:** this Markdown and the editable SVGs in `docs/timing/`. The PDF is generated from these files; do not edit a PDF copy as documentation source. Run `python3 scripts/build_timing_guide.py` after installing `scripts/docs-requirements.txt`. CI builds and attaches the PDF to the HB release. Runtime behavior is covered by the native buffer, boundary, learning and Movy clip-edit tests; documentation still needs review when semantics change.

**Implementation:** [boundary selection](../src/follower_timing.h), [harmony and follower release](../modules/harmonybus/dsp/harmonybus.c), and [Movy integration](https://github.com/douglasmason/harmonybus-movy). The tests use host callbacks; physical Move behavior still needs device verification.


## Arpeggio phase

Arp Start controls timing; Note Phase controls which note occupies each step. The **Arp / Strum** panel has exactly eight knobs: Playback, Hold, Order, Rate, Gate, Strum, Arp Start, and Note Phase. Clear Notes has been removed from the panel.

| Start mode | Timing for a 1/16 arp hit at beat 0.10 |
| --- | --- |
| Auto (default) | First note at 0.25, then 0.50, 0.75, 1.00. |
| Free | First note at 0.10, then 0.35, 0.60, 0.85. The entire pattern retains the first-hit offset. |
| 1st Note Free | First note at 0.10, then 0.25, 0.50, 0.75. Only the opening note is off-grid. |

Additional held keys join the pool without resetting its clock. Late callbacks skip expired steps while preserving the original clock anchor. Retrigger Held on a harmony change deliberately restarts according to the selected start mode. Auto starts strictly on the next division, including when the key arrives exactly on a division.

**Note Phase** rotates the ordered pattern by whole arp steps. Zero retains the normal opening note; +1 puts that note one step later, and -1 advances the pattern one step. For an ascending C-E-G cycle in Auto, zero starts C-E-G, +1 starts G-C-E, and -1 starts E-G-C. This moves the note pattern, not the beat grid. Auto uses the actual root as its zero-phase starting note even in an inverted voicing; Free and 1st Note Free use the selected order's normal first note. Random order has no repeating note cycle to phase-shift predictably after its opening note.

The effective limit is one Chord Grid interval in either direction, falling back to one 4/4 bar when Chord Grid is Free (no grid). With that one-bar fallback: 1/16 gives -16 to +16, 1/8 gives -8 to +8, and 1/4 gives -4 to +4. The numeric control has a static maximum span of -256 to +256; HB clamps edits to the effective range and clamps the saved offset when Arp Rate or Chord Grid changes. Rates longer than the selected interval allow only zero, since no whole arp step fits inside it. Offset is zero by default.

Repeat Arp bypasses Follower Buffer on presses and releases. Releasing all momentary keys before Auto starts cancels that start; latch retains it. Together and Trigger Strum are unaffected by these phase controls. Explicit saved Free and Auto choices retain their meaning; presets without a phase setting use the new Auto default. Start mode and Note Phase save with the instance.


## Conductor recording and chord quality

Auto Chord and Arp / Strum work on both conductor and follower tracks. A conductor publishes the complete generated harmonic gesture to the bus while arpeggiating individual voices. Local monitoring and Render To receive the generated output.

With Movy hbclean.31, recording stores the emitted conductor chord voices and their timing. Those voices are marked as already rendered, so playback does not generate a chord for every saved note. Changing chord form afterward does not expand previously recorded chords. Older, unmarked notes retain ordinary processing. Save and reload preserve this distinction.

| Control | Behavior |
| --- | --- |
| Quality | Auto, Major, Minor, Dim, Aug, Maj7, Dom7, Min7, Half Dim7 or Dim7. Overrides apply in both chord modes; Form still chooses the included degrees. |
| Chromatic Quality | In Scale Degree mode, out-of-scale keys use Scale, Major / Maj7, Major / Dom7 or Dim / Dim7. An explicit Quality overrides this choice. |
| Chromatic Below (Foll Mod) | The regular modifier also applies to follower chord gestures: lower the input by one semitone and use diminished quality. The next unmodified gesture returns to normal. |

The experimental UI Test page is no longer exposed. Existing Foll Mod controls remain available. A broader momentary knob-touch interface has not been added in this release.

In Movy hbclean.31, touching a knob shows its full parameter name and current value in a highlighted header. Releasing it restores the page header; when several knobs are held, releasing the most recent returns to the previous held knob. Touching alone does not edit a setting or arm a modifier.

Generated chords are classified from the complete generated note set. The previous chord no longer biases a new generated triad toward a shared major root: F-Dm-G-Em remains F-Dm-G-Em, including inversions, rather than F-F6-G-G6. Raw played voicings retain their contextual interpretation.

Position shows the current position within the combined conductor cycle, in bars (or beats for shorter cycles). Loop Length shows its total duration separately. Position deliberately contains no slash: the host treats slash-separated string values as paths and would display only the final segment.

## Split groups and Content labels

Follower Content choices now omit the “In” prefix. Saved indices and older text values remain compatible.

Closest Split with **135 / 2467** keeps degree 7 in the 2467 group. Previously, Chord content over a triad could leave that group empty and fall back to all chord tones, sending B to C over C major. Explicit 135 / 2467 and 1357 / 246 groups now remain intact: Content narrows the group when possible; otherwise the full group is available. Other split modes retain their existing behavior.

## Closest Split Chromatic and Master Transpose

**Follower Travel: Closest Split Chromatic** keeps the normal Closest Split rendering for in-scale inputs. An out-of-scale input instead approaches the rendering of the next higher in-scale input: find that input, run its normal split mapping in the same register, then subtract one semitone from the output. The existing Split selection still chooses the groups.

For example, with C-major reference, C-major harmony, Scale content and 135 / 2467, C-sharp approaches the rendering of D: it plays D-flat, then D resolves it. If the split mapper sends that D input to a different pitch, the C-sharp pad plays one semitone below that actual output instead. The same rule works across octaves. This guarantee assumes harmony and mapping settings remain unchanged between the two notes; physical MIDI limits clamp a leading tone below note zero. No higher valid MIDI input means ordinary split fallback.

The chromatic distinction uses the follower reference root and parent scale, not a fixed list of black piano keys. It applies to ordinary follower mapping; Auto Chord and Arp generation continue to own their pitches. Normal Split is unchanged, and existing saved Travel values keep their meanings.

**Global panel: Master Transpose** offers As Played and twelve named destination roots, with both enharmonic names for chromatic roots. Internally, the destination minus the follower reference root gives a semitone offset, choosing the nearest direction (up at a tritone). As Played resets the offset to zero. With F as reference, selecting G sets +2; selecting E sets -1. If the inferred reference is unavailable, the display shows -- and a destination change waits until a reference is available to be selected again.

The existing numeric Global Xpose remains available for octave shifts. Both controls edit the same master offset, saved through the existing state format. The destination display follows that offset and the current reference root. Changing the reference does not pin a previously chosen destination; select the destination again to recalculate.

Changing master transpose releases sounding voices at their previous pitches and applies the new offset to subsequent gestures. Current and learned harmonies transpose together, so a held conductor chord supplies the new harmony immediately without relearning its timing. Rapid knob changes retain pending note-offs.

## Receiver tracks and MIDI routing

**HarmonyBus 0.2.127 / Movy hbclean.32.** On the main panel choose Role: Conductor / Follower / Receiver / Off. Set Role to Receiver, then set Receive Channel (immediately after Render To Ch) to the source's Render To channel. Put an instrument after HB and unmute the destination's local audio. Receivers deliver already-rendered notes without applying chord mode, harmony mapping, quantization, or another render broadcast. Raw pad notes on a Receiver are consumed; use a Conductor or Follower to generate input.

**Fresh Movy sets:** tracks 1–12 retain three conductor/follower quartets with Plaits. Tracks 13, 14, 15 and 16 receive channels 1, 2, 3 and 4 respectively and have no instrument loaded. All local outputs still start muted. Load an instrument and unmute each destination you want to hear. Existing saved sets retain their chains and settings; configure Receiver manually there.

**Compatibility:** original stock Move/Schwung MIDI broadcasts remain intact. Receivers additionally listen inside the hosted HB module. A receiver later in the audio processing order can consume notes in the same block; an earlier receiver consumes them on its next tick. This is audio-block scheduling, not a musical buffer. Private conductor-recording packets are not receiver input.

**Note ownership:** receivers track notes per source. A source's release does not cut another source holding the same pitch. Role or channel changes, source removal and transport stop release owned notes. Receiver queue overflow clears pending input and releases sounding notes.

**Playhead feedback:** Movy polls playing position after 40 ms elapsed as well as its existing tick-count schedule. This avoids waiting eight slow UI ticks when UI work is busy; physical LED response still depends on the next UI tick and needs device confirmation.

Accidentals spelling is on the Global panel and applies across HB instances.

Four-bar timing is available for Quant Grid, Chord Grid, Follower Buffer, Lookahead, Arp Rate and Strum. Lookahead also offers -4 Bars. Four bars correspond to 16 beats (8 seconds at 120 BPM); the musical Follower Buffer retains its 1 ms leading-edge margin. Existing defaults and saved option IDs are preserved.

## Master transpose across live and recorded tracks

HarmonyBus 0.2.127 applies master transpose to rendered conductor playback as well as live and recorded follower input. Render To channels and HB Receivers hear the same final pitches as local monitoring. Follower pads keep their reference-scale roles: playing 1-3-5 continues to follow the detected conductor harmony after transposition.

Recorded conductor voices bypass chord generation, so changing chord mode does not regenerate an existing recording. They still pass through master transpose. New conductor chord recordings store their rendered voicing in the reference key; the current master transpose is applied on playback. Harmony detection uses that same reference basis and applies transpose once. Changing transpose releases old sounding pitches before new notes use the new setting.

Legacy recordings made with nonzero master transpose may already contain that transpose in their saved pitches. Those files do not record the original offset, so HB cannot automatically recover their original reference key. Recordings made at zero transpose need no conversion; any baked offset in an older clip can be corrected using the clip transpose control.

## Held follower chords

With Conductor Chord mode and Retrigger Held On, a changed effective harmony revoices held or latched follower chord gestures using their original input note, register and voicing settings. Old notes are released before the new chord sounds on local and Render To outputs. Repeated observations of the same harmony do not repeatedly retrigger. Arpeggios and strums use the new chord pool and restart according to their phase and timing settings.

Retrigger Held Off preserves the chord until a new input gesture. Turning it On while an old chord remains held catches that chord up to the current harmony. Harmony-driven updates do not consume an armed next-note modifier, and released unlatched input notes are not revived.

A pending quantized follower input does not block harmony updates to an already-playing chord or arp. Conductor MIDI is resolved, due follower input is released, held chords are revoiced, and then the arp emits its next step. Future queued inputs keep their original scheduled onset.

With Chord Mode Off and arp or strum enabled, raw follower notes pass through the ordinary Content/Travel mapper before entering the arp. Retrigger Held On remaps active owners when harmony changes; Off preserves their original interpretation until a new press. Original input keys still own note-offs, and master transpose applies once.

## Repeat Arp and Follower Buffer

Repeat Arp bypasses Follower Buffer for both note-on and note-off input, with Chord Mode Off or enabled. Chord Grid, Anticipation and Quant Grid do not capture these arp inputs. Arp Rate and Phase own playback timing: Free starts on note-down; Auto waits for its own grid phase. A momentary note released before an Auto start can therefore remain silent.

Input still passes through the conductor-first audio queue. At a conductor change, Retrigger Held refreshes the held pitch pool before the next arp note is emitted, including raw-note Relative mode. Lookahead can still select the effective harmony; it does not delay arp input through Follower Buffer.

Ordinary follower notes and Trigger Strum retain their existing buffer behavior. This bypass applies to Repeat Arp.


## Held arps and pressure (0.2.127)

Retrigger Held defaults to On for new instances and prepared Movy tracks. Existing saved Off choices remain Off. Harmony changes replace the held arp mapping before the due hit without restarting Auto or moving the running grid. Repeated pitches emit OFF then ON in the same callback when output capacity allows.

Polyphonic aftertouch sets subsequent Repeat Arp hit velocities for the original held pad and all chord voices it owns. Before the first pressure event, attack velocity is preserved. Pressure is clamped to 1–127 so zero pressure cannot become a MIDI note-off. Pressure does not restart the arp or change an already-sounding note's volume. Harmony revoicing retains pressure; releasing a pad stops pressure updates, including when its gesture is latched. Movy hbclean.39 forwards coalesced pressure once per UI tick using the held pad's original pitch and chain.


Movy hbclean.39 also records note-relative pressure curves during normal follower recording. Playback sends the original follower note and its pressure curve through HB, so arp velocities reproduce the gesture while pitches continue following the current conductor. Curves follow note onset quantization, clip speed, transposition, copies, and loop wraps. Legacy clips have no pressure curves. Retrospective Capture remains note-only.

## Foll Play: harmony-aware phrase transformations

Foll Play changes playback without rewriting clip notes or recorded pressure. Its eight controls are:

| Knob | Control | Behavior / default |
| --- | --- | --- |
| K1 | Tone Rotate | -24 to +24 permitted-tone steps; 0 |
| K2 | Wrap Octave | Keep the result within the source's harmony-rooted octave; Off |
| K3 | Mirror | Reverse tone positions around the root before rotation; Off |
| K4 | Octave | -3 to +3 octaves after the tone transform; 0 |
| K5 | Arp Range | Repeat the finished voicing over 1–4 octaves; 1 Oct |
| K6 | Apply To | Both, Clip or Live; Both |
| K7 | Bypass | Temporarily disable these transformations; Off |
| K8 | Reset | Restore the panel's neutral settings |

For raw follower notes, Follower Content supplies the permitted tones. Closest Split retains the input's assigned group. Closest Split Chromatic first transforms the next diatonic input's resolution, then places the chromatic approach one semitone below it. Manual approaches are applied after the transform. Auto Chord rotates through the generated chord's tones, retaining its chosen quality and extensions. With no valid harmony, harmonic transforms pass notes through.

For a C-major triad, rotation +1 sends C4, E4, G4 to E4, G4, C5. Wrap Octave instead makes the last result C4. Scale content uses the scale's tone sequence; Free content uses chromatic steps. Mirror reverses the tone index before rotation. At MIDI limits, octave adjustment preserves the resulting pitch class.

Voicing and Arp Range do different jobs: voicing constructs the chord and inversion; range repeats that finished set in octaves. Repeat Arp deduplicates overlapping pitches and omits pitches above MIDI 127. Range also works for conductor arps; the harmonic phrase transforms apply to followers. Together and Once playback retain their existing voice count.

Clip/Live distinguishes Movy's playback marker from live input. The marker stays attached to queued notes and held owners, so deferred notes and later revoicing keep the same scope. On hosts without that marker, input is treated as Live. New transform values reach held arps without restarting their phase; sustained non-arp notes follow Retrigger Held. Original source keys still own note-offs and pressure. Existing saved sets load with neutral transformations.

## Performance-touch response

Movy hbclean.40 sends an owned toggle's Off before normal view lookup and automation bookkeeping, including after navigating away from its original page. While a performance toggle is held, module-contract reloads, parameter polling and periodic autosave wait. Forced saves during teardown remain enabled. Parameter refresh resumes after a short 100 ms release quiet period; that period does not delay the Off write.

Foll Play Bypass uses the same touch-On/release-Off behavior as Chrom Below and Scale Above. Reset is a one-shot touch action. Native and controller tests verify event ordering and absence of parameter reads during the held gesture; physical Move response time still requires an on-device check.

## Rapid input into a latched arp

With Repeat Arp and Latch enabled, a new gesture replaces the pitch pool after all input keys have been released. It does not restart the arp clock. The current hit completes its gate, and the next scheduled hit uses the updated pool. Auto, Free and 1st Note Free retain their original timing anchor after startup. Faster input can replace a note before it gets a turn; it must not create missing scheduled hits or extra off-grid attacks. Stop and explicit clearing still silence the arp.

## Harmony pad colors

Movy hbclean.43 with HarmonyBus 0.2.129 provides one shared Pads Global panel inside HB, accessible from any track. Display settings apply to all HB tracks and save with the Set; the old Movy Settings controls are removed. Standard selects Effective highlighting with 1/4-note pulses and pure Track color over the scale background. Current, Effective, Lookahead and Both color pads by their RENDERED pitches, using the active track's follower mapping, chord voicing, modifiers and live Foll Play settings. Each input pitch class is previewed through the effective rendering context once; the resulting pitches are compared with the selected chord. All octaves share the same classification. No notes are sent and one-shot modifiers are not consumed by previewing.

Current tests the rendered pitches against the current conductor chord. Lookahead tests them against the signed lookahead harmony before follower-buffer adjustment, and remains unlit until prediction is ready. Effective tests them against the harmony actually used to render, including predictive follower buffering (bypassed by Repeat Arp). Both overlays Current and Lookahead half a pulse cycle apart. Existing saved display selections retain their meanings and numeric values; Lookahead is appended.

For example, with C as the follower reference, Relative travel and Scale content over G major, input C renders G and lights as a chord tone; input D renders A and does not, even though the input pitch D belongs to G major. With Auto Chord enabled, the generated chord root determines the highlight, independent of added tones and inversion. With Auto Chord off, the single rendered note determines it.

The input-key root always has track color as its background. Other pads whose rendered voices all belong to the effective scale have dim-white backgrounds; chromatic results are dark. Current defaults to cyan and Lookahead/Effective to yellow. Overlay pulses leave the background visible between peaks. Last-played, held and immediate pad-down feedback cannot override this scheme.

Pad Pulse Rate: Off (default), 1/16, 1/8, 1/4, 1/2, 1 Bar, 2 Bars, 4 Bars. Shape: Smooth (default), Triangle, Square. Current, Effective and Lookahead each have eight fixed colors plus Track. Off makes overlays steady, blending shared tones. Move's fixed palette approximates blends in discrete steps. All instances save the same shared display settings; a stale track restore cannot overwrite a live change.

Polling is read-only, at most once per 50 ms, and paused during performance-touch gestures. Pulses follow the master transport when running, or tempo when stopped. Standard uses the same effective-input preview as Effective. Drum and session pads retain their normal display. The six controls are Pad Colors, Pulse Rate, Pulse Shape, Current Color, Effective Color and Lookahead Color. Only the rendering classification varies by track. Stock Schwung can show this panel, but its native pad LEDs require host support; Movy hbclean.43 supplies that integration.

## Operation lanes and performance controls

Sixteen slots transform the rendered output without rewriting source clips. The two shared editing panels and global Steps / Perform switch are described in [Operation lanes](operations.md), including the recording path, momentary controls and triggered enclosures.

## Lookahead anti-buffer (0.2.142)

Next Harm now includes **Lookahead Anti Buffer**, independently adjustable from
Lookahead and Follower Buffer. Positive lookahead starts later by this amount
(default 25 ms), clamped to the actual harmony boundary. A nonzero anti-buffer
also prevents harmonic pre-capture from selecting that harmony before the new
start. As of 0.2.144, enabled nonzero lookahead with a nonzero anti-buffer also
forces the effective **Follower Buffer to 0 ms**, including during learning.
This disables early capture for both harmony and Quant Grid. The displayed
buffer is 0 ms; its configured value is preserved in saved state and returns
when lookahead is off or the anti-buffer is zero. This buffer override also
applies to negative lookahead; the negative harmony offset itself is unchanged.
The earlier timing diagrams describe the **0 ms** compatibility setting. See
[follower paths and timing](follower-paths.md) for examples and diagnostics.

Next Harm places **Follower Buffer** immediately beside **Lookahead Anti Buffer**
(replacing Reset Learn). This is the same per-track buffer control as in Timing,
so either location edits the same saved value and shows the effective value.


## Follower input scale and pad roles (0.2.153)

Follower Scale describes the input keyboard. Explicit scales never borrow accidentals from the current or lookahead chord. Infer resolves a collection around the follower root from the current observed harmony, with Major as the initial/tie preference. Movy mirrors that resolved collection without disabling Infer.

Output collections start from the explicit follower scale and accommodate actual chord tones, preserving the remaining scale degrees. Only Infer selects a parent scale from harmony. Borrowed Scale can select Aeolian, Dorian or Mixolydian b6 for the parallel-minor borrowing family; Dominant Scale remains the higher-priority override. Thus C Phrygian as an input scale cannot introduce E-flat into the third of a C-major output chord. Ordinary Closest Split uses the stable degree bucket for a chromatic input (marked with `*` in the input-role display). Closest Split Chromatic instead maps the next higher in-scale input and approaches its rendered note from one semitone below (marked `-1`). Output roles use ordinal degree labels; the Note column gives the exact rendered pitch.


## Live arp edits (0.2.154)

Rate, gate, order, start phase, note phase, hold mode, playback mode, and strum spread preserve the retained raw input notes. Rate changes rescale the remaining step and gate time rather than clearing or restarting the input pool. Turning arp playback off drains sounding notes safely and retains the pool for re-enabling. Clear Arp remains the explicit way to empty the pool; enabled Clear on Harmony Change and transport-stop behavior still apply.

The Operation panel has exactly eight parameters. The redundant standalone Slot Summary overflow page has been removed; the selected lane and State / Punch remain on the main Operation panel.


## Global follower input scale (0.2.155)

Follower Scale is shared by every HarmonyBus instance, alongside the follower root controls. Edits from any HB panel or Movy's Key control update the same value. Infer uses the shared observed harmony and reference root, so track selection cannot choose a different scale. Content, travel and split remain per-track.

Legacy states adopt the first restored follower scale (or an earlier explicit saved scale). A conductor's default Infer value does not override a saved follower scale. Subsequent track restores cannot undo a live edit; every newly saved track records the current shared value.


## Recorded follower input roles (0.2.156 / Movy hbclean.65)

Movy saves the input root and resolved scale at each follower note onset. Playback projects that note's scale degree into the current global input root and scale before HarmonyBus auto-chord, arp, and harmony rendering. Green playback lights use that projected input. The source MIDI pitch stays saved unchanged, so returning to the original key restores it. Master transpose changes output only. Clip transpose remains a separate semitone edit; its value during recording is retained so it cannot change the captured degree accidentally.

Chromatic notes retain their distance below the next degree. Explicit input-role metadata preserves the ordinary split role and the chromatic split approach even when the projected pitch is a member of the new scale. Live pad presses clear prior playback metadata. Note-offs retain the pitch owned at onset, including if the input key changes while a note is held.

Legacy follower notes have no historical input key. They adopt the active input root and scale when first loaded with a recognized follower context; set the original input key before changing it for an older recording. New context metadata survives saves, copies, recording tails, and Capture. Conductor, rendered-output recordings, and drum notes retain absolute pitches.


## Touch release and tap timing (0.2.158 / Movy hbclean.67)

The default Hold Time is 350 ms. A shorter unused approach touch arms or disarms the pending trigger; reaching 350 ms makes it a momentary hold. Existing explicitly saved custom thresholds are preserved. Movy completes an owned touch release before ordinary knob-model lookups and automation handling. Short touch releases use the same native button burst as Pending / Reset, without sending a second trigger command. Long releases and cancellations do not create a trigger burst.

## Shared controls and recorded performance (0.2.158 / hbclean.67)

Operation lane assignments and settings, input root/scale, master transpose, chord grid and quant grid are global. Lookahead, its anti-buffer, follower buffer, mapping, auto chord and arp remain per track. Performance ownership and recorded actions stay with the affected track. Old travel integers are unchanged; None moves only in the displayed option order.

Movy saves per-input relative operation outcomes beside source notes. Ordinary non-evolving operation values and approach/enclosure steps are captured at input onset; explicitly evolving automatic lanes remain live. These records survive clip persistence and copying and are supplied before source-note playback. Live held lane controls temporarily replace that lane's recorded action instead of applying it twice. Repeat, reverse, time-shift and speed gestures recorded on a clip retain their timed intervals, independent of later button assignments. Existing clips without these records continue their prior behavior; already baked rendered notes remain absolute.

The quiet default pad overlay uses the track's active rendering harmony, and only on followers. Highlighted inputs are those whose actual rendered voices belong to that chord, including Travel None, transpose and lookahead. Inputs rendering the output tonic retain full track color. Other inputs rendering output-scale tones get grey backgrounds; lit harmony overlays replace that grey without blending. Input-scale membership does not change this coloring. Live and recorded-input green feedback retains priority. Optional Current, Effective, Both and Lookahead display choices remain available; conductor tracks keep ordinary keyboard colors.

## Control layout updates (0.2.162 / hbclean.70)

Global places Harmony Flow on its bottom row; the redundant standalone Harmony Flow panel has been removed. Tap / Hold (ms) is the gesture threshold, not harmony persistence. Follow Play exposes Render Velocity %, sharing the existing routed gain with track + volume; Reset and Bypass sit together in Play Tools, the final panel. Operations exposes the global Steps / Perform switch.

Chromatic Auto Chord adds Minor / Min7 and Dim / Min7b5. Chord Grid Free is now labeled Observed: it retains observed harmonic change positions without fixed-grid snapping. The Chord Timing page shows the selected grid, anticipation, Learning/Locked status and transition count, and current loop position. Its bottom row shows Last At, Next At, Last Chord and Next Chord. Last At is the most recent confirmed change, retaining its original registered position rather than the later confirmation time. Next At is the next change in the learned cycle and stays blank until the cycle is locked. Positions are one-based bar:beat with hundredths of a beat, using four quarter notes per bar; 2:3.50 means bar 2, beat 3 plus half a beat. They are relative to the conductor cycle (the combined repeat cycle when several conductor clips are active), and are not shifted by lookahead. Fixed chord-grid and anticipation settings do affect the registered positions. Learning works with lookahead off. An initial harmony, an unchanged chord, and a duplicate seed are not counted as transitions. A clip/model reset clears stale readouts. Per-clip grids are not yet implemented.

Holding a step and Left/Right for 350 ms moves the entry one step; continued holding repeats. Short presses retain timing nudges. Moving retains note metadata and supports Undo. See [the controls review](controls-160.md) for scope and pending conductor-source design.

## Closest Split source groups (0.2.162)

In Harm. / Out, both Closest Split variants classify the source tonic triad from follower root and scale (degrees 1, 3, 5), then choose nearby pitches in the rendered chord or its scale complement. A conductor seventh or suspension does not reclassify the source melody. Content filters narrow the destination group when possible but cannot force a crossing into the other group. Explicit 135 / 2467, 1357 / 246, and Active / Outside splits retain their own policies. Closest Split Chromatic retains its approach-note behavior for chromatic source inputs; source-scale chord tones use the ordinary split assignment. Auto follower scale remains inferred from observed harmony; choose an explicit scale to keep the source context fixed.

## Pad overlays (0.2.164 / hbclean.71)

Harmony colors replace the scale background while lit; grey returns when the pulse is off. Only Current and Lookahead colors mix. Output-tonic track color and playing-input feedback retain priority. Full Lookahead displays the next known observed-loop chord throughout the lead-up to its boundary, including when the render lookahead offset is zero. Both Full Lookahead combines Current with that full preview. At an observed boundary, the full preview advances to the following loop event, wrapping at the loop end. No full preview is shown until a valid loop model is available. These display choices do not alter MIDI mapping or playback timing.

Both Color defaults to Blend, mixing the selected Current and Lookahead colors on shared pads. A specific color (including Track) instead colors shared pads in Both and Both Full Lookahead, while nonshared pads retain their own harmony color. Pulse Shape None produces steady colors and preserves the stored pulse rate. New options are appended so existing saved selections retain their meanings.

The default pulse rate and Standard preset use 1/4-note pulses again. None keeps the display steady without changing the selected rate.

## Live pad preview isolation (0.2.164)

Pad previews discard replay-only source-role and chromatic-target coordinates on their private mapping copy, matching a fresh live note-on. Recorded note rendering retains those coordinates. Regression coverage samples three loop passes with lookahead off/on and compares all pad masks before and after replay metadata changes.

Input Tonic Color in Pads Global selects the follower input-root background. It defaults to Track and also offers Grey (the scale-tone color) and the named harmony colors. Harmony overlays remain above this background. The setting is shared across follower tracks.

Horizontally adjacent pads with the same base color alternate between nearby palette shades when their effective rendered note sets differ. Pads with identical outputs retain the same shade. The remembered last-played note no longer paints a white pad; explicit step-hold editing still shows its note selection.


## Global humanize (Movy hbclean.76)

Humanize / Tools reuses the Play Tools panel. Timing, Velocity and Gate are shared settings, initially zero. Timing is a maximum signed offset in milliseconds, rounded down to whole sequencer ticks at the current tempo and clip speed. Gate and Velocity are maximum percentage variations around recorded durations and attack levels. Offsets are deterministic per track, clip and source onset; chord members move together and repeated loops retain the same feel. They do not rewrite notes, consume operation lanes, change live inputs or alter the active recording track.

Timing and Gate apply only to recorded follower inputs. Conductor durations and onset positions remain exact so humanization cannot move the observed harmony schedule. Velocity applies to both recorded conductors and followers, before their normal rendering and velocity gain. Receiver tracks are not processed again. Clip boundaries constrain timing offsets; a note at the beginning cannot play before transport starts. Existing quantization and harmonic-buffer rules still take precedence. Reset and Bypass in the same panel remain Follow Play controls; set the three global amounts to zero to disable humanize.

Dominant Scale and Borrowed Scale are shared across all tracks, like Follower Scale. New presets restore one shared choice; legacy per-track presets seed it from the first non-default dominant setting. Later stale track copies cannot override an edited or restored global choice.
