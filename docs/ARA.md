# ARA integration evidence and remaining validation

Sources inspected during development:
- Official ARA SDK: https://github.com/Celemony/ARA_SDK/tree/releases/2.2.0
- ARA API: https://github.com/Celemony/ARA_API
- JUCE 8.0.12 ARA interfaces and ARAPluginDemo: https://github.com/juce-framework/JUCE/blob/8.0.12/examples/Plugins/ARAPluginDemo.h
- Attempted Steinberg Cubase Pro 15 documentation: https://www.steinberg.help/r/cubase-pro/15.0/en/cubase_nuendo/topics/ara_integration/ara_integration_c.html (HTTP 403 from this environment; content not verified).

The official SDK documents the VST3 ARA extension and JUCE implements the wrapper entry point. This is evidence for a supported SDK mechanism, not proof that this new factory is accepted by a particular Cubase version. Host support and licensing eligibility must be confirmed using official Cubase 15 documentation and a real host.

Implementation in Source/ARAIntegration.cpp: official factory entry point; custom audio modification with EditSession; JUCE ARA audio-source reader instantiated on the message thread and used on a background worker; playback-region mapping to source seconds; try-read-lock around model traversal during rendering; per-modification archive keyed by host persistent IDs; host content notifications on committed edits. Full source audio stays in Cubase; archives contain settings, tempo points and markers. Reopening queues source reads and re-renders the archived warp.

The editor selects the first effective region from the ARA editor view. No arbitrary-event access exists in ordinary insert mode. No ARA analysis content types or external host time-stretch flags are advertised; internal onset markers are plugin-private. No native speech, phoneme or note-transcription capabilities are advertised.

Acceptance is incomplete until a Windows build loads in Cubase Pro 15, ARA event selection and lifecycle are tested, edited audio is heard at the expected timeline positions, offline exports match playback, and project archives reopen with matching edits. Removal/deactivation, undo/redo at host level, source replacement, clips sharing a modification, tempo changes, sample-rate changes, cancellation, multiple open editors and concurrent realtime/offline processing require additional integration testing. Failed source reads currently leave a visible error; manual retry through instance recreation may be needed.

Dependency compatibility: the JUCE 8.0.12 wrapper includes ARAChannelArrangement.cpp, which exists in official ARA SDK 2.2.0. SDK 2.3.0 renamed it ARAChannelFormat.cpp. JUCE's prose still mentions 2.1.0, which predates that utility. The project therefore pins the 2.2.0 tagged release after inspecting the wrapper and SDK source trees, rather than patching vendor code or trusting the stale prose.
