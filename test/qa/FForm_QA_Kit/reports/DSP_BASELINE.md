# F-Form DSP Baseline

## 1. Entorno

- Proyecto: F-Form.
- Commit probado: `9a9e023e8d8fc687d6de051688d28a35d02eab37`.
- JUCE: 8.0.7.
- Signalsmith Stretch: commit `57b93f4e9206a089a45387eaa39bdc9f310d3308`.
- Compilador: AppleClang 21.0.0.21000101.
- macOS: Apple Silicon host.
- QA Python: Python 3.12 gestionado por `uv`.
- QA dependencies: `numpy`, `scipy`, `soundfile`, `pytest` desde `requirements.txt`.
- Build QA: CMake + Ninja, `FFORM_BUILD_QA_HARNESS=ON`, `FFORM_BUILD_AAX=OFF`.
- Build AAX existente: generado como universal `x86_64 + arm64` antes de esta fase.

Python 3.9.6 del sistema no era suficiente y no tenía `pytest`. El kit pasó en un entorno aislado Python 3.12:

```text
5 passed in 23.95s
```

Los controles suministrados del QA Kit también se comportaron correctamente: el control varispeed 24/25 pasó y el retardo artificial de 10 ms falló por duración y offset.

## 2. Cambios de esta fase

Se añadió un renderer headless opcional:

```text
tools/fform-qa/main.cpp
```

Se activa con:

```bash
cmake -S . -B build-qa -G Ninja \
  -DFFORM_BUILD_QA_HARNESS=ON \
  -DFFORM_BUILD_AAX=OFF \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build-qa --target fform-qa-renderer --parallel 2
```

El renderer incluye directamente `src/TimeStretchEngine.h`; no contiene una implementación DSP alternativa. Opera en modo `realtime_insert_fixed_output`: recibe bloques del host y devuelve exactamente el mismo número de frames en cada callback.

También se añadieron contadores diagnósticos no bloqueantes a `TimeStretchEngine`:

- frames recibidos del host;
- frames introducidos en FIFO;
- frames solicitados y consumidos por Signalsmith;
- frames producidos;
- high-water mark de FIFO;
- frames rechazados;
- underflow y overflow;
- latencia de entrada y salida de Signalsmith;
- latencia comunicada al host.

La instrumentación no escribe archivos ni hace logging desde el hilo de audio. El renderer exporta los contadores al finalizar.

## 3. Matriz ejecutada

La matriz automatizada está en `run_baseline.py`. Ejecuta 168 casos sobre `sync_mono.wav`:

- ratios: `0.5`, `24/25`, `1.0`, `25/24`, `1.5`, `2.0`;
- pitch: `0`, `+3`, `-3`, `+12` semitonos;
- bloques: `64`, `127`, `256`, `511`, `512`, `1024`;
- una partición aleatoria determinista adicional por cada combinación ratio/pitch;
- semilla aleatoria: `1337`.

La matriz se ejecutó con:

```bash
uv run --python 3.12 \
  --with-requirements test/qa/FForm_QA_Kit/requirements.txt \
  python test/qa/FForm_QA_Kit/run_baseline.py \
  --renderer build-qa/fform-qa-renderer \
  --kit test/qa/FForm_QA_Kit \
  --out build-qa/baseline \
  --commit 9a9e023e8d8fc687d6de051688d28a35d02eab37
```

Resumen:

```text
cases: 168
renderer failures: 0
QA passes: 0
QA failures: 168
```

Los JSON completos y WAV generados están bajo `build-qa/baseline/`, que está excluido por `.gitignore`.

## 4. Defectos reproducidos

### Baseline a ratio 1.0

Con 48 kHz, mono, 576000 frames, bloques de 512:

```text
host_input_frames: 576000
fifo_inserted_frames: 576000
engine_requested_input_frames: 576000
engine_consumed_input_frames: 576000
engine_produced_output_frames: 576000
fifo_rejected_frames: 0
underflow_frames: 0
overflow_events: 0
fifo_high_water_frames: 512
input_latency_samples: 2880
output_latency_samples: 4320
reported_host_latency_samples: 4320
```

Este caso no demuestra que el audio sea perceptualmente transparente, pero sí que la contabilidad de frames se mantiene para ratio 1.0 en esta partición.

### Ratio 0.5: underflow reproducido

Agregado de los siete casos por combinación de pitch y bloque para cada pitch:

```text
underflow_frames por pitch: 4,032,000
fifo_rejected_frames: 0
```

En un caso representativo de 576000 frames y bloque 512:

```text
engine_requested_input_frames: 1,152,000
engine_consumed_input_frames: 576,000
underflow_frames: 576,000
underflow_events: 1,125
```

El host entrega 576000 frames, pero la integración solicita el doble. No hay suficientes muestras en la FIFO para cumplir esa demanda.

### Ratio 24/25: underflow reproducido

El ratio `24/25` se traduce a `0.96` para el renderer. En cada pitch probado se observó:

```text
underflow_frames agregado: 169,911
fifo_rejected_frames: 0
```

Esto confirma que el problema no se limita a ratios extremos.

### Ratio 25/24: overflow y descarte reproducidos

El ratio `25/24` se traduce a `1.0416666667`. Por pitch:

```text
overflow_events agregado: 11,226
fifo_rejected_frames agregado: 43,996
```

### Ratio 1.5: overflow y descarte reproducidos

Por pitch:

```text
overflow_events agregado: 18,510
fifo_rejected_frames agregado: 1,147,826
```

### Ratio 2.0: overflow y descarte reproducidos

Por pitch:

```text
overflow_events agregado: 19,233
fifo_rejected_frames agregado: 1,821,836
```

Caso representativo con bloque 512:

```text
fifo_high_water_frames: 32768
fifo_rejected_frames: 255488
overflow_events: 998
```

Esto demuestra pérdida silenciosa de entrada en la integración actual. No es una propiedad atribuida a Signalsmith: el rechazo ocurre antes de llamar al motor, en la lógica de capacidad de la FIFO.

## 5. Duración y sincronía

El renderer implementa un insert realtime con salida fija: por diseño escribe 576000 frames para una entrada de 576000 frames. Por eso el analizador externo, que calcula `round(input_frames * ratio)` como contrato offline, marca los ratios distintos de 1.0 como FAIL de duración.

Esto no debe interpretarse como que un insert convencional tenga permiso para cambiar la duración de una región de la timeline. Son contratos diferentes:

- realtime insert: entrada y salida por callback tienen el mismo número de frames;
- offline render: entrada y salida pueden tener cantidades distintas y el ratio debe contabilizarse explícitamente.

En los ratios distintos de 1.0, además de la duración incompatible, los marcadores presentan offsets, drift o pérdida de marcadores. En ratio 2.0 el QA pierde marcadores porque la integración ha descartado frames de entrada.

La latencia observada por Signalsmith a 48 kHz es:

```text
inputLatency: 2880 samples
outputLatency: 4320 samples
```

El plugin comunica `outputLatency` al host. Esta baseline no intenta eliminar manualmente esa latencia del render; los fallos de underflow/overflow siguen siendo independientes de una compensación constante de latencia.

## 6. Layouts y sample rates

El plugin actual solo acepta mono y estéreo mediante `isBusesLayoutSupported()`. El estímulo 5.1 contiene seis canales, pero no se declara como PASS: el layout está explícitamente fuera del contrato actual.

Casos ejecutados adicionalmente con el renderer real:

```text
48 kHz mono: PASS de ejecución a ratio 1.0
48 kHz stereo, particiones aleatorias: PASS de ejecución, sin underflow/overflow a ratio 1.0
44.1 kHz mono: PASS de ejecución a ratio 1.0, 529200 frames
96 kHz mono: PASS de ejecución a ratio 1.0, 1152000 frames
5.1: NO SOPORTADO por el plugin actual
```

La ejecución PASS aquí significa que el renderer completó y produjo WAV/diagnóstico; no certifica calidad perceptual.

## 7. Causas raíz confirmadas

### Integración F-Form confirmada

1. `TimeStretchEngine` calcula `round(numSamples / timeRatio)` como consumo solicitado.
2. El callback realtime recibe y debe devolver `numSamples`.
3. La FIFO usa capacidad fija `bufferSize * 64`.
4. Si no hay suficientes frames, el motor recibe menos de los solicitados y se registra underflow.
5. Si la FIFO está llena, `samplesToStore` se recorta y la entrada restante se pierde.
6. El renderer entrega siempre el mismo número de frames que el host, por lo que no es un renderer offline de duración variable.

Estos puntos están demostrados por los contadores y los 168 casos reproducibles.

### No confirmado como defecto de Signalsmith

No hay evidencia en esta fase de que Signalsmith sea la causa del descarte de muestras. La pérdida ocurre en la FIFO de F-Form antes de la llamada a `stretch.process()`.

La calidad perceptual, phase locking, formantes y artefactos no quedan certificados por esta baseline.

## 8. Limitaciones de esta fase

- No se ejecutó Pro Tools real ni AudioSuite.
- No se ejecutó pluginval porque no está instalado.
- No se validó firma AAX ni carga en un host de distribución.
- El renderer aún no implementa `seek()`/`flush()` offline.
- No se midieron CPU y memoria durante una sesión larga.
- No se probó automatización de parámetros desde un host.
- No se certifica calidad vocal, musical ni perceptual.
- El QA Kit trabaja con señales sintéticas, no con material profesional confidencial.

## 9. Propuesta para el siguiente milestone

1. Congelar esta baseline y sus JSON/WAV fuera del repositorio mediante `build-qa/`.
2. Añadir una prueba de regresión explícita para overflow a ratio 2.0.
3. Separar una API realtime de igual número de frames por callback.
4. Diseñar un procesador offline con número de frames de salida explícito.
5. Implementar contabilidad fraccional acumulada en el modo offline.
6. Integrar correctamente `inputLatency()`, `outputLatency()`, `reset()`, `seek()` y `flush()` en el modo offline.
7. Añadir pruebas de continuidad, bypass, seek, pre-roll y tail.
8. Mantener 5.1 como no soportado hasta implementar y validar layouts multicanal.
9. No cambiar Signalsmith hasta disponer de comparativas que separen problemas de integración de calidad intrínseca.

Esta baseline demuestra defectos de integridad de audio y sincronía en la integración realtime actual. No se han corregido todavía; ese trabajo pertenece al siguiente milestone.
