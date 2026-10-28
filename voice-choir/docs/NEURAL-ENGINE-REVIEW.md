# Voice generation status

The included engine is Signalsmith Stretch DSP with per-frame pitch analysis,
scale-aware musical parts, formant compensation, per-singer formant/drift changes,
and tonal character/microphone profiles. It is not a trained choir model, and the
names Gospel/Kids/Shout/Naat identify DSP presets, not trained singer identities.

Candidate review, 2026-10-28:

| Candidate | Published licensing | What remains before integration |
| --- | --- | --- |
| OpenVoice V1/V2 | Official README says MIT for code/models and commercial use | Singing conversion evaluation, target singer embeddings, reproducible weights, Windows CPU runtime/ONNX conversion, Urdu/Arabic consonant and timing tests |
| RVC WebUI | Repository code is MIT; individual checkpoints have separate provenance | Suitable distinct singer checkpoints, model-specific terms, f0/runtime export, C++ inference and performance evaluation |
| Signalsmith Stretch | MIT, vendored and used | Perceptual listening acceptance; DSP does not supply learned singer identities |

Sources read:
https://github.com/myshell-ai/OpenVoice/blob/main/README.md
https://github.com/myshell-ai/OpenVoice/blob/main/LICENSE
https://github.com/RVC-Project/Retrieval-based-Voice-Conversion-WebUI/blob/main/LICENSE

No OpenVoice or RVC checkpoint, voice embedding, model runtime, GPU inference or
neural model download is implemented or bundled. Neither candidate has been
verified as a realistic choir generator in this project. A production AI choir
claim requires actual model integration and listening tests on representative
solo singing, including Urdu/Arabic devotional vocals. UI workflow completion
and DSP frequency tests do not establish neural realism.
