# Live Chord Play — 0.2.294

Chord Play quality hues now use [implied harmony quality](implied-quality.md):
the resolved chord scale viewed from the chord root, independently of the
selected voicing. The expressed chord name and notes remain available unchanged.

Manual chord selection replaces current harmony without teaching it into the recorded conductor timeline. Recorded next harmony remains available for anticipation while playing. At the next recorded harmonic boundary the live override expires. A stopped manual chord has no speculative lookahead. Harmony Off releases the live selection; selecting the same chord again still retriggers it.

Chord defaults resolve in the shared parent key: seven home-scale degrees, seven degrees from the opposite major/natural-minor collection, and seven dominant sevenths on the natural-minor degrees, including flat III, VI and VII. Resolution stays native and uses the same key context as Motif scale operations. The bank snapshot accepts both legacy sixteen-slot banks and the new twenty-one-slot banks.

Conductor voicing, arp, rhythm and Render routing still own playback. Selecting harmony does not enable recording. Recorded descriptors retain their exact tones and scale; a manual audition does not mutate clip data or learned next harmony.

Validation: native bank resolution in major and minor, current/next separation, stopped previews, Harmony Off, existing conductor MIDI/arp scheduling, and the full native regression suite.


Queued Chord Play (0.2.295)
-------------------------
The native live-conductor timeline holds one current chord and up to sixteen
future selections. The first selection becomes current; subsequent selections
append snapshots. Movy activates each snapshot through the existing live input
and recording path. Enqueueing is not an early note-on or an early clip edit.
The queue is transient performance state and is not saved with a Set.

Chord Grid and Chord Anticipation determine activation boundaries. Observed uses
proven recorded chord boundaries, with a one-bar fallback while none are known.
Timing edits re-arm pending entries from the new boundary. Stopped preloading
retains the current chord and waits for transport before advancing. Stop, Off,
a Set change, conductor destruction/role changes, or a backward transport seek
cancel the queue. A full queue rejects new entries without dropping old ones.

An empty queue holds its last chord indefinitely. A later selection waits for
the next boundary; no silent placeholder is inserted. Repeated chords remain
separate attacks. Recorded onsets do not replace this queued live authority.
The recorded/learned timeline itself is never overwritten by live selections.

Followers, pad previews, Next operations, follower capture and motif arrival
read the same queued forecast, including existing early, late, Immediate and
After lookahead settings. Motif Next Chord arrival requires an actual pending
chord; an exhausted queue does not manufacture a target. Conductor motif-to-clip
loading remains the existing explicit preview/write/append/overdub operation.
Queueing a chord does not automatically insert a motif or rewrite a clip.

The native queue uses fixed capacity storage and the host's existing musical
clock. There is no UI timing loop, new scheduler thread or audio-time loader.
