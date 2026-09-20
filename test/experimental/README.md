# F-Form — QA completo (20-09-2026)

**Paquete privado para diagnóstico, NO listo para subir íntegro a GitHub.** Contiene la canción original, siete grabaciones/exportaciones de Pro Tools y el ZIP de SHIFT2 facilitados para análisis. Conserva los WAV y el ZIP sin transformación. No es un build JUCE/AAX de F-Form ni una certificación de calidad perceptiva.

## Descarga en cinco partes

Este QA completo se distribuye en **5 archivos ZIP** para facilitar la transferencia de los WAV. Descarga todos y **extrae todos en una misma carpeta**; cada ZIP utiliza la misma carpeta raíz `FForm_QA_Complete_20260920/` y se fusiona sin reemplazar contenido distinto. Primero PART_01_Core y luego PART_02 a PART_05. Solo después ejecuta `python verify_manifest.py` desde la carpeta raíz. Los archivos WAV originales se conservan byte a byte.

## Estructura

- `test/qa/FForm_QA_Kit/`: banco sintético reproducible de sincronía/pitch, scripts Python, tests, estímulos y controles de referencia.
- `test/qa/FForm_Musical_QA/`: análisis musical, informe preliminar histórico (previo a recibir el original) y análisis ampliado.
- `test/qa/FForm_Musical_QA/local_audio/00_ORIGINAL.wav`: fuente original estéreo, 48 kHz, PCM 24 bits.
- `test/qa/FForm_Musical_QA/local_audio/renders/`: siete WAV originales de Pro Tools. Los cuatro renders musicales están en contenedor de ocho canales, de los que dos contienen audio; se conservan SIN convertirlos a estéreo.
- `experimental/SHIFT2/SHIFT2-main.zip`: proyecto de Gemini recibido, sin cambios.
- `experimental/SHIFT2/SHIFT2_Informe_Tecnico.md`: auditoría preliminar y limitaciones del prototipo C++.
- `manifest_sha256.json`: hashes de todos los archivos de este paquete, salvo el manifiesto mismo.

## Ejecución

Python 3.10+, `numpy`, `scipy`, `soundfile`, `pytest`. La herramienta `uv` del Mac puede gestionar Python 3.12.

```bash
cd test/qa/FForm_QA_Kit
python -m pip install -r requirements.txt
python -m pytest -q
```

Para ejecutar las pruebas musicales completas desde la raíz del paquete:

```bash
python -m pip install numpy scipy soundfile pytest
python test/qa/FForm_Musical_QA/analyze_full.py
```

El nuevo análisis genera en `test/qa/FForm_Musical_QA/reports/` los ficheros `musical_full.json` y `musical_full.md`. Los resultados históricos de `report.json` y `RESULTADOS_PRELIMINARES.md` son anteriores al original, y se mantienen para trazabilidad. La nueva comparación del original tiene en cuenta offset antes del null. La comparación con el bypass nativo no prueba PDC sin documentar posiciones de transporte y momentos de conmutación.

Comprueba que el contenido fue transferido íntegramente:

```bash
python verify_manifest.py
```

## Integración en tu repositorio local PitchTimePro

Copia el contenido de `test/qa/` al directorio del mismo nombre del proyecto. Si ya existe `FForm_QA_Kit`, no sobrescribas inadvertidamente scripts/harness nuevos: compara primero. Coloca `experimental/SHIFT2` fuera del código de producción, sin integrarlo. **No hagas `git add .` sobre este paquete.** Añade las reglas de `GITIGNORE_SNIPPET.txt` al `.gitignore` del repo ANTES de copiar `local_audio/`. La canción original y bounces no son material para publicar automáticamente.

Los informes locales de M1/M2/M3 (`DSP_BASELINE.md`, `REALTIME_STABILIZATION.md`, `M3_OFFLINE_ARCHITECTURE.md`) que tu agente ha creado en su Mac no están aquí, salvo lo que incluían los kits compartidos: deben conservarse en el repositorio local; no los reconstruimos ni los fingimos.

## Advertencias de interpretación

1. El módulo musical v0.1 usa medición espectral aproximada para pitch; no sustituye un tono de laboratorio.
2. `SHIFT2-main.zip` no es una implementación AAX compilada; la prueba C++ independiente indica fallos importantes. No integrar el prototipo en `src` de producción.
3. El render neutro y el original muestran offset de 2.880 muestras en una ventana comprobada; la causa no está atribuida todavía al DSP ni al PDC. La duración del neutro también difiere en 384 muestras. **El análisis ampliado detecta además divergencia localizada cerca de 16–16,7 segundos**, que necesita comprobación independiente (edición, automatización, bounce, audio DSP).
4. F-Form M2A hace fallback seguro ante `time_ratio != 1`; estos WAV no son prueba de que el procesador offline nuevo funcione.
