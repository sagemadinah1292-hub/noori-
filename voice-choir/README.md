# NOORI CHOIR 0.3.0 — reference workflow development build

Native Windows x64 C++20/JUCE VST3, intended for Cubase Pro 15. No ARA. This
revision replaces the six-knob chorus interface with a tall teal/gold workflow:
Import → Choir Setup → Processing → Results, using the supplied NOORI SVG.

## Workflow

1. Browse/drop a WAV or AIFF, or click **CAPTURE FROM TRACK**, play the vocal in
   Cubase, then **STOP CAPTURE**. Source waveform, playback and mute are available.
   Capture has a 120-second limit; pause/seek discontinuities are captured exactly
   as received, so record a continuous section. Direct Cubase audio-event drops
   may not provide ordinary file paths; file browse and track capture are fallbacks.
2. Choose the number of singers, DSP choir character, microphone tone, key/scale,
   octaves, thirds and fifths. Main plus selected musical parts occupy the first
   slots; remaining singers are unison variations. Reduce selected parts if they
   exceed the number of singers. Key is manual. Scales: major, natural minor,
   harmonic minor and ascending melodic minor.
3. **CREATE VOCALS** analyzes pitch and renders separate formant-compensated
   musical parts. Progress reflects work. Cancellation keeps the preceding result.
4. Audition source or generated voices through the plugin's output in the host.
   Results show actual waveforms, Play All/Stop, master gain, and per-voice play,
   mute, solo, gain, pan, regenerate, delete and WAV export. The host must run its
   audio callback for audition. When idle the insert passes track audio through.
5. Adjust Lo-Cut, Brightness and Reverb. Effects render in the background on slider
   release; playback restarts after rendering. **PRINT POST FX TO STEMS** selects
   processed or dry files. The reverb tail is contained within the original duration.
6. Drag **STEMS — DRAG TO CUBASE** into the DAW, or **EXPORT STEMS** to your chosen
   project directory's **NOORI CHOIR** folder. Eight singers produce eight files.
   Multi-file Windows dragging is implemented using JUCE's native file/OLE path;
   actual Cubase acceptance, track creation and timeline placement remain untested.
   **EXPORT MIX** explicitly makes one combined file. It does not replace stems.
7. Changing per-voice controls or export options invalidates prepared drag files.
   Click **PREPARE STEMS** before dragging again. Completed drag files persist in
   the local cache and are not deleted during the drag operation.

Exports retain exact source frame count and sample rate. Output channels follow
Mono/Stereo. Choose 24-bit PCM or 32-bit IEEE float in Settings. Optional BWF
metadata retains an imported WAV's sample reference or the capture's reported
host sample position. A file without a timestamp defaults to zero. BWF metadata
alone does not guarantee DAW placement: align all file starts with the original
event or use the host's timestamp import command where appropriate. Individual
stems print per-voice gain and stereo pan; mute/solo apply to audition and mix
export, not removal of individual files. Mix export includes a 1/sqrt(N) gain
factor. Leave mix headroom; float files can retain samples above full scale,
whereas 24-bit PCM is limited to full scale.

## Engine and limitations

Signalsmith Stretch provides spectral pitch shifting and formant compensation.
A YIN-style detector tracks monophonic pitch. Diatonic harmony moves between scale
degrees while preserving expressive pitch offsets. Unvoiced audio is blended
with the source. The original leading/trailing silence boundary is retained in
dry stems. Source melody, not project tempo, defines musical timing.

This is a **DSP choir/harmony engine, not a neural choir model**. Gospel, Kids,
Shout, Naat, Soft Emotional, Male/Female/Mixed are audible formant/EQ/dynamics
presets, not trained singers or microphone measurements. Timbre realism, octave
artifacts, pitch detection on noisy/polyphonic sources and Urdu/Arabic diction
require listening acceptance. No AI weights or GPU inference are included. See
[neural technology review](docs/NEURAL-ENGINE-REVIEW.md).

Import is limited to mono/stereo, 8–384 kHz, 512 MiB decoded audio. Generation
rejects configurations exceeding an estimated 512 MiB for source plus dry/wet
voices. FFT and temporary buffers add overhead. Use shorter files/fewer voices.
Custom scales and automatic key detection are not implemented in this revision.

## Project persistence

Parameters, appearance, singer mix and the persistent cache ID are saved in host
state. Lossless source and dry voices live in the current Windows user's
`AppData/Roaming/NOORI CHOIR/Cache` (JUCE user-application-data directory). Reopen
restores them and rerenders FX in the background. Missing caches report a recovery
message. The cache is not embedded in the .cpr and is not portable by itself;
retain/copy it when moving projects. Persistent caches and prepared drag takes
are not automatically cleaned, so they consume disk space. Cancelled jobs can
leave a completed, unused cache take; partial export files are cleaned up.

Light/Dark and 100/200/300% scale are saved. Logical editor size is 720×1000,
1440×2000 or 2160×3000 before host DPI scaling. Large scales need a suitable
monitor. The original supplied SVG is included; its visible mark is embedded.

## Build, tests and installer

Visual Studio 2022 Desktop C++, CMake >=3.24, Git:

```powershell
cmake -S voice-choir -B build-choir -G "Visual Studio 17 2022" -A x64
cmake --build build-choir --config Release --parallel 2
ctest --test-dir build-choir -C Release --output-on-failure
./voice-choir/scripts/Package.ps1
./voice-choir/scripts/BuildInstaller.ps1
```

The installer script requires Inno Setup 6. CI compiles it and silently installs
into an isolated runner directory, checks the installed VST3 hash, uninstalls and
checks preservation of unrelated files. Consult the actual workflow results:
these checks do not establish interactive desktop installation or Cubase
compatibility. The installer is unsigned. Default installation is the shared
`C:\Program Files\Common Files\VST3` directory. Close Cubase first.

Tests cover actual musical output frequencies, WAV/AIFF decode, eight stems,
PCM/float/BWF export, preview/mute, FX processing, cache restoration, restore during
a render, capture sample counting, regeneration/deletion, and editor pages with
all themes/scales. CI supplies real screenshots, compiled VST3, corresponding
project sources and installer test evidence. Cubase 15 tests are not marked as
passed without a real Cubase session. See [host acceptance](docs/CUBASE-ACCEPTANCE.md).

AGPL-3.0; vendored Signalsmith components are MIT. JUCE and VST3 notices are bundled.
Source: https://github.com/sagemadinah1292-hub/noori- . BUILD-INFO.json identifies
the exact checkout used for a binary.
