# NOORI Double Delay — 0.1.0 experimental build

Independent Windows x64 VST3 dual delay, intended for Cubase Pro 15. This is a new implementation inspired by the documented DoubleDelay workflow. It is not Steinberg's original plugin, a binary conversion, or a validated sound-identical replacement.

## Install

Close Cubase. Extract the downloaded artifact ZIP and then the package ZIP inside it. Run Install.bat as administrator. Alternatively copy the whole `NOORI Double Delay.vst3` folder to `C:\Program Files\Common Files\VST3\`. Reopen Cubase, rescan if necessary and select NOORI Double Delay under Delay. No jBridge is needed. This does not replace or change the original DoubleDelay or NOORI TURAB TEMPO.

## Controls

- Two independently timed delays, with separate stereo positions.
- Each delay switches between milliseconds (0–9999 ms) and tempo sync.
- Synced notes cover whole through 32nd notes, with straight, triplet and dotted timing, multiplied by 1–10.
- Feedback sets both independent repeat loops, from 0–100%.
- Mix blends dry and wet, 0–100%; use 100% on an FX send.
- The graph handles change time horizontally and pan vertically. With sync enabled, horizontal dragging changes the multiplier.
- Cubase BPM is used when supplied by the host. Fallback BPM is editable when host tempo is unavailable.
- Clear echoes empties both buffers while preserving controls. All sound controls support host automation; settings are saved with the project. Echo buffers are not saved.

Default is two 500 ms delays, 50% feedback, hard left/right and 50% mix. The Photo preset recreates the settings visible in the supplied screenshot: dotted eighth x2 on the left, quarter x3 on the right, 50% feedback and mix. At 120 BPM, those delays are 750 and 1500 ms.

## Audio design and limits to verify against the original

The stereo input is averaged into two mono delay loops, then each return is panned linearly. The dry stereo image is preserved. A shared feedback control drives independent loops; there is no cross-feedback. Mixing is linear. These routing, gain and panning choices are implementation assumptions that require comparison with the working original.

Maximum effective delay is 10 seconds in either mode; the editor displays a warning if a synchronized note exceeds it. The actual computed value is shown in the graph, even when limited. A nominal 0 ms free delay uses one sample for a causal feedback loop. Delay changes crossfade over 20 ms; feedback, pan and mix changes are smoothed. The plugin adds no reported latency, uses preallocated buffers and does not allocate memory in the audio callback. Extremely overloaded feedback is bounded at +/-64 internally; this is not a normal-level limiter. At 100% feedback the repeats can sustain. Clear echoes to stop them. Tail reporting is finite and long-feedback renders may require an explicit longer export tail.

The supplied 32-bit DLL identifies itself as DoubleDelay 1.2.1, Nuendo/Houpert Digital Audio (2004). A limited isolated execution probe reached a message requiring Nuendo 2.x, so no reference DSP measurements were obtained. The original DLL, artwork and binary code are not included in this source or package. Old VST2 presets/projects cannot be imported automatically.

Cubase 15 loading, original-plugin null tests, original gain/pan/feedback/automation behavior and CPU performance equivalence are NOT verified. Test on a copy of a session. No claim of bit-identical audio or matching CPU use is made.

## Build and validation

Source license: GPL-3.0, with JUCE's applicable GPL/commercial licensing terms. JUCE is pinned to commit 29396c22c93392d6738e021b83196283d6e4d850.

Windows: `cmake -S double-delay -B build-delay -G "Visual Studio 17 2022" -A x64`, then `cmake --build build-delay --config Release --parallel 2`, then `ctest --test-dir build-delay -C Release --output-on-failure`. The Double Delay Windows workflow packages only after tests and AMD64 verification pass.

Linux DSP check: `cmake -S double-delay -B build-delay -DDELAY_BUILD_PLUGIN=OFF`, then build and run CTest. DSP tests cover exact impulse placement, feedback gain, timing combinations, panning, dry identity, reset, mono and stereo, block-size invariance and rapid delay automation at common sample rates. Windows processor/editor tests exercise host tempo, parameter/state restoration, fallback tempo, limits, clear and actual GUI painting.

Reference control descriptions: https://support.steinberg.de/downloads_software/Cubase_SE_3/Docs_English/Audio_Effects_and_VSTi.pdf
