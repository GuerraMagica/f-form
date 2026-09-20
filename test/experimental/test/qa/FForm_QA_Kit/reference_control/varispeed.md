# F-Form QA report

**Result:** PASS

Source: `stimuli/sync_mono.wav`
Render: `reference_control/varispeed_24_to_25.wav`

Ratio (output/input): `0.96`
Pitch shift: `0.0` semitones

Input frames: `576000`; output frames: `552960`; expected: `552960`

| Check | Result | Detail |
|---|---|---|
| exact_duration | PASS | delta 0 samples; allowed ±1 |
| finite_output | PASS |  |
| markers_ch0 | PASS | input 14, output 14 |
| offset_ch0 | PASS | +0.021 ms; allowed ±2.0 |
| drift_ch0 | PASS | +0.021 ms; allowed ±1.0 |
| jitter_ch0 | PASS | 0.021 ms; allowed 2.0 |

## Measurements

```json
{
  "abs_peak": 0.7501523494720459,
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
      25344,
      65087,
      100296,
      136942,
      177665,
      215659,
      250440,
      288970,
      329524,
      365688,
      401219,
      441341,
      480640,
      515610
    ],
    "offset_samples": 1.0,
    "offset_ms": 0.020833333333333332,
    "drift_samples": 1.0,
    "drift_ms": 0.020833333333333332,
    "maximum_anchor_residual_samples": 1.0,
    "maximum_anchor_residual_ms": 0.020833333333333332,
    "anchor_deltas_samples": [
      0.0,
      1.0,
      1.0,
      1.0,
      1.0,
      1.0,
      0.0,
      0.0,
      0.0,
      1.0,
      0.0,
      0.0,
      1.0,
      1.0
    ]
  }
}
```

This report checks technical invariants, not perceptual equivalence or professional audio quality.
