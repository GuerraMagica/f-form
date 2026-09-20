# F-Form M3: Offline Prototype Results

## Scope and safety

This prototype was implemented after reviewing `M3_OFFLINE_ARCHITECTURE.md`. It is standalone only. It is not connected to `PluginProcessor`, VST3, AU or AAX, and it does not implement AudioSuite.

M2A realtime source files remain unchanged. SHIFT2 is retained only as an experimental reference and is not used by this prototype.

## Implementation

Added:

```text
src/dsp/OfflineStretchEngine.h
tools/fform-offline/main.cpp
```

CMake option:

```text
FFORM_BUILD_OFFLINE_HARNESS=ON
```

The renderer uses the exact pinned Signalsmith backend:

## Quality gate: sincronía temporal

El prototipo se ejecutó sobre `sync_mono.wav` mediante el analizador oficial de
`FForm_QA_Kit`. Los siete ratios produjeron renders reales del ejecutable
`fform-offline-renderer` compilado desde el código actual.

| Ratio | Frames salida | Duración | Marcadores | Offset inicial | Drift | Residual máximo | Resultado |
|---:|---:|:---:|:---:|---:|---:|---:|:---:|
| 0.50 | 288000 | exacta | 14/14 | +0.552 ms | +0.333 ms | 0.406 ms | PASS |
| 0.75 | 432000 | exacta | 14/14 | +0.802 ms | -0.021 ms | 0.385 ms | PASS |
| 0.96 | 552960 | exacta | 14/14 | +0.479 ms | -0.167 ms | 0.167 ms | PASS |
| 1.00 | 576000 | exacta | 14/14 | 0.000 ms | 0.000 ms | 0.021 ms | PASS |
| 1.04 | 599040 | exacta | 14/14 | -0.469 ms | -0.125 ms | 0.094 ms | PASS |
| 1.50 | 864000 | exacta | 14/14 | +0.083 ms | -0.021 ms | 0.125 ms | PASS |
| 2.00 | 1152000 | exacta | 14/14 | -0.063 ms | -0.438 ms | 0.833 ms | PASS |

Los valores PASS son diagnósticos técnicos con los umbrales iniciales del QA
Kit. No certifican ausencia de pre-echo ni calidad perceptual. En particular,
la duración exacta por sí sola no prueba sincronía: aquí también se conservaron
los marcadores y el drift quedó por debajo de 1 ms.

## Quality gate: pitch, integridad y estéreo

El tono de 440 Hz con `+3` semitonos produjo:

```text
pitch error: +4.459 cents
duration: exacta
finite output: true
```

Para la canción original `00_ORIGINAL.wav`, los renders Release del prototipo
fueron:

| Caso | Frames | Peak lineal | RMS L/R | Correlación L/R | Finito |
|---|---:|---:|---:|---:|:---:|
| Neutral 1.00 / 0 st | 6996096 | 0.890185 | 0.119995 / 0.115239 | 0.989291 | sí |
| Time 0.96 / 0 st | 6716252 | 0.758750 | 0.119682 / 0.114917 | 0.989388 | sí |
| Time 1.50 / 0 st | 10494144 | 0.774162 | 0.117699 / 0.112864 | 0.990073 | sí |
| Time 1.00 / +3 st | 6996096 | 0.742652 | 0.117102 / 0.112488 | 0.989480 | sí |

No se aplicó normalización ni limitación. Las diferencias de peak/RMS son
observadas, no corregidas. La correlación estéreo demuestra que ambos canales
se procesan y conservan una relación similar, pero no certifica imagen estéreo,
fase, transitorios o transparencia acústica.

No se declara calidad profesional: falta escucha ciega nivelada, referencia
seca equivalente para null completo, análisis de transitorios, true peak,
formantes, pre-echo y material de diálogo aprobado.

## Determinismo y contrato de métricas

El mismo render estéreo a ratio `0.75` se ejecutó dos veces. Hash SHA-256 del
contenido de los WAV, sin usar sus rutas:

```text
85356e3076b98be57c05daf910b5f201b11caaa25ced1fc6ec65705d7c1c1280
85356e3076b98be57c05daf910b5f201b11caaa25ced1fc6ec65705d7c1c1280
```

La comparación de bytes fue idéntica.

En el prototipo actual:

- `consumedFrames` representa los frames de entrada entregados al contrato
  `SignalsmithStretch::exact()`;
- `producedFrames` representa los frames escritos en el buffer de salida;
- no pretende observar los pasos internos de análisis/síntesis de Signalsmith;
- por tanto, `consumedFrames == inputFrames` y
  `producedFrames == outputFrames` prueban el contrato del adaptador, no una
  afirmación sobre cada lectura interna del backend.

## Rendimiento y memoria

Mediciones Release con Apple Silicon, `exact()` y salida WAV flotante:

| Entrada | Ratio | Tiempo real | Peak resident memory |
|---|---:|---:|---:|
| 12 s estéreo, 48 kHz | 1.50 | 0.13 s | 36.1 MB |
| 145.76 s canción, 48 kHz | 0.96 | 1.06 s | 181.2 MB |

Estos datos muestran que `exact()` escala con la duración porque mantiene el
buffer completo de entrada y salida. Para diálogo corto puede ser práctico;
para una escena larga o un stem de película aumenta la memoria proporcionalmente.
Antes de AudioSuite se necesita un scheduler offline por bloques que conserve
el mismo contrato exacto, pre-roll, flush y determinismo sin mantener toda la
región en memoria.

## Quality gate final

| Área | Estado | Interpretación |
|---|:---:|---|
| Duración exacta | PASS | Los siete ratios cumplen el frame contract |
| Marcadores/drift | PASS diagnóstico | Dentro de los umbrales del QA Kit |
| Pitch tonal | PASS diagnóstico | +3 st dentro de ±5 cents |
| Integridad finita | PASS | Sin NaN/Inf en los renders medidos |
| Coherencia estéreo | PASS básica | Canales conservados; falta evaluación de fase/transitorios |
| Determinismo | PASS | SHA-256 idénticos en repetición |
| Calidad perceptual | PENDIENTE | Requiere escucha ciega y material seco controlado |
| Memoria larga | PENDIENTE/ADVERTENCIA | `exact()` escala con el tamaño de la región |

## Autorización del siguiente paso

El prototipo es una base válida para continuar la investigación offline, pero
no está listo para AudioSuite ni para integrarse en el plugin principal. El
siguiente paso autorizado técnicamente debe ser un scheduler offline por
bloques usando el mismo backend Signalsmith y comparado contra `exact()` como
oráculo. M2A realtime permanece intacto.

```text
Signalsmith Stretch 1.3.2
commit 57b93f4e9206a089a45387eaa39bdc9f310d3308
Signalsmith Linear 0.3.1
```

The prototype calls `SignalsmithStretch<float>::exact()`, which internally uses the pinned implementation's `outputSeek`, `process` and `flush` path. It calculates the exact output frame count before rendering:

```text
round(inputFrames * timeRatio)
```

It writes 32-bit floating-point WAV output and JSON metrics. Mono and stereo are supported. Six-channel input is rejected explicitly.

This is a correct first reference path, but it is not yet the final block-scheduled engine described in the architecture document: the first prototype processes the complete buffer through `exact()`. The next implementation step is to expose a bounded block scheduler around the same backend while preserving the exact reference path as an oracle.

## Build

Build command:

```bash
cmake -S . -B build-offline -G Ninja \
  -DFFORM_BUILD_OFFLINE_HARNESS=ON \
  -DFFORM_BUILD_AAX=OFF \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-offline --target fform-offline-renderer --parallel 2
```

The Release prototype compiled successfully without the production plugin target or AAX SDK.

## Exact-duration matrix

Input: `sync_mono.wav`, 48 kHz, 576000 frames.

| Ratio | Expected/output frames | Exact |
|---:|---:|:---:|
| 0.50 | 288000 | PASS |
| 0.75 | 432000 | PASS |
| 0.96 | 552960 | PASS |
| 1.00 | 576000 | PASS |
| 1.04 | 599040 | PASS |
| 1.50 | 864000 | PASS |
| 2.00 | 1152000 | PASS |

For every case:

```text
consumedFrames == inputFrames
producedFrames == outputFrames
exactDuration == true
```

The same prototype processed the real local `00_ORIGINAL.wav` at ratios `0.96` and `1.5`:

```text
inputFrames: 6996096
0.96 output: 6716252
1.50 output: 10494144
```

Those audio files and outputs remain under the ignored local-audio directory and are not repository candidates.

## Pitch, stereo and determinism

A 440 Hz tone rendered at `+3` semitones passed the existing tone QA:

```text
PASS pitch: +4.459 cents; allowed ±5.0
```

A stereo offset marker file rendered at ratio `0.75` with exact output length. Repeating the same render produced identical SHA-1 output hashes:

```text
99970e2c598ae5f380f812144b7685affedc9180
```

This demonstrates deterministic output for the fixed seed and current exact reference path. It does not prove perceptual transparency or professional quality.

Metrics for the 48 kHz prototype report:

```text
inputLatency: 2880 samples
outputLatency: 4320 samples
```

## Musical QA and the -1.1 dB null

The new local original is available at:

```text
test/qa/FForm_Musical_QA/local_audio/00_ORIGINAL.wav
```

The directory is excluded by `.gitignore`. Complete Musical QA render files are not part of the public repository.

The existing Musical QA report's native-host bypass comparison is approximately `-1.1 dB`, while internal bypass and return-latency windows are approximately `-115.3 dB`. That difference cannot be re-evaluated from the repository alone because the earlier render WAVs are external/local evidence and the current checkout contains only the analysis report. The likely causes remain sample-index misalignment, gain, PDC/bypass semantics, state-transition material, export conversion, or channel routing.

Required follow-up with the original and raw renders:

1. Verify sample rate, frame count, channel count and export subtype.
2. Cross-correlate over a lag search range.
3. Estimate least-squares gain after alignment.
4. Compare residual before and after lag/gain correction.
5. Repeat across pre-transition, transition and steady-state windows.
6. Analyze active channels separately, not silent container channels.

No DSP change should be based on `-1.1 dB` until that analysis is reproduced.

## What is proven

- Signalsmith offline API is available and compiles in a standalone target.
- Exact output duration works for all required ratios in the synthetic matrix.
- The real song can be rendered at representative duration ratios.
- Pitch is independent of duration in the prototype API.
- Stereo processing completes with the same temporal map.
- Repeated exact renders are deterministic with the fixed seed.
- Production realtime M2A has not been changed or connected.

## What is not proven

- No block-scheduled offline wrapper has been validated yet.
- No AudioSuite integration exists.
- No subjective professional-quality claim is justified.
- No dialogue/music perceptual comparison against a dry original has been completed.
- No 5.1 support is claimed.
- No Pro Tools host or PDC behavior has been validated for this offline target.

## Next implementation step

Keep `exact()` as the correctness oracle, then add a bounded block scheduler around the same `IOfflineDspBackend` contract. Compare block-scheduled output against `exact()` for duration, pitch, marker timing, channel offsets and deterministic hashes before considering any DAW adapter.
