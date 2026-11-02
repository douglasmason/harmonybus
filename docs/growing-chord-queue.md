# HarmonyBus 0.2.311 — growing retained chord queues

Install this HarmonyBus update together with Movy 0.34.1-hbclean.313.
The queue engine runs in HarmonyBus; updating only Movy does not remove the
old sixteen-chord limit.

Chord Play queues now grow as memory permits. Hold refills append to the
existing progression, and Stop / Play restarts all retained chords. Mute,
Immediate / Queued changes and Hold / Loop changes preserve the progression;
X or Range Tap Off explicitly clears it. New sessions default to Queued and
Loop; saved preferences remain respected.

Playback uses a bounded window of 65 future deadlines, and the footer reads
at most 32 chord names with the correct absolute cursor and total. Growth
failure rejects the new chord while preserving the existing progression.
Ordinary playback, looping and status reads do not allocate queue storage.

Regression coverage includes 4,096 queued activations, allocation failure,
Hold refills, a 4,099-chord replay and retrigger, and production-chain tests
of 64 chords followed by a retained refill and a 66-chord restart with
balanced MIDI on Schwung 1.7.3 and 1.8.2.

Explicit enqueue growth can allocate on the host audio-service thread.
Physical Move crackling and missed transitions still need hardware
verification. Publishing these packages does not install them on the Move.
