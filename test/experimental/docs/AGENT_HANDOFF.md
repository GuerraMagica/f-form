# Encargo para Codex / Copilot — QA F-Form

1. Lee `README.md`, `test/qa/FForm_QA_Kit/AGENT_BRIEF.md`, el informe musical y `experimental/SHIFT2/SHIFT2_Informe_Tecnico.md`. Registra tu commit HEAD.
2. Verifica hashes con `python verify_manifest.py` y corre las 5 regresiones sintéticas; comprueba realmente el número de tests.
3. Analiza `00_ORIGINAL.wav` frente a `ITCH-01` y los renders de pitch, preservando el WAV original. Compara con el nuevo `analyze_full.py`.
4. Investiga el offset musical medido de 2880 frames y la diferencia de duración total de 384 frames sin asumir causa interna; analiza por separado el bypass nativo con marcas reales de transición y PDC.
5. Inspecciona `SHIFT2-main.zip` sin mezclarlo en producción. Extrae y compila lo que sea posible, valida el motor con identidad, pitch, duración, stereo y condiciones límite. No acepta marketing como evidencias.
6. Nunca hagas push de `local_audio/` o material licenciado. No toques motor realtime M2A hasta nuevo encargo. Para offline diseña y valida backend Signalsmith aislado.
7. Distingue datos medidos, hipótesis y tareas pendientes. No anuncies PASS de tests no ejecutados.
