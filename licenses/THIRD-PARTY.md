# Dependency notices

- JUCE 8.0.12, commit 29396c22c93392d6738e021b83196283d6e4d850. https://github.com/juce-framework/JUCE — GPLv3 option used; JUCE-LICENSE.md retained.
- Signalsmith Stretch, commit a670068d9aeb64913331d5cc29337b19a457a7df, https://github.com/Signalsmith-Audio/signalsmith-stretch — MIT.
- Signalsmith Linear 0.6.4, commit de55e6a50ffcf6f8f43f649692d94691c7025151, https://github.com/Signalsmith-Audio/linear — MIT.
- Celemony ARA SDK 2.1.0, tag commit e87afbd2dd2693937bc9795b6b5ae87460fa8c1f, https://github.com/Celemony/ARA_SDK — API/library Apache-2.0. Their licenses are retained individually. Only API and library are required by this build.
- Steinberg VST3 SDK headers included by JUCE. See VST3-SDK-LICENSE.txt and component notices. No proprietary DSP or graphical assets from RePitch are used.

Dependencies are fetched at configure time and are not bundled as third-party source in the small source ZIP. Exact originals remain available at the pinned public repository revisions. Distributing linked GPL binaries requires providing corresponding source and build instructions in accordance with GPLv3.
