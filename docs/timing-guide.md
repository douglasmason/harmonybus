# HarmonyBus timing guide

## Which time determines the rendered note?

**HarmonyBus 0.2.106 / Movy 0.34.1-hbclean.27.** Choose a release time first. At release, map the source note using the effective harmony then available. All diagrams use 120 BPM and 4/4. Times are musical targets, subject to sequencer and audio callback resolution.

![Conductor harmony, effective harmony, capture window and follower release on a shared time axis](timing/overview.svg)

**Example:** the conductor changes from C to D at 2000 ms. A locked model with 1/8-note lookahead makes D effective at 1750 ms. A keypress at 1600 ms falls inside the 350 ms pre-boundary window and waits until 1750 ms. The eighth-note Quant Grid also lands there. The follower uses D at release.

**Defaults and scope:** Follower Buffer is global, initially **1/16 note** (125 ms at 120 BPM), following tempo. The early-lookahead diagrams explicitly use a 350 ms example buffer to illustrate wider capture windows. Changing it on any HB instance changes all followers. Lookahead defaults to **Off**. Quant Grid remains per follower. Musical buffer choices run from 1/64 through 2 Bars; millisecond choices are 0, 25, 50, then 100 to 1000 ms in 50 ms steps.

**Saved sets:** saved buffer values survive the update. For older sets containing conflicting per-instance buffers, the first restored copy becomes the shared value. Set the desired global value once, then save the set; new snapshots store the same value in every instance.

## Follower Buffer chooses among boundaries

The buffer is a pre-boundary capture window, not a fixed delay. Candidate release points come from the chord schedule and the follower's Quant Grid. **The earliest eligible boundary wins.** A boundary exactly at arrival is eligible and does not defer the note to the next point.

![Chord-only capture, an earlier competing quant boundary and immediate release outside the window](timing/buffer.svg)

**A: chord schedule only.** Chord Grid = 1 Bar, Anticipation = On Grid, Quant Grid = Off, Lookahead = Off. A keypress at 1700 ms waits 300 ms for the 2000 ms chord boundary, then maps using D.

**B: add Quant Grid = 1/8.** The same keypress waits only 50 ms for 1750 ms. That earlier point wins, so the note still maps using C. A finer Quant Grid does not guarantee alignment to the next chord.

**C: outside the window.** With Quant Grid Off, 1500 ms is outside the 350 ms window before 2000 ms; the note plays without intentional musical delay. With no enabled grids and no usable active lookahead schedule, the buffer causes no grid capture.

At 120 BPM, a quarter note is 500 ms and an eighth note is 250 ms. A 350 ms buffer therefore covers the entire interval between eighth-note grid points. Off-grid notes wait for the next point, but **already-on-grid notes stay there**. Musical durations scale with BPM; millisecond durations do not.

## Anticipation and lookahead do different jobs

The examples use C to D at 2000 ms, a keypress at 1600 ms, a 350 ms buffer, Chord Grid = 1 Bar and Quant Grid = Off.

![Anticipation changes capture timing, lookahead changes effective harmony, and the offsets do not add](timing/lookahead.svg)

**Anticipation only: 1/8 Early.** The periodic chord capture boundary moves to 1750 ms, but the effective harmony is still C. Anticipation does not predict a future chord.

**Lookahead only: 1/8.** A usable learned schedule makes D effective at 1750 ms and supplies that shifted boundary for capture. The follower releases using D. The chosen recognized harmony persists until the next shifted transition.

**Both enabled:** lookahead supplies the chord schedule while prediction is usable; Chord Grid and Anticipation do not add another offset. Quant Grid remains an independent competitor. While the model is unavailable, ordinary Chord Grid / Anticipation is the fallback.

Only contributing conductor clips enter the prediction cycle. A 3-bar and a 4-bar conductor jointly repeat after 12 bars, including their launch phases and effective playback speeds. Editing content or changing contributing clips invalidates the model; unchanged loops retain it. Non-repeating conditional clips and unsupported cycle sizes fall back to observed harmony.

## Negative lookahead moves the buffer window too

**Lookahead = -1/4, Follower Buffer = 1/16, Quant Grid = Off.** The conductor changes C to D at the bar line (2000 ms). The learned effective harmony changes at 2500 ms, one quarter note later. This shifts harmony selection; it does not postpone the conductor MIDI.

![Negative lookahead shifts harmony to one quarter note after the bar line, with the buffer immediately before that shifted boundary](timing/negative-lookahead.svg)

The capture window is **2375 to 2500 ms**, or **3/16 to 1/4 note after the bar line**. A note arriving at 2300 ms plays with C without intentional delay. A note arriving at 2400 ms waits until 2500 ms and uses D. A note arriving exactly at 2500 ms plays with D immediately. Quant Grid, if enabled, can still supply an earlier eligible release point.

Positive values advance each learned harmony transition; negative values postpone it. Choices are 1/32, 1/16, 1/8, 1/4, 1/2 and 1 Bar in either direction, plus Off. The same signed shift applies around loop wrap. The Shift display reads Early, Late or Live according to the effective schedule.

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

The clip-edit grid is distinct from HB Quant Grid: a clip edit can move starts earlier or later, while the live follower buffer only delays. This action does not add synth glide; portamento and envelope legato remain instrument controls.

## Auto Chord: root, inversion and register

Open **Auto Chord** in the HB menu on a conductor or follower. Chord Mode defaults to Off. **Scale Root** treats the played note as the literal root and builds the selected Chord Form upward from the parent scale selected by HB's Foll Root / Follower Scale controls. A detected chord alone does not uniquely identify a parent scale; select one explicitly when needed. A chromatic played root stays unchanged; upper voices use every second scale tone above it when Chromatic Keys is set to Scale. Global transpose moves the completed voicing as a unit. On an empty conductor, Conductor Chord uses Scale Root for its first gesture so the bus can establish harmony.

**Conductor Chord** uses the recognized effective conductor chord, including its quality and implied tones. Chord Form Auto preserves it; other forms select degrees while retaining recognized third/fifth/seventh alterations and taking added extensions from the parent scale. The played key supplies the desired bass pitch and register. Auto inversion snaps to the nearest chord-tone bass (ties downward); exact chord tones select their inversions directly. With Cmaj7, C3 produces C3-E3-G3-B3, E3 produces E3-G3-B3-C4, and E4 produces E4-G4-B4-C5.

| Control | Choices and effect |
| --- | --- |
| Chord Quality | Auto follows the parent scale (Scale Root) or recognized chord (Conductor Chord). Major, Minor, Dim, Aug, Maj7, Dom7, Min7, Half Dim7 and Dim7 override the chord's third, fifth and seventh; the selected form still controls how many notes sound. |
| Chromatic Keys | For roots outside the parent scale, Scale preserves the original degree behavior. Major / Maj7, Major / Dom7 and Dim / Dim7 choose the corresponding third, fifth and seventh when Chord Quality is Auto. Triad and Seventh forms determine whether the seventh sounds. |
| Chromatic Below | Arm makes the next follower chord gesture one semitone below the key, as Dim7; it then resets. The existing persistent Chromatic Below approach also gives affected chord gestures Dim7 quality. |
| Inversion | Auto, Root, First through Sixth. Auto means root position in Scale Root and bass-from-key in Conductor Chord. Unavailable inversions wrap by chord size. |
| Close | All chord tones within an octave above the chosen bass. |
| Root + Fifth Low | Keep the chosen bass, root and chord fifth low; raise remaining tones an octave. Uses the chord's actual fifth, including altered fifths. |
| Alternate Up | Starting with the closed inversion, raise every other upper voice one octave, then sort by pitch. Preserve the chosen bass. |
| Shell | Retain root, third and seventh when available; use the sixth if there is no seventh. Keep the chosen bass even if it is an omitted extension. A power shell retains root and fifth. |

Explicit Scale Root inversions place the selected bass degree below the played root; the played note still determines the chord identity. Explicit Conductor Chord inversions choose the nearest occurrence of the selected bass degree. At MIDI range edges, shift the whole voicing by octaves instead of clipping or merging its tones. First-inversion Cmaj7 with Root + Fifth Low is E3-G3-C4-B4.

Follower auto-chords are constructed **when the follower buffer releases the input**, using the effective harmony at that point. Their voicing then stays fixed for that gesture, including an ongoing arpeggio. A conductor chord publishes its complete generated pitch set as the source harmony even if its arp sounds only one voice at a time. In Movy, recording that conductor stores the generated MIDI notes in the clip, and playback sends those notes to the synth and Render To channel without generating the chord again. Older, ordinary clip notes still trigger live generation. Chord Mode Off plus Together retains ordinary follower mapping. When either generator is active, it owns pitch construction: Content, Travel, Approach and held-note harmonic retrigger do not remap its output. Arp with Chord Mode Off uses literal input pitches plus global transpose.

## Chord forms and shell examples

Chord Form chooses the degree set. The conductor chord and active parent scale determine alterations; these are scale-compatible forms, not fixed major/minor interval presets. For example, Sixth in natural minor uses the scale's lowered sixth. Use Melodic Minor when you want the raised sixth of that collection. Auto means Triad in Scale Root, and the recognized chord in Conductor Chord.

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

Open **Arp / Strum** on the follower. Playback defaults to Together, Hold to Momentary, Rate to 1/16, Gate to 50%, and Strum Spread to 0 ms. All chord/player controls are per instance.

![Chord generation at release followed by a trigger strum, with cancellation at the source release](timing/chord-player.svg)

**Together** starts the chord simultaneously. **Repeat Arp** cycles through unique held/latched pitches. Rate spans 1/64 through 2 Bars; Gate selects 25%, 50%, 75% or 90% of that interval. **Once** starts each voice once, sustains until its owning source note-off, and cancels unplayed voices on release.

**Strum Spread** is the total first-to-last span for Once: 0, 25, 50, then 100-1000 ms in 50 ms steps, plus 1/64 through 2 Bars. Four voices at 150 ms start at 0/50/100/150 ms. Together and Repeat Arp ignore Spread. Raw notes released in one callback form one group; later keys form new groups.

**Order:** Up, Down, Up-Down, Played or Random. Up-Down repeats without doubling the endpoints; in Once it makes one ascending pass so each voice starts only once. Played follows source gesture arrival order, with generated chord tones low-to-high inside each gesture. Random chooses each repeated pitch independently, or shuffles a Once group.

**Momentary** follows source releases. **Latch** holds the pool; after all keys release, the next gesture replaces it. Overlapping held keys add pitches. Clear Notes, Stop, role/routing changes and player-control changes release the voices; edits require a fresh gesture. Recorded clips use generated voices when the instance is a conductor; live source keys still control their own note-offs.

The arp starts at the follower release target. Musical intervals follow tempo; ms spreads use audio time. Stopped playback uses a free-running tempo clock; rewind clears the gesture. Buffered note-offs retain their inherited delay. Capacity is 16 source keys with up to 12 tones each; excess keys are ignored. Physical device timing needs verification.

## Dominant scale substitution

**Dominant Scale** appears in Foll Root. It is per instance and defaults to Off. It changes the output pitch collection used by ordinary follower scale/extension mapping and by auto-chord construction. It does not change the source-root policy, the input key's degree label, or the conductor's recognized chord.

The trigger is a major-third V chord without a major seventh, or a diminished leading-tone chord rooted a semitone below the configured tonic. In C minor, G major/G7 and B diminished qualify. G minor, Gmaj7, Bb major and Bb7 do not. This first implementation recognizes V and raised-vii function relative to the configured tonic; it does not infer secondary dominants or backdoor cadences.

| Setting | Output collection while the trigger is active |
| --- | --- |
| Off | Use the existing base-scale behavior. |
| Harmonic Minor | Harmonic minor on the configured tonic. Over G7 in C: G-Ab-B-C-D-Eb-F, giving b9 and b13. |
| Melodic Minor | Ascending melodic minor on the configured tonic. Over G7 in C: G-A-B-C-D-Eb-F, giving natural 9 and b13. |
| Altered V | Melodic minor a semitone above V. Over G7: G-Ab-Bb-B-Db-Eb-F. For a leading-tone diminished chord, use tonic harmonic minor. |

G altered is the seventh mode of Ab melodic minor, not a mode of C melodic minor. Its b9, #9, b5/#11 and b13 provide altered tensions; see [Jens Larsen's altered-scale lesson](https://jenslarsen.nl/melodic-minor-altered-scale/). The other two options are tonic-rooted collections. There is no single tonic-rooted C melodic-minor mode that describes that same G-altered collection.

Recognized chord tones remain legal in ordinary In Scale mapping; Conductor Chord forms preserve the detected third/fifth/seventh. Therefore an unaltered G7 can retain D even under Altered V. Requested ninths use b9, elevenths use #11 and thirteenths use b13; #9 is available in the scale collection for nearest-note mapping. Root-derived Scale Root chords use the selected collection directly.

The substitution follows the effective harmony, including positive or negative lookahead, and stops applying when the trigger disappears. Buffered inputs use the collection at release. Already-generated chord/arp gestures retain their original pitches until a new gesture starts; changing Dominant Scale explicitly clears currently sounding follower notes.

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
