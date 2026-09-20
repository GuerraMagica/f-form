# F-Form M3: Experimental DSP Evaluation & Offline Architecture

## Estado de la auditoría

Esta fase se ha realizado en modo solo lectura respecto al DSP de producción. No se ha modificado `src/`, `CMakeLists.txt`, ni el motor M2A. Los commits de referencia siguen intactos:

```text
9a9e023 Initial F-Form plugin scaffold
8b53a76 Add DSP baseline QA harness
5cc5040 Stabilize realtime DSP processing
```

El único cambio local previo a este informe pertenece al informe M2A actualizado. Musical QA permanece como material no rastreado aportado localmente.

## 1. Auditoría de SHIFT2

El repositorio indicado es:

```text
https://github.com/GuerraMagica/SHIFT2
```

No ha sido posible inspeccionarlo porque no es accesible públicamente desde este entorno:

```text
GitHub API: 404 Not Found
GitHub web: 404 Not Found
raw README.md: 404
raw src/data/cppCodebase.ts: 404
```

Por tanto, no es técnicamente válido afirmar que SHIFT2 contenga un motor C++20 compilable. Tampoco se puede verificar ninguna de estas afirmaciones:

- Identity Phase Locking.
- Detección de transitorios.
- Tratamiento de fase.
- Compensación de formantes.
- Coherencia estéreo.
- Latencia.
- Seguridad realtime.
- Gestión de memoria.
- Buffers variables.
- Duración offline exacta.
- Compatibilidad con JUCE o AAX.

La referencia a `src/data/cppCodebase.ts`, mencionada en el material disponible, sugiere potencialmente que el código C++ podría estar representado como datos dentro de una aplicación TypeScript, pero no puede confirmarse sin el repositorio o un ZIP completo. Aunque existiera ese archivo, código C++ mostrado dentro de TypeScript no equivale a un target CMake compilable ni a una implementación integrada en JUCE.

**Conclusión SHIFT2:** no candidato de integración. Antes de evaluarlo harían falta un repositorio accesible o un archivo fuente completo que incluya CMake, headers, fuentes C++, dependencias, licencia y un ejecutable de prueba reproducible.

## 2. Musical QA disponible

El banco local contiene:

- `README.md`
- `analyze.py`
- `report.json`
- `RESULTADOS_PRELIMINARES.md`

Los WAV referenciados por el informe son renders musicales de 48 kHz, 8 canales contenedor y solo los canales 1 y 2 activos. El conjunto incluye un render neutro, tres cambios de pitch, bypass interno, bypass del host y retorno de latencia.

### Integridad demostrada

Los archivos son finitos y no contienen muestras con magnitud mayor o igual a 1.0. Esto demuestra integridad numérica básica de los archivos analizados, no ausencia de clipping intersample, distorsión, aliasing o glitches.

### Pitch relativo

Comparado contra el render neutro provisional:

| Render | Resultado aproximado | Limitación |
|---|---:|---|
| +3 | +305 cents | Estimador espectral musical con resolución de 5 cents |
| -3 | -300 cents | No es un pitch meter de precisión |
| +12 | +1200 cents | No demuestra calidad perceptual |

Estas mediciones indican que el procesamiento aplicado corresponde aproximadamente a los controles solicitados. No permiten comparar calidad entre algoritmos.

### Sincronía y bypass

En una ventana de 5 a 6 segundos:

- bypass interno: null relativo aproximado de `-115.3 dB`;
- retorno de latencia: null relativo aproximado de `-115.3 dB`;
- bypass nativo del host: null relativo aproximado de `-1.1 dB`.

Son ventanas puntuales y no sustituyen una medición de PDC completa. El bypass nativo del host no debe compararse como si fuera simplemente otra condición DSP del plugin.

### Limitaciones críticas

- No hay original seco sin F-Form.
- Los renders largos son 8 canales contenedor, no prueba de procesamiento 7.1.
- No existe todavía un render offline de duración variable.
- No se puede certificar transparencia perceptual.
- No se puede afirmar superioridad de ningún algoritmo con estos datos.

## 3. Alternativas DSP

### A. Signalsmith Stretch offline

Es la primera opción recomendada. Ya está integrada y puede compartir el núcleo actual con un adaptador offline separado. El adaptador debe llamar a la API real de Signalsmith con input/output de distinta longitud, gestionar `seek`, `process`, `flush` y contabilizar frames de salida explícitamente.

Ventajas:

- Dependencia ya integrada.
- Licencia MIT.
- API C++ real.
- Pitch y time independientes.
- Latencia y operaciones de inicio/final documentadas.

Riesgo:

- Hay que construir correctamente el contrato offline; no se puede reutilizar la FIFO realtime.

### B. Signalsmith con mejoras localizadas

Debe ser la segunda etapa, después de disponer de un baseline offline:

- perfiles de configuración por material;
- preservación o corrección de formantes;
- detección de transitorios para seleccionar parámetros;
- phase locking y coherencia estéreo medidos;
- control de calidad frente a voz, música y percusión.

No conviene añadir estas mejoras antes de poder separar artefactos de integración de artefactos intrínsecos del algoritmo.

### C. SHIFT2

No evaluable actualmente. No debe incorporarse ni compararse hasta que exista código C++ accesible y un build reproducible. Un README, un informe generado por IA o código almacenado en una cadena TypeScript no son evidencia suficiente.

## 4. Interfaz DSP común propuesta

Definir una interfaz conceptual independiente de JUCE:

```cpp
struct OfflineRequest {
    double sampleRate;
    double timeRatio;
    double pitchSemitones;
    int channels;
    int64_t inputFrames;
    int64_t outputFrames;
};

struct OfflineResult {
    int64_t outputFrames;
    int64_t inputLatency;
    int64_t outputLatency;
    int64_t consumedFrames;
    int64_t producedFrames;
};
```

Cada candidato debe implementar:

```text
prepare(request)
reset()
seek(input, frames, playbackRate)
process(input, inputFrames, output, outputFrames)
flush(output, outputFrames)
```

El adaptador realtime seguirá siendo otro contrato:

```text
processBlock(input N frames, output N frames)
```

No se debe comparar un motor offline variable con un insert realtime fijo usando el mismo criterio de duración.

## 5. Diseño del Offline Stretch Engine

El siguiente motor offline debería incluir:

1. `outputFrames = round(inputFrames * durationRatio)` calculado antes de procesar.
2. Contabilidad fraccional acumulada para evitar drift por bloque.
3. Pre-roll basado en `inputLatency()` y `outputLatency()` reales.
4. `seek()` al inicio de cada región.
5. `process()` en bloques de tamaño configurable.
6. `flush()` para tail y final exacto.
7. Buffers separados de entrada y salida.
8. Un único mapa temporal para todos los canales.
9. API preparada para más de dos canales, aunque 5.1 no se declare soportado todavía.
10. Métricas de frames consumidos, producidos, duración, offset y residual de marcadores.

Este diseño no es todavía una implementación y no debe entrar en el plugin de producción hasta disponer de pruebas.

## 6. Pruebas exigibles antes de modificar producción

### Integridad

- frames de entrada/salida exactos;
- cero NaN/Inf;
- cero frames descartados;
- cero duplicados;
- silencio sin ruido;
- determinismo con misma semilla y parámetros.

### Sincronía

- impulso y marcadores sintéticos;
- offset global separado de drift;
- pre-roll y tail medidos;
- regiones cortas y largas;
- inicio y final no alineados a bloque.

### Pitch

- seno aislado con error en cents;
- voz monofónica;
- música polifónica;
- variación de pitch durante el tiempo;
- pitch independiente de duración.

### Calidad perceptual

- escucha ciega a nivel igualado;
- voz, batería, bajo, mezcla y transitorios;
- formantes y sibilancia;
- pre-echo, chorus y phase smear;
- coherencia estéreo.

### Comparación

Signalsmith offline debe compararse primero contra sí mismo con distintas configuraciones. Rubber Band o SoundTouch pueden servir como referencias externas solo respetando sus licencias. SHIFT2 queda excluido hasta ser auditable.

## Decisión recomendada

Implementar primero el **Offline Signalsmith Stretch Engine** con la interfaz común anterior, sin conectarlo todavía al plugin principal. Mantener M2A como insert realtime estable y comparar ambos contratos mediante el mismo banco de señales, pero con criterios de duración distintos.

No iniciar una reescritura phase-vocoder ni incorporar SHIFT2 sin evidencia compilable. La prioridad es obtener un render offline exacto y reproducible; solo después tiene sentido estudiar mejoras localizadas de formantes y transitorios.
