# ArtisVox One — Cadena vocal todo en uno (ArtiVox)

Plugin VST3 / AU de cadena vocal completa: afinación, Vocal Assist, limpieza, EQ, de-esser, compresión, color, tono, reverb, delay y doblador en una sola interfaz, con vista **Simple** y **Avanzado**, en **español, inglés y portugués (BR)**.

## Funciones

| Módulo | Motor |
|---|---|
| **Vocal Assist** | Eliges género (Trap Hard-Tune, Trap Melódico, Latin Trap, Reggaetón, Rap, Drill, R&B, Pop Urbano, Balada, Voz Hablada), canta 15 s y aplica la cadena profesional de ese estilo ajustada a tu toma: ganancia a -18 dBFS RMS, filtro de graves según tu nota más grave, puerta si hay ruido, EQ = perfil del género + corrección de tu voz, de-esser según tus eses, umbral del compresor calculado para la reducción típica del género, reverb limitada a 1 compás del BPM y tonalidad detectada. Ventana con espectro en vivo y estrellas. |
| **Afinación (ArtisTune)** | YIN + **TD-PSOLA**: mide el tono ciclo a ciclo y recoloca cada ciclo al periodo corregido, conservando los formantes (timbre). En la prueba: voz con deriva y vibrato de 28.7 cents de error medio → 1.0 cent. Velocidad 0 ms = hard-tune. |
| **Compresor** | 4 modelos: Limpio (VCA), Vintage (opto tipo LA-2A con liberación en dos etapas), Agresivo (FET tipo 1176, ataque 20–800 µs, realimentación) y **Serie FET→Opto** (la cadena estándar de estudio). Filtro de 100 Hz en la detección y mezcla paralela. |
| **De-esser** | Bandas Linkwitz-Riley de 4.º orden, detección relativa (sibilancia frente a la voz entera) y lookahead de 1.5 ms. |
| **Color (ArtisColor)** | Sobremuestreo x4. Cinta (pre/de-énfasis, head bump, pérdida de agudos), Válvula (polarización dinámica, armónicos pares), Drive (transformador), Lo-Fi. Compensación automática de nivel. |
| **Reverb (ArtisVerb)** | FDN de 8 líneas (Householder) + difusor Hadamard de 4 etapas + modulación + reflexiones tempranas. Nube, Plate, Sala, Cuarto. RT60 real en segundos (medido: error < 3 %). |
| **Delay** | Sincronizado al BPM, ping-pong, filtros, saturación de cinta y "wow" en las repeticiones, ducking. |

**Perfiles de referencia:** `tools/analyze_ref.py` mide acapellas profesionales (bandas, dinámica, afinación, tonalidad) y los valores se guardan en `Source/Presets.h` (`GenreProfile`). Con perfil, el Assist simula la cadena sobre tu toma en segundo plano y corrige EQ, de-esser y compresión hasta parecerse a la referencia. Perfiles cargados: Latin Trap (3 referencias), Reggaetón (3 referencias). La energía total se mide sin los sub-graves (< 80 Hz) para que los restos del 808 de las acapellas separadas no falseen el balance.

**Presets de fábrica (basados en ajustes habituales de la industria urbana):** Trap Hard-Tune, Trap Melódico, Trap Emo, Latin Trap Oscuro, Reggaetón Brillante, Reggaetón Romántico, Rap Seco, Rap Agresivo, Drill, R&B Suave, Pop Urbano, Ad-libs Ancho, Balada Acústica, Voz Hablada.

Latencia: ~39 ms en Studio y ~31 ms en Live a 48 kHz (la compensa el DAW). CPU: ~8.5 % de un núcleo con la cadena completa.

**Presets de usuario:** se guardan en `Documentos/ArtiVox/ArtisVox One/Presets/*.avpreset`.

## Compilar e instalador (Windows)

1. Doble clic en **`compilar.bat`** → carpeta `Compilado` (VST3 + programa independiente). Al final prueba el motor de audio.
2. Doble clic en **`crear_instalador.bat`** → `Instalador\ArtiVox_ArtisVoxOne_Setup.exe` (Inno Setup 7 o 6.3+).

Detalles en `LEEME.txt`. Mac: acción de GitHub (`.github/workflows/build.yml`), descarga el `.pkg` en Artifacts.

## Estructura

```
Source/
  Parameters.h        Todos los parámetros (IDs, rangos, valores por defecto)
  Presets.h           Presets de fábrica
  Translations.h      Textos ES / EN / PT-BR
  PluginProcessor.*   Cadena de audio, latencia, presets, Vocal Assist
  PluginEditor.*      Interfaz (Simple / Avanzado, pestañas, medidores)
  LookAndFeel.h       Estética ArtiVox (negro, rosa, morado)
  DSP/PitchCorrector.h  Afinación
  DSP/Dynamics.h        Puerta, de-esser, compresor, limitador
  DSP/Effects.h         Color, excitador, doblador, delay, reverb
  DSP/EqMath.h          Coeficientes de EQ (audio + curva)
  DSP/VocalAssist.h     Análisis de 15 s y propuesta de ajustes
```

**Cambiar el logo:** reemplaza `Resources/logo.png` (PNG con fondo transparente) y vuelve a compilar.

## Licencia de JUCE

JUCE 8 es gratis para vender plugins con la licencia **JUCE Starter** mientras lo facturado con plugins JUCE en los últimos 12 meses no pase de 20.000 USD; por encima hay que pasar a la licencia Indie (de pago).
