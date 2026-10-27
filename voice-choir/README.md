# NOORI Voice Choir 0.1.0

Experimental own-voice ensemble effect for Windows x64, VST3. Intended for Cubase Pro 15; actual Cubase compatibility and listening acceptance remain to be tested.

This is an independent conventional ensemble processor, not I AM CHOIR, a neural voice model, or a recreation of its choir recordings. It layers your incoming vocal with small pitch and timing differences. It does not turn your voice into distinct recorded singers. Works with any recorded language; no lyric recognition is involved.

## Install
Close Cubase. Run Install.bat (administrator permission is required to copy into the shared VST3 directory), or manually copy the entire `NOORI Voice Choir.vst3` folder into `C:\Program Files\Common Files\VST3`. Reopen Cubase and rescan VST3 plugins if necessary.

## Use
Insert NOORI Voice Choir on a vocal track. Press Play in Cubase. Choose Full ensemble, then adjust Choir blend. A mono-to-stereo or stereo track allows stereo spread; a mono output sums the ensemble. The wet ensemble uses the centred input vocal; the dry path retains original stereo.

- Voices: 2–16 layers.
- Pitch spread: 0–20 cents; paired around your original pitch.
- Timing spread: 0–30 ms additional layer variation.
- Stereo width: centred to wide.
- Choir blend: original to ensemble.
- Output: -24 to +6 dB; leave headroom.

Uses dual overlapping variable-delay grains and no feedback. It does not progressively stretch words, but layering can soften consonants and produce chorus/comb-filter coloration. Lower pitch/timing spread or blend for clearer words. Small ensemble is the gentlest preset. There is no formant modelling, octave generator, key/scale harmony, or AI choir generation in this version.

The fixed 40 ms nominal latency is reported to the host for delay compensation; layer variations remain intentional. Use Cubase Render in Place to print the processed audio, keeping the original if desired. Do not use this insert as an ARA extension.

## Validation and license
Automated engine tests check delayed dry impulses, bounded stereo output, silence/reset, mono routing, sample-rate coverage, block-size invariance, and finite tails. Windows tests also check processor state, routing, presets and real editor rendering. Cubase listening and host acceptance are pending.

AGPL-3.0; see LICENSE and bundled dependency notices. Complete corresponding source and build scripts: https://github.com/sagemadinah1292-hub/noori-/tree/codex/voice-choir/voice-choir . Pin the commit in BUILD-INFO.json to obtain the exact version. No user audio is included.
