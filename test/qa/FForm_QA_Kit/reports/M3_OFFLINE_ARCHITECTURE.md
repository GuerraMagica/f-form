# F-Form M3: Offline Time-Stretch Architecture

## Estado

Este documento es un diseño técnico, no una implementación. No se ha modificado el código de producción, no se ha creado commit y no se ha hecho push.

Referencias conservadas:

```text
9a9e023 Initial F-Form plugin scaffold
8b53a76 Add DSP baseline QA harness
5cc5040 Stabilize realtime DSP processing
```

M2A permanece como el procesador realtime estabilizado. El futuro motor offline debe vivir en otra clase y otro contrato.

## 1. Decisión arquitectónica

Se propone:

```text
F-Form Core
├── RealtimeProcessor       -> N frames in, N frames out
├── OfflineStretchEngine    -> duración variable explícita
├── IDspBackend              -> interfaz común de backend
├── SignalsmithBackend       -> primera implementación
└── Host adapters            -> futuros adaptadores offline/AudioSuite
```

No se reutilizará la FIFO realtime para cambiar la duración de una región. El motor offline recibe una región completa o una fuente con acceso determinista, calcula la duración objetivo antes de procesar y conserva un único mapa temporal para todos los canales.

El producto realtime seguirá siendo pitch-shift con duración fija. El producto offline será el responsable de conversiones de duración como `0.5`, `0.75`, `0.96`, `1.04`, `1.5` y `2.0`.

## 2. API propuesta

La API debe ser independiente de JUCE y del host:

```cpp
struct OfflineStretchRequest
{
    double sampleRate = 48000.0;
    double timeRatio = 1.0;       // output duration / input duration
    double pitchSemitones = 0.0;
    int channels = 1;
    int64_t inputFrames = 0;
    int64_t outputFrames = 0;     // round(inputFrames * timeRatio)
    int blockFrames = 4096;
    bool splitComputation = true;
};

struct OfflineStretchMetrics
{
    int64_t inputFrames = 0;
    int64_t outputFrames = 0;
    int64_t consumedFrames = 0;
    int64_t producedFrames = 0;
    int64_t inputLatency = 0;
    int64_t outputLatency = 0;
    int64_t prerollFrames = 0;
    int64_t tailFrames = 0;
    int64_t rejectedFrames = 0;
    bool exactDuration = false;
};

class IOfflineDspBackend
{
public:
    virtual ~IOfflineDspBackend() = default;
    virtual void prepare (const OfflineStretchRequest&) = 0;
    virtual void reset() = 0;
    virtual void setPitchSemitones (double) = 0;
    virtual int inputLatency() const = 0;
    virtual int outputLatency() const = 0;
    virtual void seek (const float* const* input, int frames, double playbackRate) = 0;
    virtual void process (const float* const* input, int inputFrames,
                          float* const* output, int outputFrames) = 0;
    virtual void flush (float* const* output, int outputFrames,
                        double playbackRate) = 0;
};
```

`OfflineStretchEngine` owns the exact frame accounting and calls an `IOfflineDspBackend`. This allows a later backend comparison without rewriting file I/O, host adapters, reports or tests.

## 3. Exact Signalsmith integration

The pinned source is Signalsmith Stretch version `1.3.2`, fetched from commit `57b93f4e9206a089a45387eaa39bdc9f310d3308`; its CMake dependency uses Signalsmith Linear `0.3.1`.

The exact available API provides:

```cpp
stretch.presetDefault (channels, sampleRate, splitComputation);
stretch.inputLatency();
stretch.outputLatency();
stretch.reset();
stretch.setTransposeSemitones (semitones);
stretch.seek (inputs, inputSamples, playbackRate);
stretch.outputSeek (inputs, inputLength);
stretch.outputSeekLength (playbackRate);
stretch.process (inputs, inputSamples, outputs, outputSamples);
stretch.flush (outputs, outputSamples, playbackRate);
stretch.exact (inputs, inputSamples, outputs, outputSamples);
```

For a complete offline region, `exact()` is the natural reference path because the pinned implementation already combines `outputSeek()`, `process()` and `flush()`. The first implementation should still expose the stages separately in diagnostics so pre-roll, body and tail can be measured.

Important API semantics:

- `process()` accepts independent input and output frame counts.
- `inputLatency()` is the amount of input context needed around the processing position.
- `outputLatency()` is the produced output delay, including split-computation latency when enabled.
- `seek()` supplies previous input context without calculating output.
- `outputSeek()` prepares a region start and precomputes output alignment.
- `flush()` finishes pending output and resets phase-vocoder state after the tail.
- `exact()` returns `false` when the input is shorter than the required seek length and zeros the output.

## 4. Offline input/output contract

Before processing:

```text
outputFrames = round(inputFrames * timeRatio)
```

The output buffer is allocated exactly to `outputFrames`. The engine must reject invalid ratios, non-finite values, unsupported channel counts and impossible short-region requests explicitly rather than silently shortening the result.

For each block:

- input and output channel pointers are separate;
- all channels use the same input/output frame positions;
- the block schedule is generated from cumulative frame positions, not independent rounded block ratios;
- the last block is shortened to the exact remaining output count;
- no source frames are dropped or duplicated by the adapter;
- the backend's pre-roll and tail are accounted outside the declared output region.

For a source of `N` frames and output ratio `r`, the declared output is exactly:

$$
N_{out} = \operatorname{round}(N \cdot r)
$$

Any backend latency is an alignment concern, not permission to change `N_out`.

## 5. Pre-roll, flush and region boundaries

For a region:

1. Validate the request and calculate `outputFrames`.
2. Prepare the backend with the region's sample rate and channel count.
3. Set pitch before seeking.
4. Supply enough source context for `outputSeekLength(playbackRate)` when available.
5. Call `outputSeek()` for exact alignment at the region start, or `seek()` when the caller deliberately supplies a prior context.
6. Process the body in bounded blocks.
7. Call `flush()` for the remaining tail.
8. Copy only the declared output region to the destination.
9. Record consumed, produced, pre-roll and tail metrics.
10. Reset the backend before the next independent region.

Short regions must be reported as unsupported or padded according to an explicit product policy. They must not silently produce an apparently valid but unaligned output.

## 6. Sample accounting and drift

The adapter owns a 64-bit cumulative output cursor. It must not repeatedly calculate each block as `round(blockInput * ratio)`, because local rounding can accumulate drift.

Instead, calculate target positions from cumulative ideal time:

```text
idealOutputAtInput = inputCursor * timeRatio
outputCursor = round(idealOutputAtInput)
blockOutput = outputCursor - previousOutputCursor
```

The final block is clamped to `outputFrames - outputCursor`. Metrics must assert:

```text
producedFrames == outputFrames
```

This is separate from the Signalsmith internal `inputOffset` rounding. The adapter's exact output contract must be tested independently of the backend's spectral decisions.

## 7. Mono, stereo and future multichannel

The first implementation supports mono and stereo because those are the production plugin's validated layouts. All channels must share the same time map and seek/flush positions.

Stereo tests must include:

- identical channels;
- anti-phase channels;
- intentionally offset channels;
- independent channel energy checks;
- inter-channel marker offset spread.

The data structures should use channel-pointer arrays so adding multichannel support later does not require a new processing model. This does not claim 5.1 support now.

## 8. Offline validation matrix

Each case should produce a WAV, JSON metrics and Markdown summary. Ratios:

```text
0.50, 0.75, 0.96, 1.00, 1.04, 1.50, 2.00
```

Pitch cases:

```text
0, +3, -3, +12 semitones
```

Block schedules:

- 1;
- 64;
- 127;
- 256;
- 511;
- 512;
- 1024;
- 4096;
- deterministic random partitions.

Material:

- silence;
- impulse and marker train;
- pure tones;
- synthetic stereo offset and anti-phase;
- dialogue, after an approved source is supplied;
- music, using the existing Musical QA renders only as a provisional comparison set.

Metrics must be separated:

### Temporal precision

- exact output frame count;
- consumed source frames;
- pre-roll and tail;
- global offset;
- final offset;
- drift;
- maximum marker residual;
- deterministic repeated render hash.

### Tonal precision

- sine frequency error in cents;
- pitch error by window;
- pitch stability over time;
- no pitch claim from a coarse polyphonic estimator alone.

### Perceptual quality

- blind, level-matched listening;
- transient preservation;
- pre-echo;
- chorus/phasiness;
- vocal identity/formants;
- stereo image.

Mathematical PASS must never be described as professional perceptual quality.

### Performance

- wall-clock render time;
- peak resident memory;
- allocations outside the audio path;
- deterministic CPU profile by ratio, block and material.

## 9. Musical QA null-test investigation

The local Musical QA directory contains analysis code, JSON and Markdown, but no WAV files. The report references externally supplied WAVs, so the `-1.1 dB` native-host-bypass null cannot be reproduced or decomposed from the current checkout.

The result is therefore not evidence of a DSP defect. The likely discriminators are:

1. **Temporal alignment:** a one-sample or larger offset dominates a direct subtraction.
2. **Gain:** native host bypass may pass through a different gain/PDC path than internal Enabled bypass.
3. **Segment condition:** the report compares a single 5–6 second window, not the complete file.
4. **State transition:** the bypass recording contains an automation transition, not a static reference render.
5. **Export path:** PCM_24 source/render conversion, dither, truncation or host bounce settings can alter the null.
6. **Channel routing:** the two active channels are inside an eight-channel container and must be compared after confirming routing.
7. **PDC semantics:** native bypass may remove or retain plugin delay compensation differently from internal bypass.

Correct follow-up procedure:

- obtain the original WAV and both raw bypass renders;
- verify sample rate, channel layout, frame counts and export format;
- compare the same sample-index window first;
- measure cross-correlation lag over a search range;
- estimate least-squares gain after alignment;
- calculate residual before and after lag/gain compensation;
- inspect several windows before, during and after the bypass transition;
- compare active channels separately;
- only then decide whether the residual is host alignment, gain, export conversion or DSP.

No production DSP change should be made for the `-1.1 dB` observation alone.

## 10. Risks

- `exact()` depends on enough input context; short regions need an explicit policy.
- `flush()` resets the backend, so region reuse must be isolated and tested.
- `splitComputation` trades CPU burstiness for latency and must be part of the reproducibility metadata.
- Signalsmith's spectral quality can vary by material; temporal correctness does not prove transparency.
- Randomized internals must use a fixed seed for reproducible reports.
- A future AudioSuite adapter has host-specific region, handles, tail and replacement semantics not present in this design.
- The current M2A realtime path must remain untouched while offline work is developed and tested.

## 11. Implementation plan

1. Add `OfflineStretchEngine` and `IOfflineDspBackend` in a separate non-plugin target.
2. Add a Signalsmith backend wrapping the pinned `SignalsmithStretch<float>` API.
3. Add a standalone WAV renderer using the offline contract.
4. Implement exact frame accounting and metrics.
5. Add offline QA tests for mono/stereo, ratios, pitch, block schedules, seek, flush and short regions.
6. Compare deterministic repeated renders.
7. Add Musical QA only after source WAVs are restored locally.
8. Review results and thresholds before any plugin integration.
9. Keep AudioSuite as a later adapter over the offline core; do not put host-specific code into the DSP backend.

## 12. AudioSuite preparation

The offline core should expose region-oriented processing but remain host-neutral:

```text
AudioSuite adapter
  -> obtains region, handles, sample rate and channel buffers
  -> creates OfflineStretchRequest
  -> calls OfflineStretchEngine
  -> writes exact output region and reports tail/latency
```

This prepares the architecture for AAX AudioSuite without implementing AudioSuite now. The AAX adapter must not change the DSP core's frame accounting or make realtime `processBlock()` responsible for timeline duration changes.

## Recommendation

Implement Signalsmith offline first. It is already pinned, compiled and validated in M2A, and its exact API supplies the required `outputSeek`, `process`, `flush` and `exact` operations. Do not integrate the offline engine into the production plugin until its frame, pitch, stereo, deterministic and perceptual test results have been reviewed.
