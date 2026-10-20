# Validation status — development build 0.1.1

This is not production acceptance. No claim of a delivered Windows binary, Cubase compatibility, or verified ARA integration is made by a source checkout.

## Automated DSP tests run in the Linux development environment
PASS at 44.1 and 48 kHz: sample/quarter conversion and tempo changes; 80 BPM eighth-note snapping; zero strength; straight/triplet/dotted modes; exact identity render; fixed output duration; stationary 220 Hz tone remaining within 2 Hz; coherent identical-channel stereo output; finite output and non-identity correction; marker serialization; manual movement safety; silence rejection; three synthetic syllable-like energy attacks detected within 55 ms; energy around corrected attacks; malformed tempo-map rejection.

The synthetic fixture is a harmonic tone with amplitude envelopes, not recorded Urdu. The frequency test uses zero crossings over a stationary tone, not a vocal pitch benchmark. Stereo difference tolerance is -30 dB relative energy; measured identical-channel difference is much smaller. Sample-by-sample finite checks are counted internally but do not represent independent test scenarios.

## Processor/editor validation
The internal smoke target checks host BPM/metre, worker completion, detection/render, Undo/Redo, source/marker save/restore, synchronized insert audition, captured sample count, editor construction and PNG rendering. See build logs for execution status. ARA code compilation does not test the actual host lifecycle.

## Windows CI gates
Visual Studio 2022 x64 Release compilation; DSP tests; processor/editor smoke; GUI Preview.png from the actual editor; module MZ/PE signatures, AMD64 machine type, PE32+ magic and DLL characteristic. Packaging refuses to proceed without successful smoke marker and preview. No Linux module is packaged. A successful CI package remains experimental even after these gates.

## Must be run manually in licensed Cubase Pro 15 on Windows 10/11 x64
- [ ] Confirm official Cubase 15 ARA Audio Extensions support, third-party factory discovery and permitted SDK licensing requirements.
- [ ] Install to Common Files/VST3; rescan; verify both mono/stereo loading and GUI scaling.
- [ ] Inspect real host BPM/PPQ/sample positions and 4/4, 3/4, 6/8; compare at 80 BPM and with stepped tempo changes/ramps.
- [ ] Record native Urdu Naat, Hamd, Manqabat and spoken phrases; annotate attacks, breaths, consonants and sustained vowels with native speakers. Measure precision/recall and timing error. No such corpus was provided/evaluated.
- [ ] Test every quantize grid, swing/offset, tolerance, strength, humanize and safety limit; verify selected-only behavior and noncrossing drag.
- [ ] Listen for sibilant smearing, doubled attacks, phasing, clicks and emotional timing changes; measure pitch/formants on real vocals.
- [ ] Verify timeline-aligned insert audition; bypass, seek, looping, capture limits, tempo changes and silent regions; mono/stereo 44.1/48 kHz; 24-bit WAV export and reimport.
- [ ] Use the actual Cubase ARA event workflow; select/trim/move/duplicate events; select tracks; reopen editors; switch sample rates; revoke source access; verify fallback/no unintended silence.
- [ ] Verify ARA playback, offline host export, source changes and content notifications, host Undo/Redo and multiple instances. Confirm a shared modification's expected scope.
- [ ] Save/reopen complete Cubase projects with the editor closed. Compare rendered audio, source references and attack samples before/after. Test saving during background processing (last committed state is saved).
- [ ] Run official VST3 validator/pluginval against the actual module. They have not been substituted by the internal smoke target.
- [ ] Exercise installer missing/invalid bundle, permission denied, copy error, existing destination, normal install and uninstall.

Release acceptance remains blocked until the unchecked host/audio tests pass. Report Windows build execution separately from ARA and Cubase acceptance.

## 0.1.1 regression coverage and host retest
Automated additions cover default 100% BPM targets with low confidence, non-zero project/PPQ origins at 44.1/48 kHz, exact off-grid reference matching, stereo click onset samples, rejection of impossible exact matches, actual undo/redo buffer restoration, manual edit history/redo branching, reference-file import anchoring and saved reference persistence. These checks do not establish Cubase acceptance.

Retest in Cubase: import vocal and rendered click with their correct cursor origins; inspect original/corrected/reference lanes together; match selected attacks to off-grid click transients; verify an impossible exact match keeps the previous result; listen through repeated Undo/Redo and original listening; save/reopen. Test host metadata changes without losing edit history. Grid alignment reports safety-limited attacks instead of claiming they reached the grid.
