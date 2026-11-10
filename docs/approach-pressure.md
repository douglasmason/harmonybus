# Approach pressure — HarmonyBus 0.2.314

Pair with Movy hbclean.337 for the shared Approach Pressure panel in Normal Play
and Motif Play. Four saved assignments address triple approach rows 1–3 and the
single approach row. Each assignment selects an existing operation slot, pressure
mode (Off, Momentary, Tap Latch, Peak Latch), Reverse and prepared tap amount.
All default to Off. The Movy panel supplies inline metadata to stay within the
host's module-metadata buffer; the stock HarmonyBus module hierarchy is unchanged.

Momentary releases with the pad. Tap Latch uses the prepared amount, with temporary
pressure variation on a squeeze. Peak Latch begins a fresh peak at every press,
retains it on release and permits a new lower peak. A short light tap turns it off.
Reverse inverts active pressure, so Reverse Peak retains the minimum output.
Pressure zero does not release a held pad. Stop and state reload clear runtime
pressure without erasing its settings.

Velocity, Gate, Pan, Transpose, Ratchet and Echo support pressure amounts. This
reuses lane evaluation and existing Field Amount bounds. Discrete operations stay
discrete, and existing scheduling determines when new amounts take effect; this
is not continuous audio-parameter modulation. Notes retain their normal route.
Mapped polyphonic pressure is consumed so it cannot also control arp velocity.
The maximum pressure of held pads controls a row; the newest active row wins if
two assignments address one lane. The state is bounded and allocation-free.

The `ap1` state extension saves assignments only. Older states restore pressure
Off. Native tests exercise lower replacement peaks, inverse peaks, taps, multiple
contacts, zero pressure, actual MIDI velocity, Stop, unsupported operations and
state restoration. No physical pressure calibration or audio play test is claimed.

See the [Movy pressure guide](https://github.com/douglasmason/harmonybus-movy/blob/main/docs/pressure-performance.md)
for controls, grid pressure editing and remaining synth/pitch routing work.
