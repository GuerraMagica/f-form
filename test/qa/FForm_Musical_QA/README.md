# F-Form Musical QA v0.1

Preliminary **real-render** inspector for WAV exported from F-Form. Runs independently from the JUCE plugin and does not modify the DSP. Dependencies: Python 3.10+, `numpy`, `scipy`, `soundfile`.

`python -m pip install numpy scipy soundfile`

Provide the actual files as arguments; filenames may contain spaces or accents:

```bash
python analyze.py \
  --neutral '/your/path/ITCH-01 — Time 1.00× Pitch 1.00×_Audio 2-7.1.wav' \
  --up3 '/your/path/PITCH-02 — Time 1.00× Pitch 1.19×_Audio 2-7.1.wav' \
  --down3 '/your/path/PITCH-03 — Time 1.00× Pitch 0.84×_Audio 2-7.1.wav' \
  --up12 '/your/path/PITCH-04 — Time 1.00× Pitch 2.00×_Audio 2-7.1.wav' \
  --output report.json
```

Optionally supply `--latency`, `--bypass-internal`, `--bypass-host` for a *single* 1-second sample-index-aligned null window; these are recordings of parameter changes, not single-condition reference renders. Do not infer timing/PDC of Pro Tools from an offline file alone.

**Important limitations:** This kit does not measure true peak, LUFS, transient fidelity, subjective distortion, or exact cents on polyphonic music. Its spectral-based pitch estimator has 5-cent grid resolution and needs an untransposed, time-aligned neutral render; treat the values as approximate. It cannot compare acoustic transparency without an unprocessed original. No realtime-to-offline time-stretch validation is possible until a variable-duration render exists.
