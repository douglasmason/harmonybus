HarmonyBus 0.2.263 / Movy hbclean.180: Live Key Center survives clip recording and Undo. Separate recordable absolute and recursive relative operations add signed shifts, Step Back, Return to Start and optional return after N modulations. See [Key center operations](docs/key-center-operations.md).

HarmonyBus 0.2.262 / Movy hbclean.179: Coherent Move and Launchpad pad snapshots for Movy hbclean.179, reusing identical rendered inputs within each read. The paired Movy update batches external note input, makes Launchpad pulses steady, paces LED traffic and widens footer arrows. No Schwung host changes. Physical Move latency and crackle testing remain necessary.

HarmonyBus 0.2.261 / Movy hbclean.178: Motif to Clip writes editable input intent with preview, Replace/Append/Overdub and Undo. Motif bodies exclude all boundary targets; shared target placement and padding also control live playback. See [Motif loader](docs/motif-loader.md).

HarmonyBus 0.2.259: Reuse identical pad renders within each display snapshot. Preserve current/next colors, output groups, approach intent and trails while avoiding duplicate pitch-class probes. Live and recorded playback rules are unchanged. Compatible with Movy hbclean.176. Desktop profiling improves preview cost; physical Move crackling still needs verification.

HarmonyBus 0.2.260 defaults regular Follower Travel to None, while Follower Key Travel stays Relative. Closest mappings softly favor ordered scalar runs within their original six-semitone radius and class boundaries, retaining the destination pitch-class multiset. Movy hbclean.177 defaults fresh sets to the Approach layout; saved choices are preserved. HarmonyBus 0.2.260 gives Follower Travel, Conductor Key Travel and Follower Key Travel the same seven choices, with independent saved selections: Relative, Nearest Octave, Closest Chord Tone, Closest Scale Tone, Closest Split, Upward and Downward. Relative carries the phrase using one shortest tonic shift and destination scale degrees; Nearest Octave folds each mapped degree toward its original register. Closest conductor voicings preserve the complete destination chord before choosing an inversion. Upward/Downward preserve mapped degrees while choosing their directional octave. MIDI boundaries can force the available octave. Regular follower Closest remains a legacy alias for Closest Scale Tone; None and Same as Conductor retain their separate bypass/inheritance roles. Existing Relative follower phrases may change octave where the old mapper folded individual degrees. Travel=None still applies the active dominant family: in D Dorian, A7 raises the seventh-degree output from C to C#. Follow Detected colors preserve that corrected dominant identity; next/full-next colors compare the pad output rendered now against the next chord.


HarmonyBus 0.2.257 fixes displaced playing highlights while paused: opening-chord previews no longer replace the live mapping used to locate sounding notes. Current, next, full-next, approach-row and selected-tone pulse colors now compare the same currently rendered pad note against their respective chord tones. Next coloring answers what can be played into the next chord now, rather than predicting which input will work after its mapping changes. The shared pass also removes repeated future-context rendering. Live and recorded notes now retain separate input owners, including shared output pitches, chord/arp latch, captured motion and rhythm delays. Live note-off cannot release a clip-owned note. Held Retrigger keeps its saved behavior across chord changes. Update with Movy hbclean.173 for Foll Map in every Copy-tap mode and correctly tagged recorded pressure.

**HarmonyBus 0.2.258:** One Follower Key Travel setting now controls live and recorded notes, defaulting to Relative; Conductor Key Travel defaults to Closest Split. Saved shared choices are retained; older conflicting live/recorded choices migrate to the recorded policy to preserve clip playback. Key Travel: Closest Chord Tone now keeps chord and non-chord input classes separate when follower travel is None or legacy chromatic travel, preventing the entire scale from collapsing into a triad after a key-center change. Single-note pad previews reuse their output when Auto Chord is off, removing a duplicate mapping pass. Follow Notes keeps each row attached to one live or recorded voice owner. Desktop regression tests pass; device crackling remains under investigation.

HarmonyBus 0.2.256 preserves the conductor's chord progression through Key Center changes, then uses the shared closest-assignment engine to choose an inversion near the recorded range. Follower Closest Split maximizes available pitch-class diversity within its chord-tone and non-chord-tone pools; it never crosses those classes for diversity. Complete generated conductor chords retain their destination tones and voice counts. The current/next footer follows the transformed harmony. Reused preview assignments, direct octave selection, and skipped inactive operations reduce repeated pad work. Next Pulse Off is explicit, and color-only previews skip unused trail targets. Compatible with Movy hbclean.172; device crackling still requires Move testing. See [review and validation](docs/key-center-review.md) for measured improvements and recording limitations.

HarmonyBus 0.2.255 derives active Key Center pad-color membership, next-harmony pulse roles, and root highlighting from the same functional harmony transformation used by playback. For example, G7 resolving into A minor is colored as E7 rather than Em7. Key Center now displays Armed before a target is known and Arm>destination for a held target or scheduled motif landing. The committed key is available immediately without another note. This is a visual correction; the reported Key Center crackling is not yet resolved. Compatible with Movy hbclean.172.

HarmonyBus 0.2.254 fixes played-target trails: every pad-color mode uses the effective playing harmony, independent of full-lookahead background colors. Partial lookahead remaps remembered sounding pitches immediately; Current Chord history expires at the unshifted learned chord boundary, not at the anticipation point. Existing decay, pitch/pitch-class matching, and generated-voice filtering remain unchanged. Compatible with Movy hbclean.172; only HarmonyBus needs updating.

**0.2.253 — Key Center Travel:** Shared conductor, recorded-follower and live-follower policies (followers default to Same as Conductor). Follower policies apply to Travel=None/legacy Direct. Relative uses a consistent tonic shift; Closest Scale Tone, Closest Chord Tone and Closest Split provide register-oriented alternatives. Chromatic On and recorded approach rows resolve their target before applying approach intent. Saved input notes are not rewritten. Use Movy hbclean.172 for the grouped controls.

**0.2.246 / Movy hbclean.163:** Chord + Arp and chord-state performance controls can adopt physically held target pads, independently of Retrigger Held. The one-shot Chord + Arp gesture ends with its adopted target release; explicit latch remains persistent. Rapid momentary release/repress preserves rearticulation and rearms the arp even when both edges fall between audio ticks. Secondary V (Dom) uses a semitone-below leading-tone approach for diminished destinations; plain Secondary Fifth is unchanged. Old Secondary V labels remain accepted.

Movy Harm Play Settings knob 7 advances a live motif on a held target without recording a new input press. Harmony Setup in Steps offers Reset Track (Shift + touch): restores this track’s HarmonyBus role, routing and local defaults by track number, clears local performance state and learned predictions, and preserves clips, instruments, mixer settings, shared settings and other tracks. Reset tracks inherit the set’s shared role defaults.

HarmonyBus 0.2.245 / Movy hbclean.161 adds Dominant Color on Harm Play Settings knob 5. Touch applies the selected dominant family to the current track, release restores the underlying major/minor settings, clockwise turn latches, and counterclockwise turn switches it off. Shift-turn selects a family (Altered V initially). Hold is solid teal; latch pulses teal. Only the selected family is saved; active holds/latches reset on stop or reload. Altered V keeps secondary preparation chords on their simplified baseline.

Target Scale Source is now knob 6. The Simple Major, Minor and Dim families move to Harmony Setup in Steps mode. The control remains usable alongside the sixteen Harm Play step assignments. A conductor color suspends learned predictions while active and reuses its saved baseline timeline on release; it does not replace that timeline with a temporary performance.

Dominant to Major and Dominant to Minor now default to Simplified Target: major destinations use Major and minor destinations use Harmonic Minor. None disables dominant substitution and keeps the parent collection. Every enabled dominant family retains the dominant root, major third and flat seventh. Altered V affects V only; secondary preparation chords use the simplified target baseline. Existing saved family selections are preserved.

Copy-tap filters the HarmonyBus panels by mode. Steps contains setup and diagnostics; Perform 1 contains operation controls and performance tools; Perform 2 contains Harm Play, secondary harmony and chord/arp controls. Startup and project load always select Steps. Copy hold retains native copying; switching modes does not reset latched musical operations.

Learned next-harmony timelines are saved per conductor track and clip slot. Matching clips recover their predictions on the first playback after a reload; edits and incompatible render settings invalidate the corresponding record. The stopped opening-chord preview remains independent. Trail Decay has one larger preview below readable text controls; selectors clear the surrounding controls.

Secondary Fifth is now a separate parent-scale degree operation. Secondary II, IV and VI (Dom) simplify the target in Auto and use its dominant-preparation family; Altered V remains V-only. Ordinary secondaries preserve the parent collection by default. Parent/Simplified explicitly overrides Auto target simplification. Existing operation IDs are unchanged.

The performance footer now shows compact mode/track, sounding key center and parent scale, and current > next harmony as key-relative Roman numerals. Unknown harmony is --; h7 means half-diminished. Long scale names are abbreviated to fit. Footer data rides the existing pad snapshot without additional host reads.

**HarmonyBus 0.2.244 / Movy hbclean.160 — reinforced target trails.**

Trail reinforcement starts a single target onset at 80% of Trail Strength and a repeat within the selected history window at 100%. Dim Trail decays toward 30% of Trail Strength; Blend into Background still fades to the background. Repeats refresh the peak and decay continuously. Pitch Class combines octave hits. Generated chord voices and arp repeats do not count. The native history retains two onsets per pitch, including hits between UI polls. Install HarmonyBus 0.2.244 and Movy hbclean.160 together; Movy retains single-hit compatibility with older history snapshots. Validated against Schwung 1.7.3.

**HarmonyBus 0.2.243 / Movy hbclean.159 — target scales and full Harm Play banks.**

Harm Play 1 and 2 now expose all sixteen saved operation/motif assignments, matching step buttons 1–8 and 9–16. Harm Play Settings holds Key Center, Parallel Scale, Motif Latch and Chord + Arp on knobs 1–4; the step buttons remain playable from this page. Shift-turn Parallel Scale still selects its scale. The four special controls retain teal feedback, captured releases and latch behavior. Key Change Scale and Conductor Travel remain on Key & Scale.

Settings knobs 5–8 are Target Scale Source, Simple Major, Simple Minor and Simple Dim. These are saved per track:

- **Auto** uses the parent collection for ordinary secondary degrees and simplifies the destination for Secondary V and Leading Tone. Existing Simple Chord/Simple Scale operation choices remain active in Auto.
- **Parent** keeps the destination's parent collection, including the established borrowed/chromatic-target fallback. **Simplified** uses the selected destination-quality family. Both explicit choices override legacy per-operation simplification flags.
- Major: Major (default), Lydian or Harmonic Major. Minor: Natural Minor (default), Dorian, Harmonic Minor or Melodic Minor. Diminished/half-diminished: Locrian (default) or Locrian #2.

The destination's third and fifth select the quality family. Existing Dominant to Major/Minor treatment is applied afterward; a named dominant-family choice can therefore override the simplified collection on ii/V/leading-tone approaches. Parent / Minimal leaves the chosen collection in place. Dominant and leading-tone chord function is retained. Live chords, single-note approaches and motif/cadence rendering share the resolver; stored source pitches and assignment IDs are unchanged. Old states without these controls load Auto with the default families, so explicit dominant/leading-tone approaches now use Auto's tonicized destination collection. Existing held voices finish with their onset pitches.

Standalone HarmonyBus exposes the same four choices on Secondary Scales. Install both versions for the complete Movy settings page. Device audio crackles remain a hardware verification item.

**HarmonyBus 0.2.242 / Movy hbclean.155.** Adds Live Harmony Override (default scope) and Live + Recorded Harmony Override, using resolved musical intent with conductor harmony underneath. Chord + Arp now offers six combinations: Chord Only, Arp Only or Both, resolving on first target press or release. Persistent latches stay active.

Movy adds Pad Trails under Shift + Step 9: turn the dial through Set Parameters, Pad Colors, Pad Trails and Trail Decay. Trails remember a resolved single target per input gesture, never every generated chord voice or arp repeat. Exact pitch is the default; pitch class is optional. History is matched against each pad's displayed current/next target. Choose infinite, beat, current-chord or previous-plus-current-chord history, color/strength/pulse, linear or exponential decay, and exponential shape 0.5/1/2/4. Exponential duration is a half-life. Chord Forms stays grouped as before.

Color order is harmony/pulse, trail overlay, played highlight, then approach-row brightness. Trails share the existing bounded pad snapshot; animation performs no native reads. Copy-mode layout changes reuse cached metadata and release physical holds without resetting all performance state. Hardware crackle behavior still needs Move testing.

**HarmonyBus 0.2.241 / Movy hbclean.154 — conductor-owned shared context.** While recording a conductor, Key Center landings, Parallel Scale starts/ends and explicit parent-scale changes are saved on that clip's musical timeline, including changes performed without a note. Follower uses remain live-only. Ordinary chord/arp settings, timing and humanize remain current rendering controls; this does not introduce general knob automation. Infer remains a live base-scale selection.

Panel cleanup: **Key & Scale** replaces Global Transpose and adds Sounding Key. Touch Hold lives in operation Conditions; Arp Clear lives in Arp / Strum. Foll Root no longer duplicates dominant/borrowed controls from Track Scales. **Context Sources** is the detailed ownership view. Chord Forms is unchanged.


Conductor contributions are aggregated before each sequencer MIDI batch. A clip owns its contributions; ending a gesture, stopping a clip or replacing a set removes that ownership, without restoring stale global snapshots. A gesture crossing the loop boundary remains active until its recorded ending. Playback from the middle reconstructs the circular timeline. Explicit selections use the most recent active contribution (higher track number breaks exact ties); live gestures override recorded contributions until released. Key/parent changes persist in the clip until replaced; a momentary take closes at recording end, while a latch remains persistent. Master Transpose applies once outside the stored key changes. The target note of a recorded key landing retains the key context in which it was performed, avoiding a second key mapping on later loops; its chord form can still be edited.

**Context Sources** shows Sounding Key, Base Key, Key Owner, Parallel Owner, Parent Owner, Transpose, Sounding Scale and capture status. `T2 REC` / `T2 LIVE` identify the winning owner; `+` indicates additional overridden contributions. Existing operation lights reflect the current track's ownership. The page uses one bounded snapshot read; no new pad animation or per-cell polling. Sparse events use binary lookup, and global replay writes happen on changes rather than every audio block.

Validation includes native ownership/transpose/onset tests, the sequencer and DSP suites, loop carry and persistence, and the full Movy/Schwung/HB chain recording and replaying independent conductor contributions. Device crackle resolution remains a hardware verification item. Future-key-event lookahead visualization is not added in this release; the new panel reports the effective shared context.

**HarmonyBus 0.2.240 / Movy hbclean.153:** Pads Global > Pad Colors > Harmony Off keeps an input-scale/tonic background and played-input feedback without current/next-harmony coloring, harmony pulses or rendered-output grouping. The native snapshot skips per-pad voice previews and lookahead color analysis in this mode; unchanged layouts no longer repeatedly submit preview inputs. This is a display choice and does not disable musical follower mapping or lookahead timing.

Unchanged relative key contexts now reuse the original conductor voicing instead of harmonizing it again; identity pitch masks also return directly. Closest-chord-tone and Blues transformations remain active. Recorded-conductor regressions compare exact pitches/octaves with Key Center off, armed, and landed on the same key.

Transport heartbeats, clip-position updates and opening-preview telemetry no longer trigger the general all-track settings/Chord+Arp resynchronization path. The existing conductor-before-follower barrier and ordinary settings synchronization remain. A 16-track desktop benchmark reduced the heartbeat dispatch portion from about 47 to 12 microseconds; this is not a hardware deadline measurement or confirmation that device crackles are resolved.

**HarmonyBus 0.2.239 / Movy hbclean.152:** Track Scales has independent Dominant to Major and Dominant to Minor preferences. Parent / Minimal is the major default; Harmonic Minor is the minor default. Both offer Parent / Minimal, Harmonic Minor, Melodic Minor, Altered V, Major and Harmonic Major. Selection follows the resolution target's quality before borrowing, including explicit secondary intent. Named families apply to generated preceding ii and V; Altered V leaves the preceding ii in its parent context and uses harmonic minor for leading-tone treatment. Explicit chord tones remain authoritative. Auto Local preserves available parent modes rather than always rebuilding major/natural minor. Harmonic Major is also in the shared regular and parallel scale library. Key changes preserve dominant and leading-tone function, including conductor output. Ordinary Scale Degree input does not inherit the preceding dominant's temporary scale.

Movy submits changed melodic pads as one bounded native cable frame, removing Schwung's 16-LED-per-UI-tick staging on supported hosts; immediate melodic feedback uses the same path. Rejected frames retry and the existing 40-packet budget remains. HB no longer repeatedly reads saved set files to discover channel ownership in an audio callback; unresolved Auto channels wait for authoritative host source tags (or an explicit source channel). These changes remove identified sources of work and display staggering, but the reported device crackles still require hardware verification.

**HarmonyBus 0.2.238 / Movy hbclean.151:** Blues is available in both Parallel Scale and the regular scale library. It uses Dorian melodic degrees with dominant chord quality on every root, retaining chord forms (major triads, dominant sevenths, dominant ninths). The old six-note keyboard scale remains at its saved ID as Blues 6-note. Scale definitions now come from HarmonyBus `data/scales.json`: intervals, names, stable host/keyboard IDs and chord policy generate native tables, module choices and the pinned Movy catalog. Relative Major/Minor remains a separate parallel operation. CI rejects stale generated catalogs.

**HarmonyBus 0.2.237 / Movy hbclean.150:** Shift-turn Chord + Arp selects Chord Only, Arp Only, or Both. Ordinary clockwise/counterclockwise turns permanently latch on/off; the existing teal latch pulse remains. Chord Only bypasses arpeggiation, Arp Only uses played notes without chord expansion, and Both combines them. Selection is saved; old sets retain Both. Mode boundaries release old voices safely. Pad-preview reads now remain limited to one per 50 ms even during rapid gesture changes; note feedback and native LED pulses remain independent. This reduces display traffic but does not establish or eliminate the reported hardware audio scratchiness.

**0.2.236 / Movy hbclean.148:** Root Only, Root + Third and Root + Seventh join the shared chord forms. Chords owns Quality and Inversion; Chord Forms has independent Current Color and Next Color selectors. Multi-tone Next Pulse uses distinct peak strengths (first 100%, second 75%, third 55%). Adjacent Pad Shading defaults Off, with its resting pattern cached independently of pulse/play brightness.

Parallel Scale adds Relative Major/Minor. Key Center feedback distinguishes a changed tonic from a scale-only change and shows the current/destination tonic with a minor suffix. Master Transpose moves an active key context consistently. Conductor input is reinterpreted before voicing its current chord form; recorded source data and note-off ownership remain intact. Closest and Closest Split use cached joint assignments balancing movement, register and pitch-class collisions; diversity is a preference, not an unconditional requirement. Destination pools remain mandatory and assignment travel is bounded to six semitones (MIDI-boundary fallback uses the nearest legal note).

Repeated preview-layout writes are suppressed, identity key maps bypass harmony reclassification, and repeated transformed harmonies are cached. Native and browser checks cover these changes; reduced desktop processing cost does not establish that hardware audio dropouts are eliminated.

**Connector controls:** Pitch Play is now Approach Harmony. Connector Below (CCB) and Connector Above (CCA) use Connector Harmony. Leading Tone (LT) and Upper Dim keep diminished triads/sevenths; Tritone Sub (TTS) keeps dominant quality. Leading Tone and Upper Dim are available in the Ops lane and Harm Perform bank pickers. Legacy saved connector assignments retain their behavior.

**Fixed lane editing:** Ops 1–8 and Ops 9–16 always control lanes 1–16. Shift + turn changes the operation on that fixed lane; named performance controls retain their function and use Shift + turn for their amount or mode. Editing preserves an existing latch and does not change the lane-editor selection.

**0.2.223 / Movy hbclean.130:** Harm Perform knobs use tap for one use, hold for momentary application, clockwise turn to latch, and counterclockwise turn to unlatch. Shift + turn changes the assigned operation or motif without arming it. Overlapping performance touches compose a finite sequence, ignoring each entry's saved latch preference without changing those preferences. The separate Performance Latch panel is removed; Harm Perform panels are at the end. White is solid for trigger/hold and pulses smoothly for an active permanent latch. Triple Approach shows three amber row assignments; other layouts show only the latest selection. Sounding outputs use the full configured Play Color, including regular green, regardless of whether their input pad is held.

**0.2.220:** Approach rows repeat a fixed transformation, with resolution on the separate target pad. Selecting rows no longer arms a global performance sequence on approach layouts. This also keeps their rendered play lights and role colors aligned after repeated presses.

**0.2.219:** Auto Chord “Scale Degree” is now named “Rendered Note Root.” This is a label change; saved settings and the old Scale Degree/Scale Root command names remain compatible.

**New-track defaults:** Follower inversion is Played Top Note; conductor inversion is Auto. Arp order defaults to Shuffle. Saved overrides remain intact.

**Scale Degree Auto Chord:** Followers expand the note after travel and Follow Play into a chord, with that rendered note as root. Source-note ownership remains unchanged for releases and recorded input.

**0.2.218:** Receive Channel adds Off and defaults to Off. Disabled receivers no longer fall back to channel 1. Turning reception off releases held receiver notes; saved explicit channels remain compatible. Ordinary local input continues to use automatic source matching. Follower/input scale defaults to explicit Major; Infer remains available and saved scale choices are preserved.

**Approach routing fix (HB 0.2.217 / Movy hbclean.126):** Dedicated Approach and Triple Approach layouts enable follower approach pads independently of chromatic mapping and travel. Sound and play-color previews use the same eligibility rule. Operation knob LEDs own their indicators; generic parameter-value lights cannot overwrite them.

**FIFO Approach / Perform 2 (HarmonyBus 0.2.217, Movy hbclean.125):** Copy cycles Steps, Perform 1 and Perform 2. Perform 2 knobs and step buttons share the same 16-slot operation/motif bank on every layout. Each new touch enters a persistent three-item FIFO, without requiring overlapping touches. Triple Approach plays the queue from the top row down: row 3 (oldest), row 2, row 1 (newest), then the scale target. A fourth touch evicts the oldest assignment. Each row repeats the first step of its assigned motif; single Approach uses the latest touch. Row-assignment touches do not arm a performance sequence on layouts with approach rows. The display mirrors rows 3–2–1, highlights the newest assignment, and retains a compact slot summary. Tap/hold/latch performance and overlapping performance sequences remain independent of the row FIFO. Existing bank assignments are preserved; the eight triple motifs remain optional library choices.

> **0.2.214 / Movy hbclean.123:** Secondary II consolidates Scale Above. Secondary LT consolidates Chromatic Below and remains a semitone below the target; Auto Chord uses the Chromatic Chord setting. Secondary VII follows the effective scale’s seventh degree and can differ from LT. Legacy saved assignments and recorded note intent remain compatible. Pitch Play and Secondary panels now access the same Secondary II lane.

> **0.2.213 / Movy hbclean.122:** Copy cycles Steps → Perform → Approach. Approach has a separate motif bank on steps 1–16 and ordered transformation/motif selection on knobs 1–8. Turn knobs to assign, overlap touches to sequence, then play approach pads to advance.

> **0.2.212 / Movy hbclean.121:** Fixed operation knobs: clockwise latches on, counterclockwise off, tap arms one use, hold is momentary. Rendered-only notes blend a faint play color into the pad background; matching source and output use the solid play color.

> **0.2.210:** Track source input and sounding output separately. Pad playback now follows final emitted notes from live/recorded Auto Chord, arps, motifs and motion echoes, mapped back through each pad’s single-note rendering. Requires Movy hbclean.120 for output highlights; held pads retain immediate feedback. Includes the 0.2.209 processing optimization.

> **0.2.209:** Reduce synchronous pad-preview and note-render work by reading operation tags without repeatedly copying full motion settings. Keep recorded overrides and active motion behavior intact. Pair with Movy hbclean.119 or newer for the separate pad LED delivery fix.

> **0.2.208:** Inversion now calls Top Note **Played Top Note**, clarifying that the played melody anchors the top of the generated voicing. Includes the follower pad-color fix and Shuffle / Shuffle Cycle Pin choices.

> **0.2.207:** Arp Order offers Shuffle (honor Arp Start on launch) and Shuffle Cycle Pin (also pin every cycle). Both follow Arp Start and visit each available note once per cycle.

> **0.2.206:** Follower Auto Chord pad colors reflect the single note rendered with Auto Chord off, including conductor-harmony mode and Top Note. Expanded voicings still determine equivalent-output groups.

> **0.2.205 / Movy hbclean.116:** Stopped startup harmony preview, coherent pad updates and protected Perform lights. Four defaults panels become one Role Defaults editor; Track Scales gains Local Palette and Foll Map gains Current / Next Harmony Target. See [panel controls](docs/panel-controls.md).

> **0.2.204 / Movy hbclean.115:** Shared Track / Lane Edit Target on Chords and Arp / Strum, temporary Chord/Arp State operations, burst presets and explicit Root/Bass + Top input. Start Note defaults to Played / Pad; Start Timing defaults to First Note Free. See [state editing](docs/chord-states.md).

> **0.2.203:** Adds complete-page live snapshots for Next Harm, Chord Timing and Follower Root so Movy can refresh position and harmony fields together. Includes the three context scopes introduced in 0.2.201. Pair with Movy hbclean.113 for the display refresh fix.

> **0.2.201:** Context Scope now offers Current Harm, Current + Next, and Full Loop on Track Scales and both role-default scale panels. Current Harm uses chord-local recipes without progression inference; Current + Next recognizes adjacent-pair resolutions; Full Loop additionally uses preceding/following chords. Explicit recorded intent remains authoritative at every scope. Full Loop remains the factory default.

> 0.2.199: Mixed cadence panels, conductor/follower chord defaults, visible track overrides, strict/automatic local gap scales and recorded harmonic intent. See [operations](docs/operations.md).

> **0.2.198 / Movy hbclean.106:** Complete Secondary II–VII controls; Parent Scale, Simple Chord, and Simple Scale policies on II/III/IV/VI/VII; separate Chrom Above connector and Tritone V. Parent Scale II follows the actual next scale degree. Older recorded approaches retain their original behavior.

> 0.2.197 / Movy hbclean.105: Pitch Play and Pitch Cadences have dedicated named controls; Ops 1–8 and 9–16 are fixed to the sixteen Step Seq slots. Added three-press ii–V–target cadences, Chrom Above (tritone-sub dominant in chord mode), and Tritone II. Install both modules for named-control recording.

> 0.2.196 / Movy hbclean.104: Backdoor II/V operations, relative cadences over chromatic approach pads, destination-relative dominant colors, Whole Tone/Augmented input scales, and expanded altered/extended chord recognition. Existing lane defaults are unchanged.

> 0.2.195: Tap to arm, hold momentarily, double-tap for persistence on operation knobs and steps. Persistent activation pulses smoothly; armed/held lanes stay solid. One Next Harmony control replaces Next Once / Next Latch; lane 6 is available.

> 0.2.190: Chromatic approach pads generate the selected chromatic chord family on their rendered root, even when that root is diatonic. Fresh instances default to Dim / Dim7; saved settings remain intact.

> 0.2.189: Inversion → Top Note anchors a generated chord to the rendered played melody, keeping Chord Mode and spacing independent without another knob.

> 0.2.188: Bulk operation-editor snapshots remove repeated synchronous page reads in Movy hbclean.96, preserving old-lane pending edits and coherent dependent menus.

> 0.2.187: Auto Chord Repeat operation temporarily enables repeated chord playback using existing panel settings; tap latches, hold is momentary. Turning it off restores the saved mode. Tonal/parallel-harmony tests and guide updates are included.

> 0.2.186: Piano approach gaps report their actual scale/current/next harmony membership for Movy hbclean.94 coloring. Remove redundant Direct travel from the selector; saved Direct sounds are preserved as None.

> 0.2.179: supports Movy hbclean.87 piano-gap approaches in Closest Split Chromatic. Each gap approaches the effective mapped output of its lower pad, with independent note ownership and recorded identity. Other travel modes leave gaps silent. Pad coloring rules are unchanged. Update both modules.

> 0.2.185: Lookahead now groups Off, Immediate, After, Before and Late in the existing selector. After measures from the current chord start and resets at each chord change; old saved offsets retain their meanings. Both Full Lookahead keeps current/next pad interpretations visible independently of the rendering switch. The timing guide explains this look-back reference. Compatible with Movy hbclean.91.

> 0.2.184: Lookahead now includes Immediate. It always renders against the next known harmony and advances at each actual chord boundary, including loop wrap. Anti Buffer does not delay Immediate. Off remains the default; saved timed choices keep their meaning. Works with Movy hbclean.91.

> 0.2.183: Follow Map keeps the assignable lane-5 touch knob and adds a plain lane-6 Off/On toggle. Movy hbclean.91 labels these Next Once / Next Latch; touching the toggle only shows its description. Descriptions follow operation edits. Older assignments remain intact.

> 0.2.181: Follow Map adds an assignable Map Touch knob, defaulting to lane 5 (Harmony → Next, Auto Off). Lane 5 advances all follower pads until the next chord change; lane 6 stays latched across changes. Auto Off on the existing Conditions panel selects Normal, Chord Change or Manual for harmony/approach operations. Colors follow the selected mapping. All nine operation-knob assignments are global across tracks. Pair with Movy hbclean.89.

> 0.2.180: Chromatic is an independent follower setting, On by default, supporting Relative, Closest and the other Travel modes. Follow Touch has eight assignable lane selectors (1, 2, 3, 4, 13, 14, 15, 16): turn to select, tap to trigger, hold for momentary operation. New factory lanes 15/16 are Scale Above/Chrom Below; touch order determines the pending enclosure. Saved lane assignments and legacy travel sounds are preserved. Pair with Movy hbclean.88.

> 0.2.178: Current and Full Lookahead colors advance together on the learned boundary, including Lookahead Off while live chord detection settles. Current colors use their own follower mapping instead of the early effective mapping. MIDI timing and saved settings are unchanged; no Movy update required.

> 0.2.177: Lookahead pad colors now preview the upcoming chord’s follower mapping, so highlighted input keys land on chord tones when it arrives. Defaults: Both Full Lookahead, pulse rate 1/4, pulse shape None, Current Yellow, Play Green, Lookahead Red, Both Orange, Input Tonic Grey. Existing saved settings are preserved. Works with Movy hbclean.85; no Movy update required.

> 0.2.176: Pads Global adds Play Color for live, recorded and retained input highlights, including Off. Input Tonic defaults to Grey; Play Color defaults to Track. Effective is the default; redundant Standard is removed. Effective shares Current Color. Requires Movy hbclean.82 for configurable playback LEDs.

> 0.2.175: complete a learned conductor-clip traversal only after the wrap boundary's MIDI batch has been processed. A restart that misses the already-sounding opening chord no longer locks an incomplete schedule, invalidates it at the wrap, and waits through a second pass; deterministic clips lock after their first complete traversal.

> 0.2.174: learned next-harmony knowledge is independent of the Lookahead time. At Lookahead Off, Full Lookahead pad modes, Next Harmony, chord-transition diagnostics and explicit Next harmony operations remain available; ordinary effective harmony and note rendering stay current and are not shifted early.

> 0.2.173: Chords adds Shell 7, Shell 9, Shell 6/9, Rootless 7 and Rootless 9. Voicing omissions retain semantic chord tones for followers and pads. Form, quality and voicing edits apply at the next input onset. Chord Form joins existing Operations, including cycle conditions and recorded outcomes (Movy hbclean.79). Learned conductor timelines are now retained independently and recomposed at actual launch, including full deterministic form-operation cycles. New clips/settings still need observation; unplayed source scanning and whole-event reshaping of old baked captures are not included.

> 0.2.172: restarting the same cached clip resets its launch boundary, including clips that begin with silence.

> 0.2.171: Pause/Stop preserves raw/baked note-offs before clearing pitch ownership, preventing stuck local and routed voices. retains up to 32 learned clip/playing-conductor configurations across launches, with phase alignment and eviction on confirmed divergence, missing transitions, or edited clip contents. Requires Movy hbclean.78 for explicit slot identity. Cache is session-local and learns a new or changed rendering configuration on its first full pass. Simultaneous conductor combinations are retained as combinations; queued future launches are not predicted before activation. Tempo is available in Humanize / Tools, using the shared Schwung tempo request protocol (20–300 BPM). External Link peers can own tempo. Movy .78 additionally records raw Auto Chord gestures and marks the selected track button independently of note activity.

> 0.2.170: existing Play Tools becomes Humanize / Tools with global Timing (0–30 ms), Velocity (0–30%) and Gate (0–30%), all default 0/off. Requires Movy hbclean.76 for playback processing. Recorded follower inputs receive early/late timing and gate variation; recorded conductor inputs receive velocity variation only, preserving conductor analysis. Chord attacks move together, offsets repeat across loops, and tracks vary independently. Live playing and recording are unchanged. Velocity varies recorded input attacks before the existing render-velocity gain. Timing uses the sequencer tick resolution, stays within the clip loop, and remains subject to explicit quantization and harmony-buffer rules. Play Reset and Bypass retain their previous per-track Follow Play meanings.

> 0.2.167: Chord Timing now shows the last confirmed transition position and the next learned transition position, plus their chord names. Positions are one-based bar:beat (four quarter notes per bar), relative to the conductor loop; e.g. `2:3.50` means bar 2, beat 3 plus half a beat. Lookahead does not shift these readouts. Grid Status reports Learning/Locked and the number of genuine changes, excluding a constant harmony or duplicate seed. Next stays blank until a complete cycle is learned. Observation works with lookahead off; predicting a future change requires the learned cycle. With multiple conductor clips, positions refer to their combined repeating cycle. Fixed Chord Grid/Anticipation settings are reflected in registered positions. Compatible with Movy hbclean.74; update HarmonyBus and restart to load the expanded panel.

> 0.2.152: correct the stale 0.2.137 module title and description shown in Schwung Manager. Display name is now simply Harmony Bus; the version field and DSP knob carry the release number. No musical changes from 0.2.151.

> 0.2.151: Arp Hold adds Single variants; previous Latch modes remain Overlap with their saved indices preserved. Toggle-off identifies raw input note and channel. Auto Chord replaces its duplicate Follower Scale knob with Clear on Harmony Change (default Off); committed chord changes remove released latched inputs and retain held pads. Movy hbclean.61 displays the exact retained input pads, independent of rendered pitches.

> 0.2.150: pending approaches advance once per incoming note gesture (simultaneous distinct notes within 25 ms count as one; a repeated key starts a new gesture). Buffered presses retain their own stages. Generated arp and strum notes inherit their source stage and cannot consume another stage; an arp repeats that source voicing until a new input replaces it. Touch Mode names distinguish Arm for consumable pitch triggers from Latch for continuous operations; legacy Toggle values retain their meaning. State/Punch shows Held, Latched, Off or the pending sequence. Pending/Reset shows the remaining enclosure order. Existing Movy hbclean.60 is compatible; no Movy update required.

> 0.2.149: shared ordered approach triggers across Movy knobs and step controls. Short taps arm/toggle; long holds gate immediately and release off. Above/Below tap order creates a three-gesture enclosure; re-tapping an unused modifier removes it. Global Hold Time defaults to 250 ms; operation slots offer Hold, Toggle and Tap/Hold. Pending triggers are runtime-only and reset on stop/teardown. Requires Movy hbclean.59 for timed hardware gestures.

> 0.2.148: Arp Rate adds Cycle durations to fit a complete pattern into the selected time. Ordinary rates remain per step. Up-Down counts the return path; Random counts as many independent draws as there are unique rendered notes. New Shuffle order plays each unique rendered note once per cycle, reshuffling at the next cycle or when the note pool changes. Octave range participates in the pool; shared pitches on the same channel are deduplicated. No new panels.

> 0.2.147: Latch with Off now uses normal Latch replacement: new notes replace a released gesture, overlapping held keys join it, and repeating a latched input key removes it individually. Latch Acc. with Off retains accumulation. Clear Arp occupies the former Strum Spread knob; Strum Spread moves to Auto Chord’s free slot. There is no timed grouping window: held-key overlap defines a gesture.

> 0.2.146: Arp Hold adds Latch with Off. New input keys accumulate; pressing a latched key again removes it. Releases keep the remaining pool latched. Toggle identity uses the input key and channel, before harmony mapping or transpose; a chord-producing key toggles its whole generated chord. Existing Latch behavior is unchanged.

> 0.2.145: Next Harm places the linked Follower Buffer control beside Lookahead Anti Buffer, replacing Reset Learn. Both buffer locations edit the same saved setting and display its effective value.

> 0.2.144: enabled lookahead with a nonzero anti-buffer automatically uses and displays Follower Buffer = 0 ms. The saved buffer returns when lookahead is off or the anti-buffer is zero. Applies during learning too.

> 0.2.143: direct follower MIDI owns its held-note display once input begins; auxiliary monitor snapshots cannot add silent notes or resurrect releases. Held notes refresh output-role context when the harmony changes without changing pitch. Pair with Movy hbclean.56. The reported pad G/A-sharp behavior remains unconfirmed on hardware.

> 0.2.142: Harmony Flow presents conductor notes, detected harmony, lookahead-selected harmony and transposed rendered harmony as one row. Pair with Movy hbclean.54 for simultaneous updates.

> 0.2.141: coherent follower rows, cached split mapping, None travel, clearer root readouts and a single Foll Notes page before Operations. Pair with Movy hbclean.53 for simultaneous follower display updates at up to 25 Hz.

> 0.2.140: Foll Map now includes Approach, Reset, Scale Next and Chrom Next. The separate Foll Mod page is removed; Diagnostics stays last. Movy hbclean.52 remains compatible.

> 0.2.139: group settings by musical workflow, followed by note analysis, pad appearance and diagnostics.

> 0.2.138: simplify note analysis to the global conductor page and two per-track follower path pages. Movy hbclean.52 remains compatible.

> 0.2.137: new followers default to Scale; input roles stay anchored to the follower root; transposition applies consistently across travel modes. Next Harm adds an independent Lookahead Anti Buffer (25 ms by default). Foll Trk has two note-path pages. Pair with Movy hbclean.52 for fresh-set defaults and running-transport/deleted-set lifecycle fixes. See [follower paths and timing](docs/follower-paths.md).

> 0.2.136 fixes unnecessary idle scheduler work introduced with Ratchet/Echo in 0.2.133. Empty repeat schedulers now return immediately, and empty note-owner tables are skipped. Keep Movy hbclean.50. This addresses a measured performance regression; the reported stock transport symptom still needs device confirmation.

> 0.2.136 extends Auto and cycle conditions to Clip Repeat, Reverse, Time Shift and Speed when paired with Movy hbclean.50. Manual holds override the automatic schedule. See [operation controls](docs/operations.md).

> Release 0.2.136 adds sixteen assignable operation slots per track and host-aware clip controls. Pair with Movy hbclean.50 for Settings → Step Row → STEPS / PERFORM. See [operations](docs/operations.md).










# Harmony Bus

Experimental Schwung MIDI FX for Ableton Move.

Operation controls: [sixteen lanes with shared Operation, Timing and Conditions editors](docs/operations.md).
The new operations use a global Steps / Perform step-row mode, not knob touch. The published version below remains unchanged.

Harmony Bus uses a **Conductor / Follower / Receiver** model:

- a Conductor instance infers harmony from incoming notes and publishes it to a shared in-process bus;
- Receiver instances deliver already-rendered notes on their selected channel to the next instrument, without remapping or rebroadcasting;
- Follower instances keep their own rhythm/register, but remap their pitches against the current harmony;
- follower content and travel settings control which tones are allowed and how input notes map to them;
- follower source-root policies: **Infer Input**, **Infer Notes**, and **Explicit**.

## Install with Schwung Manager

In Schwung Manager use:

**Custom → From GitHub URL → `douglasmason/harmonybus`**

Manager reads `release.json` and downloads the release asset.

## Timing and current release

HarmonyBus **0.2.129** makes Follower Buffer global with a **1/16-note** default for
fresh settings. Millisecond choices are 0, 25, 50, then 100 to 1000 in 50 ms steps. The same control also offers tempo-relative durations from 1/64
to 2 Bars. Existing saved values survive; the first restored legacy copy becomes
the shared value. Change it once and save to make all snapshots agree.

Already-on-grid notes remain on-grid even when the buffer spans a full interval.
Lookahead remains Off by default. Use Movy **0.34.1-hbclean.25** for the new explicit
Quantize + Fill Gaps clip operation and prepared 1/16-note defaults.

Read the canonical [timing guide](docs/timing-guide.md), with editable
[SVG diagrams](docs/timing). CI generates the PDF from those sources and attaches
it to each release. The PDF is an export, not a separately maintained document.
Native tests cover timing, mapping, state restoration and complete module loading;
physical Move behavior still needs device verification.

To rebuild the guide (Linux with `fonts-dejavu-core` installed):

```bash
python3 -m pip install -r scripts/docs-requirements.txt
python3 scripts/build_timing_guide.py
```

## Build

```bash
./scripts/build_harmonybus_move.sh
```

The output is `dist/harmonybus-v0.2.142-module.tar.gz`.

## License

MIT.

HB 0.2.104 adds negative lookahead (late harmony) with the capture window before the shifted boundary, and resolves recognized two-note Movy voicings before due followers. Lookahead stays Off by default; new sets use a 1/16-note global buffer. See the [timing guide](https://github.com/douglasmason/harmonybus/blob/main/docs/timing-guide.md).

### Auto Chord and Arp / Strum (0.2.129)

Per-track Auto Chord provides Off, Scale Degree and Conductor Chord, with selectable power, triad, seventh, ninth, add9, sixth, 6/9, eleventh, thirteenth and suspended forms, key-selected or explicit inversion, and Close / Root + Fifth Low / Alternate Up / Shell voicings. Arp / Strum provides Together, Repeat Arp and Once; Momentary/Latch; five orders; musical rates and gate lengths; and ms or musical strum spreads. Both generators are inactive by default. Use Movy hbclean.31 to record and replay rendered conductor chords.

Generation happens after follower-buffer release, before arpeggiation. Generated gestures keep their initial voicing until released or replaced. Chord Mode Off with an active arp uses raw input pitches, plus transpose; existing Content/Travel/Approach mapping applies when both generators are inactive. See the [canonical guide](docs/timing-guide.md) for inversion examples, control locations, latch and cancellation semantics.

Dominant Scale (Off / Harmonic Minor / Melodic Minor / Altered V) substitutes the output collection on functional V or raised-vii diminished harmony relative to the configured tonic. It applies to ordinary follower scale mapping and generated extensions, while preserving source-degree labels and recognized chord tones. The guide details triggers, altered-scale spelling and held-gesture behavior.

### Follower scale baseline (0.2.170)

An explicit Follower Scale now supplies the rendering baseline as well as the input layout. Actual chord tones replace conflicting scale degrees; unrelated notes keep the follower collection. Infer retains harmony-based parent selection. Grey pad backgrounds remain derived from actual rendered scale membership, so this corrects the rendering and its preview together. Travel None preserves pitches.

All melodic-minor modes are selectable: Melodic Minor, Dorian b2, Lydian Augmented, Lydian Dominant, Mixolydian b6, Locrian #2 and Altered. Existing scale IDs and saved presets retain their meanings. Update Movy to hbclean.77 for the added layouts and saved input roles.

Borrowed Scale fits in the existing Foll Root panel beside Dominant Scale. It is global, defaults to Minimal, and offers Aeolian, Dorian and Mixolydian b6 for major-third follower contexts encountering bIII major, iv minor, bVI major or bVII major. Actual chord tones always win. It changes the output collection, not the physical input layout. Dominant Scale retains priority, including Altered V. This release does not infer secondary ii-V progressions or add new Travel modes; the input scale remains shared.

Dominant Scale and Borrowed Scale are shared across all tracks, like Follower Scale. New presets restore one shared choice; legacy per-track presets seed it from the first non-default dominant setting. Later stale track copies cannot override an edited or restored global choice.

### Progression-based Infer (0.2.170)

Infer now uses confirmed conductor transitions during learning and the full registered loop once learned. Each harmony contributes equally to scale membership scoring across all fifteen scales around the existing follower reference root. Ties retain the previous best choice; no evidence defaults to Major. Input layout, rendering and Used Scale share this baseline, independently of lookahead position. Explicit scales still take precedence, and chord alterations plus global borrowing/dominant overrides still apply to rendering.

The existing Foll Root panel's final slot is now Used Scale. A `?` marks equally scoring alternatives; `(default)` means no harmony evidence yet. `--` means the follower reference root is unresolved. Inferred Root and Used Root keep their root-policy meanings. This estimates a collection, not tonal function or modulation: it does not infer the tonic from a progression, inspect unplayed raw clip notes, or automatically identify secondary ii-Vs. The learned progression is available after playback observation, even with the lookahead offset off.

### Parallel-harmony regression coverage

`tests/parallel_harmony_test.c` compares Dm11–G9–Cmaj9–Am11 with parallel minor-11 and major-9 loops. It checks chord preservation, changing output collections, learned-loop lookahead, pad membership and chromatic approach mapping through the production mapper. The [timing guide](docs/timing-guide.md) explains the results and the distinction between parent-scale accommodation and independent per-chord modes. These tests do not add automatic per-chord mode selection.

0.2.203: Play Motif uses the existing performance lanes; user library edits are shared, stock edits create copies, and the lane editor allocates recordings automatically. Fixes the guide version gate for the pending display snapshot release.
