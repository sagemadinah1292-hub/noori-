# NOORI TURAB TEMPO
**Preserve the Emotion. Perfect the Timing.**

Experimental C++20/JUCE Windows x64 VST3 vocal attack editor, intended for Cubase Pro 15. Version 0.1.0 is a development build, not a validated production release. See [VALIDATION.md](VALIDATION.md) before using it in a session. A source archive is not a Windows installation package.

## Implemented
- Acoustic attack detection: Hann-window FFT positive spectral flux, RMS rise, zero crossings, normalised autocorrelation, envelope backtracking, adaptive peak threshold and refractory interval.
- Source and corrected waveform, sample-position markers with selection and constrained dragging, project beat ruler, grid, scroll, zoom and overview. Dark purple/charcoal JUCE editor uses original drawing code.
- Host BPM, PPQ, sample position and time signature; capture records tempo changes at block boundaries. Quarter/eighth/sixteenth/thirty-second grids, straight/triplet/dotted rhythm, swing, offset, tolerance, strength and deterministic humanize.
- Piecewise monotonic time warp with fixed endpoints and bounded local stretch. Signalsmith Stretch at unity pitch, compensated input/output latency, shared stereo processing, and crossfaded unity-rate islands around protected consonants.
- Naat/voice heuristics, emotional confidence weighting, breath/noise rejection and sustained-voicing suppression. These controls influence analysis or timing; they are not trained Urdu classifiers.
- Worker-thread analysis/rendering, cancellation in the session API, immutable audio snapshots for realtime playback, Undo/Redo, original comparison, float-accurate source/marker persistence, mono/stereo and 24-bit WAV export.
- Official ARA SDK 2.1.0/JUCE factory, source reading, playback renderer, selected region binding, tempo content reading and per-modification archives. Compiling this code does **not** verify Cubase 15 ARA support or correct host integration.

Pitch tab is disabled because pitch editing is not implemented. Level tab displays RMS amplitude; it does not edit gain. No pitch or phoneme recognition is claimed. No proprietary RePitch assets/code are used.

## Working insert workflow
1. Put the VST3 on a mono/stereo vocal track. Set Cubase to 44.1 or 48 kHz; default test tempo is 80 BPM.
2. Press **Capture**, play the vocal through the insert, then **Finish capture**. Capture stops adding data on a seek/loop, at 120 seconds, or on tempo capacity exhaustion. It never pretends to read arbitrary DAW events.
3. Alternatively place the transport at the source event's start and **Import vocal** (WAV/AIFF/FLAC). File sample rate must match the project. File import anchors to the current transport and current BPM; it cannot query an entire Cubase tempo track. Use capture for changing tempos.
4. Press **Detect attacks** to inspect markers or **ALIGN ALL ATTACKS TO GRID** to analyse and render in one step. Drag markers; shift-click adds selections. **Align selected attacks** renders selected adjustments. Light ghost lines show the original attack positions.
5. Enable **Audition with Cubase transport** and play the same region. This replaces the insert output with the cached correction during playback, aligned to project samples. Outside the loaded region it outputs silence while audition is enabled. Disable audition to pass the track through. **Compare original** switches the audition buffer.
6. **Export WAV** writes a 24-bit corrected file. Keep the source and review pronunciation, breaths, vowels and artefacts by ear. Save the Cubase project to persist input and edits.

## ARA workflow — experimental, host acceptance pending
If Cubase Pro 15 exposes this factory as an Audio Extension, use its supported event extension command and select a vocal event. The editor binds to the first selected ARA playback region. Sources are read through `ARAAudioSourceReader`, never through insert assumptions. All modifications preload through background workers, including when the editor is closed. Play Cubase to audition; no separate stopped-transport ARA preview renderer is implemented.

Only unscaled ARA regions are supported; the factory does not advertise host time-stretch transformations. Pre-existing host-stretched regions are skipped by the renderer. Shared audio modifications share the same source-domain warp. Trimmed regions retain full-source marker coordinates. Multiple selected events are not batch edited. Moving a region updates its quantization origin; already rendered markers are not automatically requantized. Save and reopen uses the ARA host source and archived edits. Original-source fallback is buffered at matching sample rate while loading; differing-rate sources can be silent until preloading completes. Cached differing-rate playback currently uses linear sample-rate interpolation, not a mastering resampler.

Official Steinberg documentation could not be retrieved in the build environment (HTTP 403). Consequently this project does not assert official Cubase Pro 15 compatibility. See [docs/ARA.md](docs/ARA.md) and run every host test in VALIDATION.md.

## Build Windows x64 (Visual Studio 2022)
Install Visual Studio 2022 Desktop development with C++, Windows SDK, CMake >=3.24 and Git. Then:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DNOORI_ENABLE_ARA=ON -DBUILD_TESTING=ON
cmake --build build --config Release --target NooriTurabTempo_VST3 noori_tests noori_plugin_smoke --parallel 2
ctest --test-dir build -C Release --output-on-failure
./scripts/Package.ps1
```

The workflow at `.github/workflows/windows.yml` runs the same steps on `windows-2022` on push, PR or manual dispatch. It uploads `Noori-Turab-Tempo-Windows-x64.zip` only after successful compilation, tests, GUI preview generation and PE32+ AMD64 DLL validation. No Windows plugin is fabricated if that job cannot run. The build fetches pinned JUCE 8.0.12, Signalsmith Stretch and Linear 0.6.4, and official ARA 2.1.0 sources. JUCE includes the Steinberg VST3 headers.

The generated package contains the genuine `.vst3` bundle, Install.bat, Verify-PE.ps1, README.md, BUILD-INFO.json, VALIDATION.md, a screenshot of the actual editor and license notices. Close Cubase; run Install.bat as Administrator. Default destination is `C:\Program Files\Common Files\VST3\NooriTurabTempo.vst3`. Existing bundles must be moved aside manually; installer refuses to overwrite. Uninstall by closing Cubase and removing that one bundle.

## DSP-only developer tests
```sh
cmake -S . -B build-dsp -DNOORI_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-dsp --parallel 2
ctest --test-dir build-dsp --output-on-failure
```
This compiles a test executable and DSP library, not a Linux plugin or a product standalone. On non-Windows, plugin generation is rejected unless explicitly using the internal `NOORI_COMPILE_CHECK=ON` option (shared-code validation only). Internal smoke-test binaries are never packaged.

## Limits and engineering notes
- No trained Urdu model, native-speaker corpus evaluation, objective formant evaluation, or guaranteed syllable/breath classification. The examples in the brief are intended evaluation material, not recognition claims.
- No dedicated formant modification. Unity-pitch spectral stretching aims to preserve timbre; sustained-vowel and consonant controls are heuristics, not guarantees. Test pitch checks use stationary synthetic tones and do not establish vocal quality.
- Maximum decoded source size 128 MiB; realtime capture preallocates 120 seconds. A session retains immutable revisions until destruction, with a 512 MiB budget. This avoids freeing buffers on the realtime thread. Reaching the limit requires a new instance. Very long sources are rejected. Uncompressed insert project state can be large.
- Analysis/rendering are offline workers. File dialogs and WAV export/state serialization are non-realtime operations. Starting and finishing insert capture temporarily suspend processing; imported/rendered buffers are swapped only after a completed job.
- Capture derives piecewise constant tempos from block metadata; it cannot recover tempo changes within a host block. ARA tempo entries are converted to piecewise sample/quarter segments; continuous tempo ramps between entries are not modelled. Time signatures are displayed from transport; note divisions use quarter-note PPQ independently of metre.
- Sample Accurate off rounds movement to approximately 1 ms; on uses integer sample positions. Humanize is repeatable and constrained. Strength zero is an identity warp. Warp limits can prevent a marker from reaching the requested grid line; no crossing is allowed.
- Changes during a worker job are disabled. Saving during a job saves the last committed revision, not an unfinished render. Undo history is session-local; reopening restores the committed correction, not the undo stack.
- This is unsigned development software. VST3 SDK validator/pluginval and Cubase host validation are not replaced by the internal smoke test.

## License
Original source is GPL-3.0-only (LICENSE), using JUCE under its GPLv3 option. Commercial redistribution under other terms needs the appropriate JUCE agreement and rights to this source. Signalsmith Stretch/Linear are MIT; ARA API/library are Apache-2.0; bundled VST3 notices are retained in `licenses/`. No rights to RePitch or Steinberg/Celemony trademarks are granted. See [licenses/THIRD-PARTY.md](licenses/THIRD-PARTY.md).
