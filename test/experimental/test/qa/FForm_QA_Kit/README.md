# F-Form QA Kit — regression tests for synchronization and pitch

This kit is an **external test harness**, not F-Form, a JUCE/AAX build, or an audio-quality certification. It generates deterministic reference WAVs, then measures WAVs you render through the real F-Form build. It cannot infer that the plugin passed tests without those renders.

## Setup

Python 3.10+:

```bash
python -m pip install -r requirements.txt
python fform_qa.py generate --out stimuli
python -m pytest -q
```

Files created: `sync_mono.wav`, `sync_stereo_identical.wav`, `sync_stereo_antiphase.wav`, `sync_stereo_offset.wav`, `sync_5_1_offset.wav`, `tone_440_mono.wav`, `silence_stereo.wav`. Each file is 48 kHz, floating-point WAV; the six-channel test has six channels stored in numeric file order. Check your host's interpretation of 5.1 channel order and export unmodified channels.

## How to obtain REAL results

1. Import a stimulus as-is into Pro Tools or your chosen host, without sample-rate conversion, tempo conform, gain changes, fades, or normalization.
2. Render through the build and settings you want to test. Print exact host version, plugin version/commit, render mode, sample rate, buffer size, algorithm parameters, and whether host delay compensation is enabled. **For the primary tests, align the rendered region to the original start and remove intentional pre-roll; do NOT manually drag individual transients into alignment.** Keep a second raw export if diagnosing reported plugin latency.
3. Export at 48 kHz float WAV with the same channel count. Maintain full tail, and do not trim/pad to force a passing duration.
4. Run the relevant command, supplying the desired **output duration / input duration** ratio:

```bash
python fform_qa.py analyze \
  --source stimuli/sync_mono.wav \
  --render renders/fform_sync_mono.wav \
  --kind sync --ratio 1 \
  --json reports/sync_mono.json \
  --report reports/sync_mono.md
```

Offline cinema-to-TV rate conversion **24 fps to 25 fps** at constant pitch:

```bash
python fform_qa.py analyze \
  --source stimuli/sync_stereo_offset.wav \
  --render renders/fform_24_to_25.wav \
  --kind sync --ratio 24/25 \
  --json reports/speed.json --report reports/speed.md
```

Pitch +3 semitones at unchanged duration:

```bash
python fform_qa.py analyze \
  --source stimuli/tone_440_mono.wav \
  --render renders/fform_plus3.wav \
  --kind tone --ratio 1 --pitch-semitones 3 \
  --json reports/pitch.json --report reports/pitch.md
```

Silence:

```bash
python fform_qa.py analyze --source stimuli/silence_stereo.wav \
  --render renders/fform_silence.wav --kind silence --ratio 1
```

To test 5.1, repeat with `sync_5_1_offset.wav`. To detect channel decorrelation, also inspect the identical and anti-phase stereo renders in a DAW with a null test. A spectral DSP may change the waveform while preserving perceptual quality: do **not** demand a null against the unprocessed source for non-identity pitch/time transformations.

## What the metrics mean

- `frame_error`: output frames minus `round(input_frames * durationRatio)`. A sample-exact offline render is an engineering objective, not an automatic property of a spectral library.
- `offset_ms`: median marker timing error vs desired time map, measured separately for each channel. This is *not necessarily* pure plugin/host latency: pre/post-echo or asymmetric transient smearing can move the measured energy maximum.
- `drift_ms`: last-minus-first marker timing error. In a constant-ratio transform it should not grow over the length of the render.
- `maximum_anchor_residual_ms`: largest deviation from the median marker offset, detecting local slips or inconsistent transient placement.
- `interchannel_offset_spread_samples`: maximum difference in global marker offsets between channels, after accounting for intentional input offsets and scaling them by the duration ratio.
- `markers_chN`: count preservation. Missing or additional resolved markers are a diagnostic warning; heavily altered DSP could produce false positives, which require listening and waveform inspection.
- `pitch_error_cents`: measured dominant tone vs expected frequency; not a direct measurement of formant quality or perceived clarity.

Default thresholds are **initial diagnostic targets, not an industry standard**: duration ±1 sample, offset ±2 ms, drift ±1 ms, residual ±2 ms, interchannel ±1 ms, pitch ±5 cents. Change these for agreed product requirements. For offline audio delivery, also examine alignment in samples in a DAW.

## What this cannot certify

- The C++ build, host callback semantics, AAX AudioSuite support, plugin delay compensation and real-time thread safety. Those require running F-Form and instrumenting its C++/JUCE integration.
- Musical, vocal, stereo/5.1 quality or perceptual superiority. Add licensed source material for voice/music/transients and do blind matched-level audition alongside measurements; no single objective audio metric is sufficient.
- A 5.1 host routing convention. Verify channel order before concluding channels are swapped.

## Source hygiene

Keep confidential production dialogue and masters **outside any public repository**; synthetic examples can be committed. Share anonymized, approved excerpts locally with your coding agent if permitted by company policy.
