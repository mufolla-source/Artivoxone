#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

// ============================================================================
//  Presets de fabrica: cadenas vocales completas por estilo urbano.
//  Valores basados en ajustes habituales de la industria (2025-2026):
//   - Afinacion: hard-tune trap 0-5 ms; trap melodico 10-25 ms; latin trap ~8 ms
//     con humanize ~15; reggaeton 20-30 ms; drill 25-40 ms; R&B/pop 20-40 ms;
//     natural 40-60 ms.
//   - Limpieza/EQ: filtro de graves 80-110 Hz, recorte de 2-4 dB en 200-400 Hz,
//     +1.5-2.5 dB en ~3 kHz y realce de aire por encima de 10 kHz.
//   - De-esser 6-10 kHz con ~2-4 dB de reduccion.
//   - Compresion en serie FET 4:1 rapido (4-7 dB) -> opto (2-3 dB); drill 5:1 rapido.
//   - Reverb: plate 1-2.5 s / pre-delay 15-30 ms para trap, rap y pop; hall 2-4 s /
//     pre-delay 25-45 ms para R&B y baladas; reggaeton con espacios cortos.
//   - Delay sincronizado (1/4, 1/8, 1/8 con puntillo) y ducking.
//  Unidades reales (dB, Hz, %, ms, s, indice). Lo que no aparece vuelve a su valor
//  por defecto. La tonalidad, la escala y la ganancia de entrada no se tocan.
// ============================================================================
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<const char*, float>> values;
};

namespace PresetIdx
{
    enum { Default = 0, TrapHard, TrapMelodic, Emo, LatinTrap, Reggaeton, ReggaetonRomantic, Rap, RapAggressive,
           Drill, RnB, PopUrban, Adlibs, Ballad, Spoken, Count };
}

inline const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace PID;
    // Estilos: compStyle 0 Limpio, 1 Vintage(opto), 2 Agresivo(FET), 3 Serie(FET->opto)
    //          satType 0 Cinta, 1 Valvula, 2 Drive, 3 Lo-Fi
    //          spaceStyle 0 Nube, 1 Plate, 2 Sala, 3 Cuarto
    //          dlyDiv 0 1/2, 1 1/4, 2 1/4., 3 1/8, 4 1/8., 5 1/8T, 6 1/16
    static const std::vector<FactoryPreset> presets =
    {
        { "Por defecto", {} },

        { "Trap Hard-Tune", {
            { tuneOn, 1 }, { tuneSpeed, 0 }, { tuneHuman, 0 }, { tuneAmount, 100 }, { tuneStyle, 3 },
            { hpfFreq, 100 }, { mudFreq, 300 }, { mudGain, -3 }, { presFreq, 3000 }, { presGain, 2 }, { airFreq, 12000 }, { airGain, 3 },
            { deessAmount, 40 }, { deessFreq, 7500 },
            { compStyle, 3 }, { compAmount, 52 }, { compRatio, 4 },
            { satDrive, 25 }, { satType, 0 }, { toneExciter, 15 },
            { spaceStyle, 2 }, { revDecay, 2.4f }, { revPre, 25 }, { revMix, 20 }, { revSize, 70 }, { revDamp, 45 },
            { dlyDiv, 1 }, { dlyMix, 18 }, { dlyFeedback, 40 }, { dlyPingPong, 1 }, { spaceDuck, 55 }, { width, 120 } } },

        { juce::String::fromUTF8 ("Trap Melódico"), {
            { tuneOn, 1 }, { tuneSpeed, 12 }, { tuneHuman, 10 }, { tuneStyle, 0 },
            { hpfFreq, 95 }, { mudGain, -2.5f }, { presGain, 2 }, { airFreq, 12000 }, { airGain, 3.5f },
            { deessAmount, 40 }, { deessFreq, 7500 },
            { compStyle, 3 }, { compAmount, 50 }, { compRatio, 4 },
            { satDrive, 18 }, { satType, 0 }, { toneExciter, 15 },
            { spaceStyle, 1 }, { revDecay, 1.8f }, { revPre, 20 }, { revMix, 18 },
            { dlyDiv, 3 }, { dlyMix, 12 }, { dlyFeedback, 30 }, { spaceDuck, 50 }, { width, 115 } } },

        { juce::String::fromUTF8 ("Trap Emo / Melancólico"), {
            { tuneOn, 1 }, { tuneSpeed, 12 }, { tuneHuman, 0 }, { tuneStyle, 0 },
            { hpfFreq, 90 }, { mudGain, -2 }, { presGain, 1.5f }, { airGain, 2 },
            { deessAmount, 38 },
            { compStyle, 3 }, { compAmount, 48 }, { compRatio, 4 },
            { satDrive, 15 }, { satType, 1 },
            { spaceStyle, 2 }, { revDecay, 2.8f }, { revPre, 35 }, { revMix, 25 }, { revDamp, 55 },
            { dlyDiv, 2 }, { dlyMix, 15 }, { dlyFeedback, 40 }, { spaceDuck, 50 }, { width, 120 } } },

        { "Latin Trap Oscuro", {
            { tuneOn, 1 }, { tuneSpeed, 10 }, { tuneHuman, 15 }, { tuneStyle, 0 },
            { hpfFreq, 100 }, { lowGain, 1 }, { mudGain, -2.5f }, { presGain, 1 }, { airGain, 1 }, { toneTilt, -1.5f }, { toneStyle, 2 },
            { deessAmount, 45 },
            { compStyle, 3 }, { compAmount, 58 }, { compRatio, 5 },
            { satDrive, 35 }, { satType, 1 },
            { spaceStyle, 1 }, { revDecay, 1.6f }, { revPre, 20 }, { revMix, 15 }, { revDamp, 60 },
            { dlyDiv, 4 }, { dlyMix, 14 }, { dlyFeedback, 35 }, { spaceDuck, 55 } } },

        { juce::String::fromUTF8 ("Reggaetón Brillante"), {
            { tuneOn, 1 }, { tuneSpeed, 15 }, { tuneHuman, 15 }, { tuneStyle, 1 },   // refs medidas: 55-76 % de la voz a < 10 cents
            { hpfFreq, 110 }, { mudFreq, 300 }, { mudGain, -3 }, { presFreq, 3500 }, { presGain, 2.5f }, { airFreq, 12000 }, { airGain, 4.5f },
            { toneStyle, 0 }, { toneExciter, 25 },
            { deessAmount, 45 }, { deessFreq, 7000 },
            { compStyle, 3 }, { compAmount, 52 }, { compRatio, 4 },
            { satDrive, 12 }, { satType, 0 },
            { spaceStyle, 1 }, { revDecay, 1.2f }, { revPre, 15 }, { revMix, 12 },
            { dlyDiv, 3 }, { dlyMix, 8 }, { dlyFeedback, 25 }, { spaceDuck, 50 } } },

        { juce::String::fromUTF8 ("Reggaetón Romántico"), {
            { tuneOn, 1 }, { tuneSpeed, 30 }, { tuneHuman, 35 }, { tuneStyle, 1 },
            { hpfFreq, 100 }, { mudGain, -2 }, { presGain, 2 }, { airGain, 4 }, { toneExciter, 15 },
            { deessAmount, 40 },
            { compStyle, 3 }, { compAmount, 45 }, { compRatio, 3.5f },
            { satDrive, 10 }, { satType, 1 },
            { spaceStyle, 1 }, { revDecay, 1.6f }, { revPre, 20 }, { revMix, 16 },
            { dlyDiv, 1 }, { dlyMix, 10 }, { dlyFeedback, 30 }, { spaceDuck, 45 } } },

        { "Rap Seco", {
            { tuneOn, 0 }, { tuneSpeed, 40 },
            { hpfFreq, 90 }, { lowGain, 1 }, { mudFreq, 250 }, { mudGain, -3 }, { presGain, 2.5f }, { airGain, 2 },
            { deessAmount, 40 }, { deessFreq, 7500 },
            { compStyle, 3 }, { compAmount, 62 }, { compRatio, 4 },
            { satDrive, 20 }, { satType, 2 }, { satMix, 60 },
            { spaceStyle, 3 }, { revDecay, 0.6f }, { revPre, 5 }, { revMix, 6 },
            { dlyMix, 0 }, { spaceDuck, 40 } } },

        { "Rap Agresivo", {
            { tuneOn, 0 },
            { hpfFreq, 100 }, { mudGain, -3 }, { presFreq, 3500 }, { presGain, 3 }, { airGain, 2 },
            { deessAmount, 45 },
            { compStyle, 2 }, { compAmount, 70 }, { compRatio, 8 }, { compAttack, 5 }, { compRelease, 80 },
            { satDrive, 30 }, { satType, 2 },
            { spaceStyle, 3 }, { revDecay, 0.5f }, { revMix, 5 },
            { dlyDiv, 6 }, { dlyMix, 6 }, { dlyFeedback, 10 } } },

        { "Drill", {
            { tuneOn, 1 }, { tuneSpeed, 32 }, { tuneHuman, 25 }, { tuneStyle, 1 },
            { hpfFreq, 110 }, { mudGain, -3 }, { presFreq, 3500 }, { presGain, 3 }, { airGain, 2 },
            { deessAmount, 45 },
            { compStyle, 2 }, { compAmount, 65 }, { compRatio, 5 }, { compAttack, 1 }, { compRelease, 100 },
            { satDrive, 25 }, { satType, 2 },
            { spaceStyle, 3 }, { revDecay, 0.8f }, { revMix, 7 },
            { dlyDiv, 5 }, { dlyMix, 8 }, { dlyFeedback, 20 } } },

        { "R&B Suave", {
            { tuneOn, 1 }, { tuneSpeed, 30 }, { tuneHuman, 45 }, { tuneStyle, 1 },
            { hpfFreq, 85 }, { lowFreq, 180 }, { lowGain, 1.5f }, { mudGain, -2 }, { presGain, 1.5f }, { airGain, 3.5f }, { toneExciter, 15 },
            { deessAmount, 38 },
            { compStyle, 1 }, { compAmount, 45 }, { compRatio, 3 }, { compRelease, 200 },
            { satDrive, 12 }, { satType, 1 },
            { spaceStyle, 2 }, { revDecay, 2.6f }, { revPre, 35 }, { revMix, 22 },
            { dlyDiv, 1 }, { dlyMix, 10 }, { dlyFeedback, 35 }, { spaceDuck, 45 },
            { doublerOn, 1 }, { doubler, 15 }, { width, 120 } } },

        { "Pop Urbano", {
            { tuneOn, 1 }, { tuneSpeed, 20 }, { tuneHuman, 30 },
            { hpfFreq, 100 }, { mudGain, -2 }, { presGain, 2 }, { airGain, 4 }, { toneExciter, 20 },
            { deessAmount, 40 },
            { compStyle, 3 }, { compAmount, 50 }, { compRatio, 4 },
            { satDrive, 12 },
            { spaceStyle, 1 }, { revDecay, 1.8f }, { revPre, 20 }, { revMix, 18 },
            { dlyDiv, 3 }, { dlyMix, 8 }, { spaceDuck, 50 } } },

        { "Ad-libs Ancho", {
            { tuneOn, 1 }, { tuneSpeed, 5 }, { tuneHuman, 0 },
            { hpfFreq, 220 }, { lowGain, -4 }, { presGain, 3 }, { airGain, 3 },
            { compStyle, 2 }, { compAmount, 65 }, { compRatio, 6 },
            { satDrive, 35 }, { satType, 2 },
            { doublerOn, 1 }, { doubler, 60 }, { doublerVoices, 1 }, { width, 160 },
            { spaceStyle, 1 }, { revDecay, 1.4f }, { revMix, 22 },
            { dlyDiv, 3 }, { dlyMix, 18 }, { dlyFeedback, 40 }, { outGain, -3 } } },

        { juce::String::fromUTF8 ("Balada Acústica"), {
            { tuneOn, 1 }, { tuneSpeed, 50 }, { tuneHuman, 60 }, { tuneAmount, 80 }, { tuneStyle, 2 },
            { hpfFreq, 75 }, { lowGain, 1 }, { mudGain, -1.5f }, { presGain, 1 }, { airGain, 2.5f },
            { compStyle, 1 }, { compAmount, 38 }, { compRatio, 3 }, { compRelease, 250 },
            { satDrive, 8 }, { satType, 1 },
            { spaceStyle, 2 }, { revDecay, 3.2f }, { revPre, 40 }, { revMix, 25 },
            { dlyMix, 0 } } },

        { "Voz Hablada", {
            { tuneOn, 0 }, { hpfFreq, 90 }, { gateThresh, -50 }, { mudGain, -2 }, { presGain, 2.5f },
            { deessAmount, 45 }, { compStyle, 0 }, { compAmount, 55 }, { compRatio, 3 },
            { revMix, 0 }, { dlyMix, 0 }, { satDrive, 5 } } },
    };
    return presets;
}

// ============================================================================
//  Perfil de referencia de un genero: como suena una voz TERMINADA de ese estilo,
//  medido en acapellas profesionales con tools/analyze_ref.py (mismos filtros que
//  el Vocal Assist). Bandas = energia relativa a la voz entera, en dB.
//  refs = numero de acapellas promediadas (0 = sin perfil todavia).
// ============================================================================
struct GenreProfile
{
    int refs = 0;
    float low = 0, mud = 0, pres = 0, sib = 0, air = 0;   // 120 Hz, 320 Hz, 3.2 kHz, 7 kHz, >11 kHz
    float dynRange = 6;                                    // p95 - mediana del nivel (dB)
};

// Generos del Vocal Assist -> preset base, reduccion de ganancia objetivo (dB) y perfil
struct AssistGenre { const char* key; int preset; float targetGr; GenreProfile profile {}; };
inline const std::vector<AssistGenre>& assistGenres()
{
    static const std::vector<AssistGenre> g = {
        { "Trap Hard-Tune",  PresetIdx::TrapHard,     5.5f },
        { "Trap Melodico",   PresetIdx::TrapMelodic,  5.0f },
        // Latin Trap: 3 referencias (4:22, 2:55, 0:29; pesos por duracion hasta 3 min: 47/46/7 %) - nov 2026
        { "Latin Trap",      PresetIdx::LatinTrap,    6.0f, { 3, -11.5f, -4.65f, -9.95f, -13.4f, -20.65f, 3.9f } },
        // Reggaeton: 3 referencias (0:23, 3:10, 2:50; pesos 15/45/40 % por duracion) - nov 2026
        { "Reggaeton",       PresetIdx::Reggaeton,    5.0f, { 3, -10.95f, -4.2f, -11.9f, -16.2f, -21.9f, 2.5f } },
        { "Rap",             PresetIdx::Rap,          6.5f },
        { "Drill",           PresetIdx::Drill,        7.0f },
        { "R&B",             PresetIdx::RnB,          4.0f },
        { "Pop Urbano",      PresetIdx::PopUrban,     5.0f },
        { "Balada",          PresetIdx::Ballad,       3.0f },
        { "Voz Hablada",     PresetIdx::Spoken,       5.0f },
    };
    return g;
}
