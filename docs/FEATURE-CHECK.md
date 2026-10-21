# Screenshot comparison — 0.3.0

The supplied RePitch page shows product capabilities. This project does not implement the entire RePitch product.

| Capability visible in the reference | NOORI TURAB TEMPO | Practical limit |
| --- | --- | --- |
| Choose another project track as timing grid | Dropdowns enumerate ARA region sequences; insert mode also lists other captured NOORI inserts by host track name | Only audio tracks/events exposed by the host to this ARA document. Ordinary VST3 inserts cannot enumerate arbitrary project tracks; place NOORI on a reference track and Capture/Finish to connect that track within the same plugin process. The built-in metronome must be rendered to audio and exposed through ARA. |
| Quantize to DAW bar/beat and BPM | Explicit exact BPM mode; quarter/eighth/sixteenth/thirty-second grids; straight/triplet/dotted | Integer-sample targets. Conflicting or unsafe targets reject the operation. Clip-edge attacks whose grid target is the fixed file endpoint stay unchanged and are counted in status. |
| Align selected track | Quantize selected track acts on its independent event sessions; preview event can be chosen | Trimmed source ranges are respected. Shared modifications with different placements or crops require independent event copies; they are rejected rather than edited ambiguously. |
| Align selection | Separate marker action with selection count, Select All, clear, range selection, Shift-add and Ctrl+A | A marker must be selected. This is distinct from choosing a track. |
| Reference metronome or vocal timing | Metronome threshold detection or vocal acoustic attack analysis on an ARA reference | Reference event trims, project placement and source sample rates are mapped explicitly. Vocal/percussion modes, short RMS onset refinement. No trained Urdu recognition; acoustic onset accuracy on real vocals needs listening/annotation. |
| Sample-accurate switch | Exact mode forces sample accuracy, 100% strength, zero tolerance/humanize/offset/swing | It cannot infer an event's project start from a WAV file. In insert mode, capture project timing or import at the correct cursor; Audio start = cursor permits correction when host timing is available. |
| Original and edited timing review | Separate original/corrected waveform and attack lanes plus reference markers | One preview event at a time. Full multi-vocal overlay is not implemented. |
| Undo/redo | Plugin history restores rendered audio/markers and listening mode | Settings, references, source audio, selection, appearance and view edits are captured too. Full undo stack is not persisted across project reopening. |
| Pitch / natural tuning | Not implemented; Pitch remains disabled | No pitch blobs, retuning, scale recognition or pitch correction. |
| Level editing | RMS view only | No editable gain envelopes. |
| Track pinning / multiple overlaid vocal tracks | Not implemented | Track selection and batch timing are supported; pin/overlay tools are not. |
| Add/remove arbitrary attack markers | Add / double-click original lane; remove selected / Delete | Existing and manual markers can be selected and dragged. Undo restores them. |
| Native host acceptance | Windows build, DSP, processor/editor and in-process ARA host tests | Cubase Pro 15 Audio Extension discovery, project access, real audio quality and complete lifecycle remain native manual tests. |

Light/dark themes, PNG logo loading and reference-waveform overlay are described in [UPDATE-0.3.md](UPDATE-0.3.md).
