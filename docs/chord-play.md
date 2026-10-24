# Chord Play conductor output — 0.2.293

Chord selections now feed the conductor’s existing chord player and MIDI Render destination. Track 1 can therefore send the selected harmony to original Schwung track 3 using its existing routing. Conductor voicing, arp, and rhythm settings govern the output. Render Off keeps selections silent for setups with separately triggered followers.

The selection descriptor remains the only recorded input; rendered voices are not recorded a second time. Movy requires explicit Record and replaces the conductor’s recorded interval. Selecting or releasing harmony alone does not arm recording.

Validation includes literal and functional harmony, MIDI channel 3 output, arp scheduling, note-off pairing, and the full native suite.
