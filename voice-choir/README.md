# NOORI CHOIR 0.2.0 — experimental

C++20 / JUCE Windows x64 VST3, intended for Cubase Pro 15. No ARA.
Modified 2026-10-28. AGPL-3.0; see LICENSE and dependency notices.

## Offline workflow

1. Insert NOORI CHOIR on a vocal track. Import a mono/stereo WAV with **Import WAV**, or drop one WAV onto the editor. PCM and IEEE-float WAV decoding uses JUCE's WAV reader. The source is read locally; no upload or network processing occurs.
2. Choose Pitch spread and Grain variation, or a starting preset. Offline export always renders **eight separate voices**. The four controls labelled LIVE affect only the live insert; blend, gain, width and live voice count do not change stems.
3. Click **Export 8 WAVs…** and select the existing directory containing your Cubase project (not the .cpr file).
4. Eight uniquely named 24-bit WAVs are written directly inside `<project>/NOORI CHOIR/`. Repeated exports keep previous takes. **Open export folder** opens that directory.
5. Import all eight files into Cubase and place their starts at the original vocal event's start. Files retain source sample rate, mono/stereo channel count, exact sample count and leading/trailing silence. Export does not infer the event's position on the Cubase timeline. If the source was bounced from the project origin, place all files at that origin.

The engine uses independently detuned, centred overlapping grains with per-voice drift and amplitude variation. It does not progressively stretch words or add a tail. Source silence and sharp attacks are retained; pitch/grain variation can still colour sustained vocals. Duration and event position are preserved, not every waveform sample. This is conventional DSP, **not an AI choir model**, distinct trained singers, formant synthesis, or generated harmony.

Import and export use a background worker. Cancel removes incomplete output; an export is published only after all eight renders finish. A publication failure rolls back files from that attempt. A process crash during publication can leave partial files; crash-atomic multi-file publication is not implemented. Source WAVs are never overwritten. Inputs are limited to mono/stereo, 8–384 kHz, and 512 MiB decoded float data; invalid/non-finite audio is rejected. Samples outside full scale are limited during export. Reimport audio after reopening a project: imported samples and selected export directory are not embedded in plugin state.

## Interface

Select Light/Dark and 100%/200%/300%. These settings and processing controls are saved in host plugin state. The largest logical editor is 2940 × 1980; host DPI scaling can increase its physical size further, so it may exceed a monitor.

No supplied NOORI logo asset was present in the source branch or workspace. The NOORI text is a fallback, not a claim to reproduce that asset. **Load NOORI logo…** displays a user-selected PNG/JPEG with preserved aspect ratio. Its local path is saved; the file must remain available on that computer. Bundling the approved asset remains pending.

## Live insert

The previous 2–16 voice stereo ensemble is retained. Pitch spread and grain variation also control its pitch/timing spread. LIVE width, blend and output affect the insert. It reports 40 ms nominal latency to the host. Mono, mono-to-stereo and stereo routing are supported. Live and offline algorithms differ; stems do not sum to an exact reproduction of the live insert.

## Build and verification

Use Visual Studio 2022 with Desktop C++ tools, CMake >=3.24, and Git:

```powershell
cmake -S voice-choir -B build-choir -G "Visual Studio 17 2022" -A x64
cmake --build build-choir --config Release --parallel 2
ctest --test-dir build-choir -C Release --output-on-failure
./voice-choir/scripts/Package.ps1
```

JUCE is pinned by commit. The workflow builds only Windows x64 VST3 and uploads a ZIP after engine, offline, WAV I/O, processor/state, and editor tests pass. Package.ps1 checks the AMD64 PE header, test markers and actual UI previews. The ZIP includes exact project sources and licensing; BUILD-INFO.json records its commit and unverified host status. It is an experimental bundle, not a verified Windows installer. Copy the entire `NOORI CHOIR.vst3` directory into `%CommonProgramFiles%\VST3` while Cubase is closed. Install.bat is a copy helper requiring suitable permissions; its interactive installation has not been tested.

Portable DSP tests can be built with `-DCHOIR_BUILD_PLUGIN=OFF`, or compiled directly using g++ and the Source include directory. Windows tests generate six screenshots at both themes and all three scales.

## Remaining acceptance work

Actual Cubase Pro 15 loading, playback, delay compensation, project restore, file dialog/drop behaviour, listening quality and high-DPI usability require a Windows/Cubase session. External WAV drop into the plugin is implemented but not verified in Cubase. Dragging eight generated files from the plugin directly into Cubase, automatic track creation, project-folder discovery, tempo/event-position transfer, ARA and timeline placement are **not implemented**. Use Cubase's file import or Explorer workflow manually. There is no AI model, GPU inference, singer identity/formant model, or verified installer.

Source: https://github.com/sagemadinah1292-hub/noori- . Use the commit in BUILD-INFO.json for the exact revision.
