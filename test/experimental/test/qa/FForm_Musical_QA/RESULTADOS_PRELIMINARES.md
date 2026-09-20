# F-Form — Musical QA, primera evaluación (20-09-2026)

## Alcance

Se han leído siete WAV entregados por el usuario. Ninguno es un WAV inequívocamente identificado como **original sin plugin**. Se usa el render neutro (`ITCH-01`, TIME 1.00×, PITCH 1.00×) como referencia provisional para comparaciones, no como sustituto válido de la fuente original para medir transparencia DSP.

No se ha compilado F-Form ni descargado SHIFT2: los accesos git de este entorno fallaron por DNS. Estas mediciones se refieren exclusivamente a los WAV aportados.

## Metadatos y validez de archivos

| Archivo | Duración | Fs | Canales contenedor | Canales con señal | Peak máx. (lineal) |
|---|---:|---:|---:|---|---:|
| ITCH-01 / neutral | 145.760 s | 48 kHz | 8 | 1,2 | 0.8902 |
| PITCH-02 / 1.19× | 145.760 s | 48 kHz | 8 | 1,2 | 0.8255 |
| PITCH-03 / 0.84× | 145.760 s | 48 kHz | 8 | 1,2 | 0.9157 |
| PITCH-04 / 2.00× | 145.760 s | 48 kHz | 8 | 1,2 | 0.7121 |
| BYP-01 / internal Enabled | 44.801 s | 48 kHz | 2 | 1,2 | 0.7967 |
| BYP-02 / host bypass | 33.281 s | 48 kHz | 2 | 1,2 | 0.5904 |
| LAT-01 / return recording | 59.881 s | 48 kHz | 2 | 1,2 | 0.7967 |

Todos los archivos contienen muestras finitas y ningún sample con magnitud >= 1.0. Esto **no excluye** distorsión espectral, transitorios dañados, aliasing, true-peak intersample clipping o glitches por transición. Se ha escuchado una selección de fragmentos, no se ha realizado una valoración auditiva ciega integral.

Los renders largos están almacenados como **8 canales, seis de ellos completamente silenciosos**. No son una prueba de que el plugin haya procesado realmente 7.1: comprueba el bus de bounce y exporta estéreo interleaved para próximas pruebas.

## Pitch relativo al render neutro

Análisis de correlación de espectros musicales promediados entre 12 y 30 segundos, frecuencia de muestreo 48 kHz, rejilla 5 cents:

| Archivo | Desplazamiento espectral estimado | Coeficiente de correlación del perfil |
|---|---:|---:|
| PITCH-02 / 1.19× | +305 cents aprox. | 0.961 |
| PITCH-03 / 0.84× | -300 cents aprox. | 0.964 |
| PITCH-04 / 2.00× | +1200 cents aprox. | 0.949 |

Estas cifras demuestran desplazamientos de contenido espectral aproximadamente acordes con los controles. No constituyen una medida precisa de cents de una nota aislada ni una medida de calidad perceptual.

## Comparaciones de identidad temporal parciales

En la ventana musical 5–6 segundos, LAT-01 y BYP-01 coinciden con el neutro con correlación de forma de onda ~0.9999996 y un residuo alrededor de -112 dB relativo, consistente con un render muy próximo al mismo contenido en ese intervalo. Existen otras ventanas posteriores con diferencias mayores en ambos archivos: son grabaciones con cambios de estado y no deben compararse como si fueran un render estático.

La primera muestra mayor que 1e-6 aparece en el render neutro alrededor del sample 100728 (2.099 s) y en los renders pitch-shift alrededor de los samples 95847–96116 (1.997–2.002 s). Diferencias de umbral de onset **no** prueban que el plugin tenga latencia negativa o que su PDC sea incorrecta; el análisis espectral puede producir pre-echo / energía adelantada. Necesitamos original seco, procedimiento exacto de grabación y marcadores de inicio para atribuir causas.

## Pendientes críticos

1. WAV original sin F-Form, mismo tramo y exportación.
2. Renders nuevos en estéreo, preservando inicio y fin y sin normalización ni fades.
3. Vídeo o captura de retorno realtime para medir demora tras Play; los bounces solos no verifican tiempo de arranque real ni PDC del DAW.
4. WAV de time-stretch **offline** con duración efectivamente distinta: no presentes fallbacks de realtime como time-stretch medido.
5. Paquete ZIP de SHIFT2 para auditar su implementación C++ real; la lista de archivos modificados que compartió Gemini solo menciona `src/data/cppCodebase.ts`, no `src/FFormDSPCore.h` en un repositorio JUCE.
