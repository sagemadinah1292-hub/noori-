# NOORI TURAB TEMPO 0.3.0

## Review workflow

Choose the edit track and preview event. The reference dropdown selects DAW bar/beat, an imported reference, an ARA-exposed project track, or another captured NOORI insert. Render the built-in metronome to an audio track first. Ordinary inserts cannot enumerate arbitrary Cubase tracks; capture/finish NOORI on both audio tracks in the same plugin process.

Use Exact BPM for integer-sample grid targets or Reference track / exact for reference attacks. Collisions and unsafe stretches are rejected, preserving the previous audio. A file's fixed endpoints cannot be moved; affected edge attacks are reported. Correct detection is separate from correct target placement: inspect the original markers and manually correct missed/extra attacks before alignment.

The short RMS onset refinement avoids using an arbitrary zero crossing as the attack start. Percussion mode uses shorter analysis windows and spacing without vocal breath/vowel suppression. This is an acoustic detector, not Urdu phoneme recognition. Synthetic tests do not establish real-vocal accuracy.

Original and corrected waveforms stay visible. Overlay reference draws the selected reference waveform over the corrected lane in green. Event crops, source rates and project placement are respected. The reference audio is a snapshot: choose/reload the reference after changing its source audio.

Click Add attack, then the original waveform at the desired onset, or double-click the original lane. Select unwanted markers and use Remove selected or Delete/Backspace. Existing corrected markers drag within the stretch limit. Use Select all or a range before Align selected attacks. Quantize selected track needs no marker selection.

## Undo and redo

Plugin history now captures source and rendered audio, detection/quantization settings, presets, reference choices and markers, project anchoring, attack selection/addition/removal/movement, theme, imported logo, zoom/scroll, views and listening controls. Continuous slider changes are coalesced; a new action clears the redo branch. Batch alignment is grouped across event sessions. Undo/Redo tooltips identify the next action; Ctrl+Z, Ctrl+Shift+Z and Ctrl+Y work when the editor has keyboard focus.

History is retained while the plugin/ARA sessions remain alive, including editor close/reopen. Project saves preserve the current settings and edits, not the full history stack. Host transport changes and project-track creation/deletion belong to Cubase history. Recording audio in progress is not reversible; the completed capture import is. Undo never deletes an exported WAV or reverses installation. Selecting an edit/preview track is navigation; history returns to the affected session when restoring its edits.

## Appearance and logo

Light is the default. The header button switches between light and dark themes. Settings > Load PNG logo accepts a PNG up to 4 MiB and fits it proportionally in a 210 x 62 header area. The PNG is stored with the session and can be undone/redone. The supplied chat image must be attached as an actual PNG file to embed it as the built-in default; until then, the header uses NTT text branding and supports loading the original locally.

## Validation limits

Windows compilation, automated DSP and editor checks, and an in-process official ARA SDK test host are separate from native Cubase Pro 15 acceptance. Real Urdu recordings, native ARA lifecycle, installer acceptance and listening quality still require testing in Cubase. Pitch correction, editable gain envelopes and pinned multi-vocal overlays are not implemented.
