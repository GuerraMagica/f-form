# F-Form QA report

**Result:** FAIL

Source: `stimuli/sync_mono.wav`
Render: `reference_control/artificial_delay_10ms.wav`

Ratio (output/input): `1.0`
Pitch shift: `0.0` semitones

Input frames: `576000`; output frames: `576480`; expected: `576000`

| Check | Result | Detail |
|---|---|---|
| exact_duration | FAIL | delta 480 samples; allowed ±1 |
| finite_output | PASS |  |
| markers_ch0 | PASS | input 14, output 14 |
| offset_ch0 | FAIL | +10.000 ms; allowed ±2.0 |
| drift_ch0 | PASS | +0.000 ms; allowed ±1.0 |
| jitter_ch0 | PASS | 0.000 ms; allowed 2.0 |

## Measurements

```json
{
  "abs_peak": 0.75,
  "samples_abs_ge_1": 0,
  "channel_0": {
    "input_count": 14,
    "output_count": 14,
    "input_positions": [
      26400,
      67798,
      104474,
      142647,
      185067,
      224644,
      260875,
      301010,
      343254,
      380924,
      417936,
      459730,
      500666,
      537093
    ],
    "output_positions": [
      26880,
      68278,
      104954,
      143127,
      185547,
      225124,
      261355,
      301490,
      343734,
      381404,
      418416,
      460210,
      501146,
      537573
    ],
    "offset_samples": 480.0,
    "offset_ms": 10.0,
    "drift_samples": 0.0,
    "drift_ms": 0.0,
    "maximum_anchor_residual_samples": 0.0,
    "maximum_anchor_residual_ms": 0.0,
    "anchor_deltas_samples": [
      480.0,
      480.0,
      480.0,
      480.0,
      480.0,
      480.0,
      480.0,
      480.0,
      480.0,
      480.0,
      480.0,
      480.0,
      480.0,
      480.0
    ]
  }
}
```

This report checks technical invariants, not perceptual equivalence or professional audio quality.
