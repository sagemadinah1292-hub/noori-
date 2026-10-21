# NOORI TURAB TEMPO
**Preserve the Emotion. Perfect the Timing.**

Experimental C++20/JUCE Windows x64 VST3 vocal attack editor, intended for Cubase Pro 15. Version 0.3.0 is a development build, not a validated production release. See [VALIDATION.md](VALIDATION.md) before using it in a session. A source archive is not a Windows installation package.

## 0.3.0: complete edit history, themes and timing review
Use **Audio Extension / ARA**, not an ordinary track insert, for project-track dropdowns. Assign this extension to the vocal and to the reference track's audio events. A built-in Cubase metronome or MIDI track has no audio source for the plugin to read: render it to an audio track first. The dropdown can list only tracks/events Cubase exposes to this plugin's ARA document. Native Cubase factory discovery and access still require verification on the user's machine.

Choose the **Edit** track at the top left and the reference project track in the right dropdown. Choose **Reference: metronome clicks** or **Reference: vocal attacks**. Event placement, source trims and differing source sample rates are accounted for. **Quantize selected track** is active when its source audio is loaded and does not require marker selection; it processes independent event sessions on that track. Use the Preview event dropdown to review each event. Shared modifications at different placements/crops are rejected with instructions to make independent copies.

Choose **DAW bar/beat | BPM grid** for tempo quantization. **BPM grid | exact** forces 100% strength, sample accuracy and zero tolerance/humanize/offset/swing. It validates all selected targets together and never silently clamps them off-grid. Collisions/stretch-limit failures preserve the previous result; choose a finer grid, a smaller attack selection or a higher stretch limit. Targets at the fixed audio-file endpoints cannot be reached by an interior marker; those edge attacks remain unchanged and are counted in status. **BPM grid | natural** retains adjustable strength, timing protection and bounded movement. The ruler shows bar:beat in the current time signature; grid mathematics uses quarter-note PPQ. Full historical time-signature maps are not modelled.

**Align selected attacks** is a separate action. Detect attacks, then use **Select all attacks**, click markers, drag a horizontal range, or press Ctrl+A while the editor has keyboard focus. Shift adds markers/ranges; Escape clears. The button displays the selection count. Only corrected markers can be dragged; clicking original markers selects them too.

An ordinary insert cannot enumerate arbitrary DAW tracks. It can reference **another connected NOORI insert**: put NOORI on the click track too, press Capture, play the reference through it, and Finish capture. Keep that instance open. Cubase's track name then appears with “(connected insert)” in the vocal instance's reference dropdown. Select it and quantize; capture origins determine exact project alignment. Connected instances share references within the same plugin process, so separately sandboxed plugins cannot communicate this way. Captured reference data and its track identity are saved with instance state. ARA is required for a broader host-provided track list. Capture, vocal/reference-file import and an explicit explanation for unavailable tracks remain available. Host BPM/PPQ/sample position is read consistently; when unavailable, enter the manual BPM shown above the waveform, anchored to project sample zero. Import a file at its project start; **Audio start = cursor** corrects its placement when host timing is available.

See [the screenshot feature comparison](docs/FEATURE-CHECK.md) for what remains missing. ProjectPreview.png is the real editor bound to the automated in-process ARA test host; Preview.png shows insert mode. Neither image establishes native Cubase acceptance.

## Implemented
- Acoustic attack detection: Hann-window FFT positive spectral flux, RMS rise, zero crossings, normalised autocorrelation, envelope backtracking, adaptive peak threshold and refractory interval.
- Source and corrected waveform, sample-position markers with selection and constrained dragging, project beat ruler, grid, scroll, zoom and overview. Dark purple/charcoal JUCE editor uses original drawing code.
- Host BPM, PPQ, sample position and time signature; capture records tempo changes at block boundaries. Quarter/eighth/sixteenth/thirty-second grids, straight/triplet/dotted rhythm, swing, offset, tolerance, strength and deterministic humanize.
- Piecewise monotonic time warp with fixed endpoints and bounded local stretch. Signalsmith Stretch at unity pitch, compensated input/output latency, shared stereo processing, and crossfaded unity-rate islands around protected consonants.
- Naat/voice heuristics, emotional confidence weighting, breath/noise rejection and sustained-voicing suppression. These controls influence analysis or timing; they are not trained Urdu classifiers.
- Worker-thread analysis/rendering, cancellation in the session API, immutable audio snapshots for realtime playback, Undo/Redo, original comparison, float-accurate source/marker persistence, mono/stereo and 24-bit WAV export.
- Official ARA SDK 2.2.0/JUCE factory, source reading, playback renderer, selected region binding, tempo content reading and per-modification archives. Compiling this code does **not** verify Cubase 15 ARA support or correct host integration.

Pitch tab is disabled because pitch editing is not implemented. Level tab displays RMS amplitude; it does not edit gain. No pitch or phoneme recognition is claimed. No proprietary RePitch assets/code are used.

## Working insert workflow
1. Put the VST3 on a mono/stereo vocal track. Set Cubase to 44.1 or 48 kHz; default test tempo is 80 BPM.
2. Press **Capture**, play the vocal through the insert, then **Finish capture**. Capture stops adding data on a seek/loop, at 120 seconds, or on tempo capacity exhaustion. It never pretends to read arbitrary DAW events.
3. Alternatively place the transport at the source event's start and **Import vocal** (WAV/AIFF/FLAC). File sample rate must match the project. File import anchors to the current transport and current BPM; it cannot query an entire Cubase tempo track. Use capture for changing tempos.
4. Press **Detect attacks** to inspect markers or **Quantize selected track** to analyse and render in one step. Drag markers; shift-click adds selections. **Align selected attacks** renders selected adjustments. Original and corrected audio and attacks stay visible in separate labelled lanes. Only corrected markers can be dragged.
5. **Align** enables **Audition with Cubase transport** automatically. Undo/redo restores the listening state captured with each action. Play the same region. This replaces the insert output with the cached correction during playback, aligned to project samples. Outside the loaded region it outputs silence while audition is enabled. Disable audition to pass the track through. **Listen original** switches only the audition buffer; both visual lanes remain visible. Undo/redo also restores original/result comparison. Buttons and Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y operate the plugin history when its editor has keyboard focus. Cubase may intercept shortcuts, so the buttons remain available.
6. **Export WAV** writes a 24-bit corrected file. Keep the source and review pronunciation, breaths, vowels and artefacts by ear. Save the Cubase project to persist input and edits.

## Timing and metronome reference (0.1.1)
Default strength is 100%, tolerance and humanize are zero, and confidence weighting no longer softens a 100% move. Sample-accurate targets use integer samples. A constant host tempo is refreshed when pressing Align, rather than continuously changing the displayed grid during playback. Captured/ARA tempo maps retain their multiple tempo segments. If BPM targets are limited by the local stretch bound, the status reports how many attacks could not reach the grid. Increase the stretch limit under Settings or align a smaller selection; review audio quality. Natural Naat remains an optional softer preset.

To match the **actual metronome audio**, render the Cubase click track to WAV (mono/stereo, same sample rate as the vocal). Place the transport at the rendered file's exact start, then press **Import metronome click**. Importing vocal and reference files from a common project-start export with silence preserved is the simplest way to keep their origins consistent. The reference import anchors independently to the current cursor, so files with different starts also work when each start is set correctly. The green reference lane displays detected clicks; choose **Match reference clicks (exact)** and Align all or selected attacks.

Click detection uses the first sample crossing 4% of the file's peak after at least 25 ms of quiet. It is intended for a clean isolated metronome, with no music or background noise. Exact means matching these detected onset coordinates, not estimating a BPM from the recording. Reference mode forces 100% strength, sample accuracy, zero tolerance and zero humanize. Each attack chooses its nearest reference click; it is not a one-to-one musical phrase matcher. If multiple attacks compete for one click or a target exceeds stretch safety, the entire operation is rejected and the previous audio/history stays intact. Use a finer metronome subdivision or select the attacks to match. Reference clicks and their project alignment persist in saved state; the reference is visual/timing guidance and is not mixed into the vocal export. An ordinary insert imports a rendered click file; it cannot select an arbitrary Cubase track directly.

## ARA workflow — experimental, host acceptance pending
If Cubase Pro 15 exposes this factory as an Audio Extension, use its supported event extension command and select a vocal event. By default the editor follows the first selected ARA region; the Edit track and Preview event dropdowns allow an explicit choice. Sources are read through `ARAAudioSourceReader`, never through insert assumptions. All modifications preload through background workers, including when the editor is closed. Play Cubase to audition; no separate stopped-transport ARA preview renderer is implemented.

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

The workflow at `.github/workflows/windows.yml` runs the same steps on `windows-2022` on push, PR or manual dispatch. It uploads `Noori-Turab-Tempo-Windows-x64.zip` only after successful compilation, tests, GUI preview generation and PE32+ AMD64 DLL validation. No Windows plugin is fabricated if that job cannot run. The build fetches pinned JUCE 8.0.12, Signalsmith Stretch and Linear 0.6.4, and official ARA 2.2.0 sources. JUCE includes the Steinberg VST3 headers.

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
- Sample Accurate off rounds movement to approximately 1 ms; on uses integer sample positions. Humanize is repeatable and constrained. Strength zero is an identity warp. Natural-mode warp limits can prevent a marker reaching the requested grid line; exact mode rejects an unsafe/colliding target set. No crossing is allowed.
- Changes during a worker job are disabled. Saving during a job saves the last committed revision, not an unfinished render. Undo history is shared across edit sessions visited by this plugin instance; reopening restores the committed correction, not the undo stack.
- This is unsigned development software. VST3 SDK validator/pluginval and Cubase host validation are not replaced by the internal smoke test.

## License
Original source is GPL-3.0-only (LICENSE), using JUCE under its GPLv3 option. Commercial redistribution under other terms needs the appropriate JUCE agreement and rights to this source. Signalsmith Stretch/Linear are MIT; ARA API/library are Apache-2.0; bundled VST3 notices are retained in `licenses/`. No rights to RePitch or Steinberg/Celemony trademarks are granted. See [licenses/THIRD-PARTY.md](licenses/THIRD-PARTY.md).

See [0.3.0 changes and undo scope](docs/UPDATE-0.3.md) for light/dark themes, reference waveform overlay, manual attack editing and expanded history.
