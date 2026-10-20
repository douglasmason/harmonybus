# Schwung Manager category

Harmony Bus belongs under **MIDI FX → Chord & Harmony**.

The proposed entry in `schwung-catalog-entry.json` is ready to add to the
`modules` array in `charlesvestal/schwung/module-catalog.json`.
It has not yet been submitted or merged upstream.

Manager reads the subcategory from that central catalog. Its installed-module
reader supports `capabilities.component_type` (already `midi_fx` in Harmony Bus),
but does not read a custom module's subcategory. Adding a field to our module
alone therefore would not change its Manager classification.

The normal update URL comes from this repository's `release.json`.
The catalog asset name is the current release's fallback filename.
No DSP or Movy update is needed for the catalog change. It takes effect once
the upstream entry is merged and Manager refreshes its catalog.
