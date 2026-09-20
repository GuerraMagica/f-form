# F-Form Musical QA — original incorporado

Original: 6996096 frames; neutro: 6996480 frames (diferencia 384).

Offset estimado entre exportaciones: **2880 frames / 60.000 ms** (NO atribuir todavía a PDC o al DSP).

| Inicio ventana (s) | Ganancia ajustada | Null relativo (dB) | Correlación L |
|---:|---:|---:|---:|
| 5 | 0.999999937864076 | -111.82 | 0.999999999997 |
| 15 | 0.9729670107777484 | -14.61 | 0.983452253289 |
| 30 | 0.9999999420339595 | -113.09 | 0.999999999998 |
| 60 | 0.9999999482536004 | -112.78 | 0.999999999998 |
| 100 | 0.999999947628401 | -114.16 | 0.999999999998 |
| 130 | 0.9999999454271676 | -110.67 | 0.999999999996 |

### Control localizado en torno a 16 s (ventanas 100 ms)

| Inicio (s) | Null relativo (dB) |
|---:|---:|
| 14.5 | -111.97 |
| 14.7 | -115.08 |
| 14.9 | -113.64 |
| 15.1 | -109.46 |
| 15.3 | -111.36 |
| 15.5 | -113.67 |
| 15.7 | -113.47 |
| 15.9 | -5.39 |
| 16.1 | 66.31 |
| 16.3 | 21.13 |
| 16.5 | 19.91 |
| 16.7 | -111.67 |
| 16.9 | -107.99 |

**Atención:** hay una divergencia localizada alrededor de 16–16,7 s pese a que el resto de ventanas neutras casi anulan. No atribuirla al DSP sin examinar sesión, edición, automatización y sincronía de exportación.

Los WAV de ocho canales se analizan mediante sus canales 1 y 2, sin alterarlos. La estimación de pitch musical incluida en el JSON procede de `analyze.py`, con una resolución aproximada de 5 cents. No hay en este paquete un render de time-stretch offline con cambio de duración. El código SHIFT2 de Gemini no está instalado en F-Form y no es una prueba AAX.
