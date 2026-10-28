# Cubase Pro 15 manual acceptance — pending

No tests below have been performed automatically or claimed as passed.

- Install/uninstall using the interactive installer with Cubase closed; rescan
  VST3 and confirm correct plugin identity and all mono/stereo bus layouts.
- Import WAV/AIFF; compare waveform and audition against the original source.
- Test a direct Cubase event drag separately from Explorer file drop. Record
  whether Cubase exposes a path and whether event trims/offsets are preserved.
- Capture a continuous event, starting/stopping deliberately. Verify sample count
  and captured host timeline position. Do not seek mid-capture for alignment tests.
- Generate unison, octave and scale thirds/fifths on known vocal melodies; listen
  for formants, consonants, pitch errors, stereo placement and artifacts.
- Verify every lane, mute/solo, gain/pan, regeneration, deletion and master gain.
- Print each Post FX alone and together; compare dry vs wet exports.
- Drag 8 files from STEMS into Cubase. Record whether Cubase offers separate tracks
  or sequential placement, and choose separate tracks where supported. Check
  common starts and exact sample count. Test a rejected drop/cancel too.
- Export to the project NOORI CHOIR directory; import all eight manually if drag
  fails. Test timestamp-based placement and manual event-start alignment.
- Save/reopen the project with cached source and stems. Test a moved/missing cache
  and verify recovery instructions. Test two plugin instances and host reload.
- Test both themes at 100/200/300% with Windows DPI scaling and multiple monitors.
- Test long audio, cancellation, insufficient disk space and offline rendering.

Screenshots of the reference running in another DAW do not verify any Cubase test.
