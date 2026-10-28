# NOORI CHOIR 0.4.1 — protected friends preview

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
   harmonic minor, ascending melodic minor, and custom pitch classes selected in Settings.
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

This is a **DSP choir/harmony engine, not a neural choir model**. Gospel Choir,
Children's Choir, Shout / Gang Choir, Male Choir, Female Choir, Mixed Choir,
Soft Emotional Choir and Nasheed / Naat Choir are audible formant/EQ/dynamics
presets, not trained singers or microphone measurements. Timbre realism, octave
artifacts, pitch detection on noisy/polyphonic sources and Urdu/Arabic diction
require listening acceptance. No AI weights or GPU inference are included. See
[neural technology review](docs/NEURAL-ENGINE-REVIEW.md).

Import is limited to mono/stereo, 8–384 kHz, 512 MiB decoded audio. Generation
rejects configurations exceeding an estimated 512 MiB for source plus dry/wet
voices. FFT and temporary buffers add overhead. Use shorter files/fewer voices.
Automatic key detection is not implemented. Custom-scale buttons are intervals relative to the selected key.

## Project persistence

Parameters, appearance, singer mix and the persistent cache ID are saved in host
state. Lossless source and dry voices live in the current Windows user's
`AppData/Roaming/NOORI CHOIR/Cache` (JUCE user-application-data directory). Reopen
restores them and rerenders FX in the background. Missing caches report a recovery
message. The cache is not embedded in the .cpr and is not portable by itself;
retain/copy it when moving projects. Persistent caches and prepared drag takes
are not automatically cleaned, so they consume disk space. Cancelled jobs can
leave a completed, unused cache take; partial export files are cleaned up.

Light/Dark and 75/100/200/300% scale are saved. New instances default to 75%
(540×750). Existing saved 100/200/300% choices are preserved. The larger sizes
are 720×1000, 1440×2000 and 2160×3000 before host DPI scaling. Large scales need a suitable
monitor. The original supplied SVG is included; its visible mark is embedded and rendered
in white on the gold header. A replacement white logo file is not required.
The eight choir types are accessible on the opening screen and in Choir Setup.

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


## Friends-only registration

The registration screen supports email + individual invitation key, server-signed
activation, three-day trials and binding to one Windows installation. The owner
can generate 20 keys and grant permanent **free** access; no payment system is
included. See ../activation-service/HOSTINGER-SETUP.md for the PHP/MySQL owner
portal prepared for www.noorivst.stopncart.com.

Version 0.4.1 defaults to **registration required**, using the owner-supplied HTTPS
API and RSA-2048 public modulus in Config/Activation.json. This file is public trust
configuration; the private signing key stays on Hostinger. CMake options and
Actions variables may explicitly override these defaults. A development build
requires an explicit NOORI_ENABLE_LICENSING=OFF and is labelled unprotected.
BUILD-INFO.json records the compiled mode. Live Hostinger activation must still be
tested with this installer before distributing it as a verified friends release.
No private key belongs in a plugin, installer, Git repository or chat.

Default online policy checks at startup and renews a short (15-minute) signed
lease. Trial expiry is recorded by the server on first activation. Windows DPAPI
protects saved activation credentials; the activation record is separate from
Cubase project state. Removing local credentials does not release a key for use
on another computer. Regenerating/saving/exporting voices and preview require a
valid grant in protected builds; the original dry insert audio remains available
when unactivated or expired. Existing generated files are never deleted by expiry.

Keys are bound to a hash of the Windows installation ID. A Windows reinstall or
identity change can need an owner-approved reset. This is protection against
ordinary key copying, not a claim that desktop source cannot be modified. The
project's existing AGPL terms remain applicable.
