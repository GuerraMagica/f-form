# F-Form

Plugin de time-stretching y pitch-shifting de Guerra Magica Audio, desarrollado con JUCE/C++ para macOS.

## Estructura sugerida

- `src/` : código del plugin y DSP base.
- `include/` : headers de librerías internas o utilidades compartidas.
- `vendor/` : librerías externas (Rubberband, SoundTouch, FFT wrappers, etc.).
- `build/` : compilación local.
- `resources/` : assets UI y recursos del plugin.

## Dependencias y licencias

El motor DSP utiliza [Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch), distribuido bajo licencia MIT, junto con Signalsmith Linear. Las licencias y avisos de estas dependencias deben conservarse al publicar el proyecto.

## Requisitos del sistema

- macOS 12+ recomendado.
- Xcode Command Line Tools.
- CMake 3.22+
- Ninja (o Xcode generator)
- Homebrew para instalar dependencias.

## Instalación en macOS Apple Silicon e Intel

### 1. Instalar Xcode

```bash
xcode-select --install
```

Luego acepta la licencia:

```bash
sudo xcodebuild -license accept
```

### 2. Instalar Homebrew

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

Si tu usuario es administrador, añade Homebrew a tu PATH:

```bash
(eval "$($(which brew) shellenv)")
```

### 3. Instalar herramientas

```bash
brew install cmake ninja git
```

### 4. Generar el proyecto

```bash
mkdir -p build
cd build
cmake -G Ninja ..
cmake --build .
```

### 5. Compilar universal (Apple Silicon + Intel)

En macOS, para empacar un binario universal:

```bash
cmake -G Ninja -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" ..
cmake --build .
```

Esto permite generar artefactos compatibles con ambos tipos de hardware Apple.

## Objetivo del proyecto

La meta es producir un algoritmo de time-stretch/pitch-shift con calidad de estudio, basado en técnicas como:

- Phase Vocoder avanzado
- Overlap-Add con análisis de fase
- WSOLA / PSOLA para transitorios
- Preservación de formantes
- Control de artefactos y sedación de banda de transiciones

## Fase 1 recomendada

1. Crear un audio processor funcional con entrada/salida.
2. Consensuar un buffer de audio por bloque.
3. Integrar un motor DSP placeholder.
4. Añadir parámetros de pitch y time.
5. Probar latencia, estabilidad y calidad.

La primera integración del motor ya está conectada al flujo de audio mediante una FIFO por canales. El motor controla de forma independiente el ratio temporal y el pitch, pero todavía requiere pruebas auditivas y mediciones con material real antes de considerarse una versión de producción.

## Siguientes pasos

- Integrar una librería de referencia para validación rápida.
- Añadir un UI de control mínimo.
- Medir artefactos y transitorios.
- Probar en DAW reales.

## Recomendación técnica inicial

Para la primera validación, es razonable usar una librería de análisis/síntesis como Rubber Band o SoundTouch en la fase prototipo, y dejar el algoritmo propio como segunda etapa de producción.
