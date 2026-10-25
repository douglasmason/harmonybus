# Live Chord Play — 0.2.294

Manual chord selection replaces current harmony without teaching it into the recorded conductor timeline. Recorded next harmony remains available for anticipation while playing. At the next recorded harmonic boundary the live override expires. A stopped manual chord has no speculative lookahead. Harmony Off releases the live selection; selecting the same chord again still retriggers it.

Chord defaults resolve in the shared parent key: seven home-scale degrees, seven degrees from the opposite major/natural-minor collection, and seven dominant sevenths on the natural-minor degrees, including flat III, VI and VII. Resolution stays native and uses the same key context as Motif scale operations. The bank snapshot accepts both legacy sixteen-slot banks and the new twenty-one-slot banks.

Conductor voicing, arp, rhythm and Render routing still own playback. Selecting harmony does not enable recording. Recorded descriptors retain their exact tones and scale; a manual audition does not mutate clip data or learned next harmony.

Validation: native bank resolution in major and minor, current/next separation, stopped previews, Harmony Off, existing conductor MIDI/arp scheduling, and the full native regression suite.
