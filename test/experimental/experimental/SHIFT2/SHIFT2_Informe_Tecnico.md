# SHIFT2 / F-Form — Auditoría del ZIP y comparación con el WAV original

**Alcance:** inspección del `SHIFT2-main.zip` proporcionado por el usuario; extracción y compilación independiente del texto de `FFormDSPCore.h`; prueba sintética de 440 Hz; comparación del `00_ORIGINAL.wav` con el render neutro anteriormente proporcionado. No se ha compilado ni probado un plugin JUCE/AAX de SHIFT2, ni se ha validado la aplicación web completa.

## Veredicto operativo

SHIFT2 es principalmente una aplicación React/TypeScript que procesa archivos de audio en el navegador y exhibe código C++ como literales dentro de `src/data/cppCodebase.ts`. Ese texto C++ contiene un núcleo compilable fuera de JUCE, pero la prueba mínima revela defectos DSP graves. No sustituir el motor Signalsmith de F-Form M2A ni fusionar SHIFT2 en `main`.

## Inventario verificable

- `src/dsp/dspEngine.ts` (787 líneas): implementación STFT/phase-vocoder simplificada en JavaScript/TypeScript, procesada como buffer completo con Web Audio; no AudioWorklet de baja latencia ni integración AAX.
- `src/data/cppCodebase.ts` (905 líneas): literales exportables con los nombres `FFormDSPCore.h`, `TimeStretchEngine.h`, `PluginProcessor.cpp`, `PluginEditor.cpp` y `fform_qa_v2.cpp`. No son archivos C++ separados en el ZIP.
- `src/data/auditReport.ts`: afirmaciones sobre calidad/arquitectura, no resultados de medición.
- El proyecto contiene `package.json` con `vite build`, `tsc --noEmit` y dependencias React/Vite; no contiene `CMakeLists.txt`, pruebas AAX ni binarios nativos.

## Defectos específicos del algoritmo

| Hallazgo | Evidencia | Consecuencia |
|---|---|---|
| No es Phase-WSOLA | Sin búsqueda temporal ni maximización de correlación WSOLA en `dspEngine.ts`/texto C++; sí STFT, fases e IFFT | Nombre comercial no sustentado por la implementación |
| La preservación no es cepstral | `extractSpectralEnvelope()` calcula máximo móvil + suavizado en TS y máximo móvil en C++; no log-spectrum + IFFT + liftering | No se demuestra preservación de formantes con método cepstral |
| Pitch incorrecto en el núcleo C++ | `pitchFactor` multiplica avance de fase, pero no realiza traslación de bins ni remuestreo adecuado | Tono solicitado no garantizado; +12 semitonos falla la prueba de 440 Hz |
| Normalización OLA errónea | `olaNormFactor = 1 / (winSum * 0.5)` para ventana Hann al cuadrado | Ganancia neutral ~−48.16 dB en la prueba independiente |
| `getLatencyInSamples() = N` sin prueba | Declaración fija en el código, no medición impulso/host PDC | 2048 muestras es afirmación, no latencia comprobada |
| `fractionalPhaseAcc` sin uso | Solo se inicializa y reinicia, no gobierna `Hs` | «0 drift por acumulador» no implementado |
| Realtime vuelve a tener descartes | `if (ch.inFifoCount < ch.inFifo.size()) ...` sin manejo del `else`; `Hs != Ha` con N fijo | Se reintroducen riesgos de overflow/underflow |
| Memoria en hilo audio | `std::vector<double> real(N), imag(N)...` y `vector<bool> isPeak` dentro de `processRealtimeBlock()` | Asignaciones recurrentes, no apto como código realtime robusto |
| Audio estéreo | Cada canal se analiza/sintetiza independientemente, sin anclaje común | Riesgo de alteración de imagen/coherencia no evaluado |
| Test de duración trivial | El QA C++ compara `out[0].size()` con `round(inFrames * 1.25)` cuando el motor ya hace `output.assign(...)` con ese tamaño | No demuestra ausencia de silencios, drift de transitorios, pitch ni calidad |
| Métrica de coherencia no medida | TS asigna `0.96`, `0.82`, `0.81`, `0.65` o `1.0` por backend | Mostrarla como medición sería engañoso |
| Backend «Signalsmith» de web no es Signalsmith | Rama TS etiquetada `signalsmith` ejecuta fórmula simplificada, sin la biblioteca original | A/B web no es comparación válida con F-Form M2A |

## Prueba independiente C++ realizada

Se extrajo del literal TypeScript el contenido de `FFormDSPCore.h`, se compiló con `g++ -std=c++20 -O2`, y se procesó una sinusoide mono de 440 Hz a 48 kHz, 2 segundos, amplitud 0.3, con formantes e IPL desactivados para aislar la conversión base.

| Caso | Frames | RMS medido (zona estable) | Pico dominante |
|---|---:|---:|---:|
| Fuente sintética | 96000 | 0.212132 | 440 Hz |
| `pitch=1`, `time=1` | 96000 | 0.0008287 | 440 Hz |
| `pitch=2`, `time=1` (+12 st solicitados) | 96000 | 0.0001934 | ≈411.25 Hz, no 880 Hz |
| `pitch=1`, `time=1.5` | 144000 | 0.0003681 (según ventana empleada) | 440 Hz |
| `pitch=1`, `time=0.5` | 48000 | 0.0011008 (según ventana empleada) | 440 Hz |

La salida neutra presenta aproximadamente **−48.16 dB de ganancia** respecto a la fuente. El resultado de +12 st **no consigue duplicar la frecuencia fundamental**. Esta prueba no representa al F-Form M2A real: representa exclusivamente el texto `FFormDSPCore.h` generado en SHIFT2.

## WAV original vs render neutro real de F-Form

`00_ORIGINAL.wav`: 48 kHz, estéreo, PCM 24 bits, 145.752 s. `ITCH-01 — Time 1.00× Pitch 1.00×_Audio 2-7.1.wav`: 48 kHz, 8 canales contenedores (solo 2 activos), 32-bit float, 145.760 s.

La correlación de 4 s de música localiza un desplazamiento **+2880 muestras (+60.0 ms)** entre el original y el render neutro. Al compensarlo, la señal prácticamente coincide: correlación de canal izquierdo **0.999999999997** en una ventana 5–9 s; diferencia RMS relativa **−112.23 dB** (con comparación y ganancia unitaria). En otras ventanas musicales 2–132 s se observa prácticamente la misma coincidencia.

**Interpretación:** el render neutro conserva prácticamente el contenido de la fuente, pero se desplaza en esta pareja de exportaciones. El desplazamiento no puede atribuirse automáticamente a la latencia interna del DSP: podría intervenir la alineación de la exportación, el inicio de la selección, el PDC o la grabación. Tampoco prueba que una transformación de pitch/time sea transparente ni que el host bypass funcione correctamente. Hay una diferencia de duración total de 384 muestras (8 ms) entre archivos y las cabeceras tienen formatos distintos.

## Instrucciones para Copilot

1. Preservar `9a9e023`, `8b53a76`, `5cc5040` y el informe anterior.
2. No copiar `FFormDSPCore.h` de SHIFT2 al plugin de producción ni usar el A/B web como benchmark de Signalsmith.
3. Construir el OfflineStretchEngine sobre la versión real de Signalsmith y ejecutar el QA Kit real con original/renders, impulso y tonos.
4. Registrar en el Musical QA que la fuente ORIGINAL está disponible y repetir el análisis alinear/igualar ganancia antes del null test.
5. Investigar por separado los 2880 frames del render neutro y el bypass nativo, con marcas de timeline y PDC.
6. Si se desea conservar SHIFT2, mantenerlo como prototipo experimental aislado y exigir tests de identidad, pitch, duración, transitorios, fase estéreo, rendimiento y seguridad realtime antes de cualquier integración.
