# F-Form Milestone 2A: Realtime DSP Stabilization

## 1. Scope

This milestone stabilizes the normal realtime insert contract. It does not implement offline time-stretching, AudioSuite, ARA, formant processing, or a replacement for Signalsmith Stretch.

The realtime contract is now explicit:

```text
N input frames from the host -> N output frames to the host
```

`time_ratio` remains a serialized parameter for session compatibility. In realtime, only `1.0` is an active temporal setting. Historical values different from `1.0` are retained and reported as a safe realtime fallback; they no longer drive a FIFO that consumes a different number of source frames. Pitch processing remains available independently.

## 2. Changes implemented

### Realtime engine

`src/TimeStretchEngine.h` no longer uses the FIFO for realtime processing. It now:

- copies each host block into a preallocated scratch buffer;
- calls Signalsmith with equal input and output frame counts;
- preserves mono/stereo channel count;
- avoids per-block allocation in the production engine;
- keeps a preallocated delay line for latency-compensated bypass;
- updates bypass state even while processing is enabled;
- treats non-finite pitch/time parameter values safely;
- records a diagnostic fallback event when `time_ratio != 1.0`.

The production DSP algorithm remains Signalsmith Stretch. No offline processor has been added.

### Processor integration

`PluginProcessor.cpp` now passes the enabled state to the engine. The host latency remains the Signalsmith output latency. The parameter identifiers, including `time_ratio`, are unchanged.

### QA methodology

The previous baseline remains in `DSP_BASELINE.md` and is not overwritten. The new realtime runner is:

```text
run_realtime.py
```

It validates fixed output duration and diagnostic integrity separately from the old offline-duration expectations.

## 3. Before / after comparison

| Metric | Milestone 1 baseline | Milestone 2A |
|---|---:|---:|
| Output contract | Fixed output, but FIFO consumed variable input | Fixed N-in/N-out direct processing |
| FIFO rejected frames at ratio 2.0 | 255488 representative case | 0; FIFO removed from realtime path |
| Overflow events at ratio 2.0 | 998 representative case | 0 |
| Underflow frames at ratio 0.5 | 576000 representative case | 0 |
| Output frames for 576000 input | 576000 | 576000 |
| Reported host latency at 48 kHz | 4320 samples | 4320 samples |
| Time ratio != 1.0 | Defective FIFO time mapping | Safe fallback, explicit diagnostic |
| Bypass | Direct return without internal compensation | Delayed by 4320 samples |

The old ratio 2.0 and 0.5 results remain historical evidence in `DSP_BASELINE.md`.

## 4. Automated realtime matrix

Command:

```bash
uv run --python 3.12 \
  --with-requirements test/qa/FForm_QA_Kit/requirements.txt \
  python test/qa/FForm_QA_Kit/run_realtime.py \
  --renderer build-qa/fform-qa-renderer \
  --kit test/qa/FForm_QA_Kit \
  --out build-qa/realtime \
  --commit 8b53a76cba2cdb37ff8fe2af178b89d0a8c43275
```

Cases:

- time ratios: `0.5`, `24/25`, `1.0`, `25/24`, `1.5`, `2.0`;
- pitch: `0`, `+3`, `-3`, `+12` semitones;
- block sizes: `1`, `64`, `127`, `256`, `511`, `512`, `1024`;
- fixed and deterministic random partitions;
- mono synchronization stimulus;
- separate 440 Hz tonal pitch cases.

Result:

```text
cases: 340
passed: 340
failed: 0
```

The realtime matrix asserts:

- output frames equal input frames;
- requested, consumed and produced frames equal the host block count;
- zero FIFO rejection;
- zero underflow;
- zero overflow;
- zero non-finite input count;
- finite, valid output through the renderer.

## 5. Pitch measurements

Measured with the QA Kit tonal analyzer at unchanged duration:

| Requested pitch | Measured error |
|---:|---:|
| 0 semitones | approximately 0.000 cents |
| +3 semitones | +4.459 cents |
| -3 semitones | -0.563 cents |
| +12 semitones | +1.031 cents |

All four cases are inside the current diagnostic tolerance of ±5 cents. This validates pitch tuning for the tested synthetic tone; it does not certify vocal formants or perceptual quality.

The +3 semitone result was also measured in independent two-second windows. The
error ranged from approximately `+4.484725` to `+4.484760` cents, a range of
`0.000035` cents. The offset is stable across the file and does not show
progressive pitch drift. No algorithm change was made for this offset.

## 6. Bypass and latency

A disabled mono render at 48 kHz, block size 512 produced:

```text
reported_host_latency_samples: 4320
input_latency_samples: 2880
output_latency_samples: 4320
marker offset: 4320 samples
```

The measured bypass marker offset equals the reported output latency. This is an internal harness measurement. Host plugin delay compensation and Pro Tools bypass semantics still require validation in a real host.

The delay line is updated while processing is enabled, so a later bypass transition does not begin with an unprimed delay state. A full host transport seek/reset sequence is not yet simulated by the harness.

## 7. Sample rates and layouts

Previously executed representative renders remain valid:

```text
44.1 kHz mono: renderer completed
48 kHz mono/stereo: renderer completed
96 kHz mono: renderer completed
```

The current plugin accepts only mono and stereo. Six-channel 5.1 input is explicitly rejected by the harness and is not reported as supported.

## 8. Safety and realtime behavior

The production engine no longer performs FIFO shifting or silently drops source frames. Its scratch buffer, pointer arrays and bypass delay are prepared before processing. The engine performs no file I/O, logging, mutex operation or diagnostic file writing from the audio callback.

The headless harness allocates its own per-block test buffers; that allocation is outside the production plugin callback and does not represent the plugin's realtime path.

## 9. Compatibility

The following public parameter identifiers remain unchanged:

```text
time_ratio
pitch_ratio
enabled
```

Existing sessions and presets can restore historical `time_ratio` values. Values other than `1.0` no longer activate the old defective duration-changing FIFO behavior in realtime. They are reported as a safe fallback and should be exposed as unsupported-for-duration-change in a future UI/documentation pass.

This is intentionally not an offline implementation. Users who need a real duration change still require the next offline/AudioSuite milestone.

## 10. Validation status

Validated:

- QA Kit's five regression tests: `5 passed`;
- realtime fixed-frame matrix: `340 passed`;
- pitch measurements for `0`, `+3`, `-3`, `+12` semitones;
- bypass offset against reported latency;
- mono/stereo renderer execution;
- AAX target rebuilt as universal `x86_64 + arm64`.

Not validated yet:

- Pro Tools realtime behavior and delay compensation;
- Pro Tools AudioSuite or offline processing;
- pluginval;
- host automation timing;
- transport seek and reset in a real DAW;
- long-running CPU and memory profiling;
- subjective voice/music/transient quality;
- 5.1 processing.

## 11. Remaining risks

The most important remaining work is host-level validation and explicit transport lifecycle handling. The realtime engine now has a fixed-frame contract, but the correct interpretation of `inputLatency()` versus `outputLatency()` in every host remains to be checked with a real plugin instance.

A future offline engine must be a separate contract using Signalsmith's input/output processing, seek and flush APIs. It must not be reintroduced into this realtime path by growing a FIFO.

## 12. Pro Tools manual validation protocol

Use the Release AAX package:

```text
build-release/PitchTimePro_artefacts/Release/AAX/F-Form.aaxplugin
```

Record the exact Pro Tools Developer version, sample rate, buffer size, host
delay-compensation state and plugin version.

1. **Neutral path:** test mono and stereo with `time_ratio=1.0` and
  `pitch_ratio=1.0`; compare source and bounce duration and check start/end
  clicks.
2. **Pitch:** use a 440 Hz tone at `0`, `+3`, `-3` and `+12` semitones. Confirm
  unchanged region duration and compare frequency with the QA tolerance.
3. **Historical time values:** test `0.5`, `24/25`, `25/24`, `1.5` and `2.0`;
  confirm the insert does not change timeline duration and identify that real
  duration conversion is not available in realtime.
4. **Bypass:** automate Enabled on/off during sustained audio. Check clicks,
  timing jumps and delay-compensation changes.
5. **Transport:** play, stop, restart, locate, loop and repeat. Check the first
  transient after each locate for stale audio or discontinuity.
6. **Automation:** automate pitch through `0`, `+3` and `-3` while playing.
  Check zipper noise, resets and timing changes.
7. **Sample rates and buffers:** repeat at 44.1, 48 and 96 kHz and host buffer
  sizes 64, 127, 256, 512 and 1024 where available.
8. **Bounce:** perform real-time and offline bounces separately. Compare
  duration, alignment and pitch. Do not treat this as AudioSuite validation.
9. **AAX validation:** run Avid's validator separately and record its output.

The harness validates the DSP contract and internal counters. Only Pro Tools can
validate host delay compensation, transport callbacks, bypass semantics,
automation scheduling and bounce behavior.
