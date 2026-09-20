# Codex / coding agent brief — F-Form synchronization investigation

You are a senior C++ audio DSP engineer specializing in JUCE, Signalsmith Stretch, offline time-stretch, Pro Tools AAX and sample-accurate audio processing. Work in the actual F-Form repository at its checked-out commit. Do not assume that an internet-only review accurately describes the current files; inspect the source and record `git rev-parse HEAD` first.

**User-reported bug:** rendered F-Form audio is not synchronized with the original. We do not yet know whether it is constant latency, progressive time-map drift, dropped/duplicated samples, transient-local shifts, channel misalignment or host-region behavior. Diagnose; do not guess and do not replace Signalsmith prematurely.

**First deliverable, without changing DSP:**
1. Map the exact current audio path `processBlock -> TimeStretchEngine -> Signalsmith Stretch`, parameter conversions and time-ratio semantics. Identify realtime insert vs offline/AudioSuite contract, and state which is actually implemented.
2. Reproduce results at the current commit. Build a standalone/headless C++ render harness that invokes the **actual current DSP code** (do not rewrite its behavior in Python). Where JUCE is involved, use the project's pinned JUCE dependency and build configuration. If dependencies or AAX are unavailable, report the specific blocker and test only what can be compiled.
3. Use the external `fform_qa.py` kit to generate exact deterministic WAV sources. Capture the real processed files and attach JSON/Markdown QA reports. Test time ratios 0.5, 24/25, 1.0, 25/24, 1.5 and 2.0, where the current product claims to support them. Test pitch 0, +3, -3, +12 semitones separately and in combination with time stretching.
4. Test 44.1, 48 and 96 kHz, mono, stereo and 5.1 **only where the current host actually supports those layouts**; report unsupported layouts rather than pretending they pass. Exercise callback block sizes 1, 64, 127, 256, 511, 512, 1024 and varying/random partitions with identical input. Test render consistency and bounded CPU/memory over long material. Run pluginval and real Pro Tools/AAX tests separately on a suitable host if available.
5. Instrument counters: cumulative host input frames, engine-consumed frames, engine-produced frames, FIFO high-water mark, discarded/duplicated frames, underflow/overflow, reported host latency, DSP inputLatency/outputLatency, detected transport discontinuities, pre-roll and tail frames. Never log synchronously from the audio callback; export bounded diagnostics after rendering.
6. Categorize failures with measurements and root cause. Formulate a failing regression test **before** changing the production code. Provide original processed test WAV and report. Distinguish `round(N_in * outputDurationRatio)` offline from equal in/out frames demanded by a normal real-time host callback.

**Fix phase — only after reproduction:**
- Correct the time model and exact duration accounting, including cumulative fractional rounding, start/seek, reset, pre-roll, tail/flush, latency and bypass. Revisit FIFO overflow/underflow and handling of `numSamples` fluctuations. Signalsmith has both `inputLatency()` and `outputLatency()`; review its README and pinned header, not a guessed scalar latency.
- Preserve channels and inter-channel timing and test identical/anti-phase and intentionally offset stereo; avoid channel-by-channel independent time maps or incompatible randomization.
- Do not block/allocate unpredictably in the audio thread. No unsafe silent loss of incoming frames. Validate parameters and buffer bounds.
- Keep offline render and realtime insert interfaces distinct; never claim that ordinary VST3/AAX insertion can transparently lengthen a host timeline region.
- Add before/after measurement tables for duration, offset, drift, local marker residual, channel differences and pitch. When tests cannot validate perceptual quality, explicitly say so.

**Acceptance:**
- CI runs a deterministic, failing-before/passing-after test for the user-reported sync defect; it detects an injected 10 ms delay and artificial drift.
- Correct offline sample count within 1 frame of the declared rounding contract, no lost source frames, and timing errors within thresholds agreed with product owner (diagnostic defaults are not guarantees).
- Realtime behavior is stable across host block partitions, repeated playback, seek and bypass; correct reported/plugin-compensated latency.
- No regression in pitch tuning; no untested promises about subjective quality.
- Summarize exact changes, affected files, regression test commands, open limitations, and link to the diff/PR. Do not modify public API, JUCE/AAX distribution/licensing, or user session state without noting it.
