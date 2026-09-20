# F-Form M3B: Streaming Offline Results

## 1. Scope

This milestone adds a streaming offline path alongside the existing `exact()` full-buffer reference. The realtime M2A engine, GUI, AAX integration and SHIFT2 reference were not modified or integrated.

The two offline paths are:

```text
exact()    -> full-buffer correctness reference
streaming  -> sequential WAV reader/writer with bounded working buffers
```

The streaming path uses the same pinned Signalsmith Stretch backend and keeps its state across `process()` calls. It uses `outputSeek()` for the region start, cumulative input/output scheduling, repeated `process()` blocks and `flush()` for the tail.

## 2. Contract

For input frames `N` and time ratio `r`:

```text
outputFrames = round(N * r)
```

The scheduler uses 64-bit cursors. For each output block it computes the target cumulative input position from the playback rate and derives the block input count from the previous cursor. The final body block is forced to the remaining input count; the tail is emitted by Signalsmith `flush()`.

The prototype metrics define:

- `consumedFrames`: source frames supplied to the streaming backend, including the initial pre-roll context;
- `producedFrames`: frames written to the output WAV;
- `inputLatency` / `outputLatency`: values reported by the pinned Signalsmith instance;
- `prerollFrames`: input context supplied to `outputSeek()`;
- `tailFrames`: frames emitted through `flush()`.

These counters describe the adapter/backend contract. They do not expose every internal STFT read or write performed by Signalsmith.

## 3. Exact-duration matrix

Input: `sync_mono.wav`, 48 kHz, 576000 frames. Streaming block size: 512.

| Ratio | Output frames | Consumed frames | Produced frames | Exact duration |
|---:|---:|---:|---:|:---:|
| 0.50 | 288000 | 576000 | 288000 | PASS |
| 0.75 | 432000 | 576000 | 432000 | PASS |
| 0.96 | 552960 | 576000 | 552960 | PASS |
| 1.00 | 576000 | 576000 | 576000 | PASS |
| 1.04 | 599040 | 576000 | 599040 | PASS |
| 1.50 | 864000 | 576000 | 864000 | PASS |
| 2.00 | 1152000 | 576000 | 1152000 | PASS |

## 4. Synchrony

The official FForm QA analyzer was run on each streaming render:

| Ratio | Markers | Offset | Drift | Max residual | Result |
|---:|---:|---:|---:|---:|:---:|
| 0.50 | 14/14 | +0.302 ms | -0.104 ms | 0.177 ms | PASS |
| 0.75 | 14/14 | +0.490 ms | +0.063 ms | 0.281 ms | PASS |
| 0.96 | 14/14 | +0.365 ms | +0.063 ms | 0.073 ms | PASS |
| 1.00 | 14/14 | +0.021 ms | +0.021 ms | 0.021 ms | PASS |
| 1.04 | 14/14 | -0.417 ms | +0.042 ms | 0.042 ms | PASS |
| 1.50 | 14/14 | +0.094 ms | -0.063 ms | 0.052 ms | PASS |
| 2.00 | 14/14 | +0.656 ms | +0.042 ms | 0.156 ms | PASS |

These are technical marker metrics, not a perceptual quality certification.

## 5. Exact versus streaming

Both paths produced the exact declared number of frames. They are not required to be bit-identical because the call partition and operation order differ.

For 48 kHz `sync_mono.wav`, block size 512, pitch 0:

| Ratio | RMS sample difference | Peak difference | Correlation |
|---:|---:|---:|---:|
| 0.50 | 0.00609 | 0.15353 | 0.9698 |
| 0.75 | 0.01143 | 0.33474 | 0.9148 |
| 0.96 | 0.00257 | 0.06744 | 0.9959 |
| 1.00 | 0.000000005 | 0.0000003 | 1.0000 |
| 1.04 | 0.00146 | 0.04300 | 0.9987 |
| 1.50 | 0.00782 | 0.44752 | 0.9586 |
| 2.00 | 0.01026 | 0.44938 | 0.9163 |

The exact and streaming paths use different scheduling boundaries. The larger differences at 0.75, 1.5 and 2.0 require listening and transient inspection; they are not automatically defects. Ratio 1.0 is effectively identical in this test.

## 6. Block-size stability

Streaming renders at 0.96 on the same input completed at block sizes 64, 127, 1024 and 4096. Each produced 552960 frames with no reader/writer failure. The full synchronization table above used block size 512.

A broader randomized-partition test remains a follow-up: the current prototype CLI supports fixed block sizes, while the realtime QA runner already covers deterministic random callback partitions.

## 7. Performance and bounded memory

Release measurements on Apple Silicon:

| Input | Mode | Ratio | Block | Time | Peak resident memory |
|---|---|---:|---:|---:|---:|
| 12 s stereo | exact | 1.50 | full buffer | 0.13 s | 36.1 MB |
| 145.76 s song | exact | 0.96 | full buffer | 1.06 s | 181.2 MB |
| 12 s mono | streaming | 0.96 | 64 | 0.04 s | 17.4 MB |
| 12 s mono | streaming | 0.96 | 127 | 0.03 s | 17.4 MB |
| 12 s mono | streaming | 0.96 | 1024 | 0.03 s | 17.4 MB |
| 12 s mono | streaming | 0.96 | 4096 | 0.04 s | 17.5 MB |
| 145.76 s song | streaming | 0.96 | 4096 | 1.05 s | 17.9 MB |

The result supports the need for streaming before AudioSuite: memory remains nearly constant as the song length increases, while `exact()` retains full input and output buffers.

## 8. Quality and limitations

The streaming path was validated for:

- exact frame count;
- marker retention and drift;
- mono processing;
- stereo prototype execution through the same backend;
- deterministic backend seed and bounded working buffers.

Still pending:

- pitch matrix for streaming versus exact at every block size;
- full musical quality comparison with `00_ORIGINAL.wav`;
- true peak and transient-specific analysis;
- random partition streaming scheduler;
- long-duration stereo quality listening;
- dialogue and film-stem subjective evaluation.

No normalization or limiting was applied.

## 9. Listening launcher

A macOS launcher is available at:

```text
tools/fform-offline/launch-fform-offline.command
```

It invokes the streaming Release renderer, asks for input WAV, time ratio, pitch and block size, and writes results beside the source in `F-Form Offline Renders`.

Build and copy it into the requested test folder:

```bash
mkdir -p build-offline/test-release
cp tools/fform-offline/launch-fform-offline.command build-offline/test-release/
chmod +x build-offline/test-release/launch-fform-offline.command
```

The first manual presets are:

```text
Neutral:       time 1.00, pitch 0
24 -> 25 fps:  time 0.96, pitch 0
25 -> 24 fps:  time 1.0416667, pitch 0
Slowdown:      time 1.50, pitch 0
Speed-up:      time 0.75, pitch 0
Pitch up:      time 1.00, pitch +3
Pitch down:    time 1.00, pitch -3
```

## 10. Decision

The streaming prototype satisfies the engineering acceptance for exact duration, marker synchronization and bounded memory in the tested matrix. It is suitable for continued offline research, not yet for AudioSuite or production distribution.

The next step is to expand the scheduler with deterministic random partitions and compare streaming against `exact()` across pitch, stereo and musical material before any DAW adapter is attempted. M2A realtime remains unchanged.
