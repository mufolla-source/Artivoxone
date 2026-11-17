#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Identificadores de todos los parametros de ArtisVox One
namespace PID
{
    // Global
    inline constexpr const char* inGain      = "inGain";
    inline constexpr const char* outGain     = "outGain";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* before      = "before";      // Antes / Despues
    inline constexpr const char* limiter     = "limiter";

    // Afinacion (Tune)
    inline constexpr const char* tuneOn      = "tuneOn";
    inline constexpr const char* tuneKey     = "tuneKey";
    inline constexpr const char* tuneScale   = "tuneScale";
    inline constexpr const char* tuneSpeed   = "tuneSpeed";
    inline constexpr const char* tuneAmount  = "tuneAmount";
    inline constexpr const char* tuneHuman   = "tuneHuman";
    inline constexpr const char* tuneMode    = "tuneMode";    // Studio / Live

    // Limpieza
    inline constexpr const char* hpfFreq     = "hpfFreq";
    inline constexpr const char* gateThresh  = "gateThresh";

    // EQ (4 bandas)
    inline constexpr const char* lowFreq     = "lowFreq";
    inline constexpr const char* lowGain     = "lowGain";
    inline constexpr const char* mudFreq     = "mudFreq";
    inline constexpr const char* mudGain     = "mudGain";
    inline constexpr const char* presFreq    = "presFreq";
    inline constexpr const char* presGain    = "presGain";
    inline constexpr const char* airFreq     = "airFreq";
    inline constexpr const char* airGain     = "airGain";

    // Dinamica
    inline constexpr const char* deessAmount = "deessAmount";
    inline constexpr const char* deessFreq   = "deessFreq";
    inline constexpr const char* compAmount  = "compAmount";
    inline constexpr const char* compRatio   = "compRatio";
    inline constexpr const char* compAttack  = "compAttack";
    inline constexpr const char* compRelease = "compRelease";
    inline constexpr const char* compStyle   = "compStyle";
    inline constexpr const char* compMakeup  = "compMakeup";
    inline constexpr const char* compAuto    = "compAuto";

    // Tono
    inline constexpr const char* toneTilt    = "toneTilt";
    inline constexpr const char* toneExciter = "toneExciter";

    // Caracter
    inline constexpr const char* satDrive    = "satDrive";
    inline constexpr const char* satType     = "satType";
    inline constexpr const char* satMix      = "satMix";
    inline constexpr const char* doubler     = "doubler";

    // Espacio
    inline constexpr const char* revMix      = "revMix";
    inline constexpr const char* revSize     = "revSize";
    inline constexpr const char* revDamp     = "revDamp";
    inline constexpr const char* revPre      = "revPre";
    inline constexpr const char* dlyMix      = "dlyMix";
    inline constexpr const char* dlyDiv      = "dlyDiv";
    inline constexpr const char* dlyFeedback = "dlyFeedback";
    inline constexpr const char* dlyPingPong = "dlyPingPong";
    inline constexpr const char* spaceDuck   = "spaceDuck";

    // Interruptores de modulo, estilos y extras del diseno ArtisVox One
    inline constexpr const char* toneOn      = "toneOn";
    inline constexpr const char* spaceOn     = "spaceOn";
    inline constexpr const char* charOn      = "charOn";
    inline constexpr const char* compOn      = "compOn";
    inline constexpr const char* deessOn     = "deessOn";
    inline constexpr const char* doublerOn   = "doublerOn";
    inline constexpr const char* doublerVoices = "doublerVoices";
    inline constexpr const char* width       = "width";
    inline constexpr const char* tuneStyle   = "tuneStyle";
    inline constexpr const char* toneStyle   = "toneStyle";
    inline constexpr const char* spaceStyle  = "spaceStyle";
    inline constexpr const char* revDecay    = "revDecay";
    inline constexpr const char* compMix     = "compMix";
}

inline const juce::StringArray& keyNames()
{
    static juce::StringArray k { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return k;
}

inline const juce::StringArray& scaleNames()
{
    static juce::StringArray s { "Mayor", "Menor", "Cromatica", "Menor armonica", "Pentatonica menor", "Pentatonica mayor" };
    return s;
}

inline const juce::StringArray& delayDivNames()
{
    static juce::StringArray d { "1/2", "1/4", "1/4 .", "1/8", "1/8 .", "1/8 T", "1/16" };
    return d;
}

inline double delayDivBeats (int idx)
{
    static const double b[] = { 2.0, 1.0, 1.5, 0.5, 0.75, 1.0 / 3.0, 0.25 };
    return b[juce::jlimit (0, 6, idx)];
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto fl = [&] (const char* id, const char* name, float lo, float hi, float def, float skewCentre = 0.0f, const char* unit = "")
    {
        NormalisableRange<float> r (lo, hi);
        if (skewCentre > 0.0f) r.setSkewForCentre (skewCentre);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                        AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto bl = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto ch = [&] (const char* id, const char* name, const StringArray& items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    // Global
    fl (PID::inGain,  "Entrada", -24.0f, 24.0f, 0.0f, 0.0f, "dB");
    fl (PID::outGain, "Salida",  -24.0f, 24.0f, 0.0f, 0.0f, "dB");
    fl (PID::mix,     "Mezcla",  0.0f, 100.0f, 100.0f, 0.0f, "%");
    bl (PID::before,  "Antes", false);
    bl (PID::limiter, "Limitador", true);

    // Afinacion
    bl (PID::tuneOn,     "Afinacion On", true);
    ch (PID::tuneKey,    "Tonalidad", keyNames(), 0);
    ch (PID::tuneScale,  "Escala", scaleNames(), 1);
    fl (PID::tuneSpeed,  "Velocidad", 0.0f, 200.0f, 20.0f, 40.0f, "ms");
    fl (PID::tuneAmount, "Cantidad Afinacion", 0.0f, 100.0f, 100.0f, 0.0f, "%");
    fl (PID::tuneHuman,  "Humanizar", 0.0f, 100.0f, 20.0f, 0.0f, "%");
    ch (PID::tuneMode,   "Modo Afinacion", StringArray { "Studio", "Live" }, 0);

    // Limpieza
    fl (PID::hpfFreq,    "Filtro graves", 20.0f, 400.0f, 80.0f, 100.0f, "Hz");
    fl (PID::gateThresh, "Puerta", -80.0f, -20.0f, -80.0f, 0.0f, "dB");

    // EQ
    fl (PID::lowFreq,  "Cuerpo Frec",    60.0f, 400.0f, 150.0f, 150.0f, "Hz");
    fl (PID::lowGain,  "Cuerpo",         -12.0f, 12.0f, 0.0f, 0.0f, "dB");
    fl (PID::mudFreq,  "Barro Frec",     150.0f, 1000.0f, 350.0f, 350.0f, "Hz");
    fl (PID::mudGain,  "Barro",          -12.0f, 12.0f, 0.0f, 0.0f, "dB");
    fl (PID::presFreq, "Presencia Frec", 1000.0f, 6000.0f, 3000.0f, 2500.0f, "Hz");
    fl (PID::presGain, "Presencia",      -12.0f, 12.0f, 0.0f, 0.0f, "dB");
    fl (PID::airFreq,  "Brillo Frec",    6000.0f, 18000.0f, 10000.0f, 10000.0f, "Hz");
    fl (PID::airGain,  "Brillo",         -12.0f, 12.0f, 0.0f, 0.0f, "dB");

    // Dinamica
    fl (PID::deessAmount, "De-esser",       0.0f, 100.0f, 30.0f, 0.0f, "%");
    fl (PID::deessFreq,   "De-esser Frec",  3000.0f, 12000.0f, 7000.0f, 6500.0f, "Hz");
    fl (PID::compAmount,  "Compresion",     0.0f, 100.0f, 40.0f, 0.0f, "%");
    fl (PID::compRatio,   "Ratio",          1.0f, 20.0f, 4.0f, 4.0f, ":1");
    fl (PID::compAttack,  "Ataque",         0.1f, 100.0f, 8.0f, 10.0f, "ms");
    fl (PID::compRelease, "Liberacion",     10.0f, 1000.0f, 120.0f, 120.0f, "ms");
    ch (PID::compStyle,   "Estilo Comp", StringArray { "Limpio", "Vintage", "Agresivo", "Serie" }, 3);
    fl (PID::compMakeup,  "Ganancia Comp",  0.0f, 24.0f, 0.0f, 0.0f, "dB");
    bl (PID::compAuto,    "Ganancia Auto", true);

    // Tono
    fl (PID::toneTilt,    "Tono",   -6.0f, 6.0f, 0.0f, 0.0f, "dB");
    fl (PID::toneExciter, "Aire",   0.0f, 100.0f, 0.0f, 0.0f, "%");

    // Caracter
    fl (PID::satDrive, "Color",        0.0f, 100.0f, 15.0f, 0.0f, "%");
    ch (PID::satType,  "Tipo Color", StringArray { "Cinta", "Valvula", "Drive", "Lo-Fi" }, 0);
    fl (PID::satMix,   "Mezcla Color", 0.0f, 100.0f, 100.0f, 0.0f, "%");
    fl (PID::doubler,  "Doblador",     0.0f, 100.0f, 35.0f, 0.0f, "%");

    // Espacio
    fl (PID::revMix,      "Reverb",       0.0f, 100.0f, 10.0f, 0.0f, "%");
    fl (PID::revSize,     "Tamano",       0.0f, 100.0f, 50.0f, 0.0f, "%");
    fl (PID::revDamp,     "Oscuridad",    0.0f, 100.0f, 50.0f, 0.0f, "%");
    fl (PID::revPre,      "Pre-delay",    0.0f, 200.0f, 20.0f, 0.0f, "ms");
    fl (PID::dlyMix,      "Delay",        0.0f, 100.0f, 0.0f, 0.0f, "%");
    ch (PID::dlyDiv,      "Division", delayDivNames(), 3);
    fl (PID::dlyFeedback, "Repeticiones", 0.0f, 90.0f, 30.0f, 0.0f, "%");
    bl (PID::dlyPingPong, "Ping-Pong", true);
    fl (PID::spaceDuck,   "Ducking",      0.0f, 100.0f, 40.0f, 0.0f, "%");

    // Modulos
    bl (PID::toneOn,  "Tono On", true);
    bl (PID::spaceOn, "Espacio On", true);
    bl (PID::charOn,  "Caracter On", true);
    bl (PID::compOn,  "Compresor On", true);
    bl (PID::deessOn, "De-esser On", true);
    bl (PID::doublerOn, "Doubler On", false);
    ch (PID::doublerVoices, "Voces Doubler", StringArray { "2", "4" }, 0);
    fl (PID::width, "Ancho", 0.0f, 200.0f, 100.0f, 0.0f, "%");
    ch (PID::tuneStyle,  "Estilo Afinacion", StringArray { "Rapido", "Natural", "Suave", "Robot" }, 1);
    ch (PID::toneStyle,  "Estilo Tono", StringArray { "Brillante", "Calido", "Oscuro", "Neutro" }, 3);
    ch (PID::spaceStyle, "Estilo Espacio", StringArray { "Nube", "Plate", "Sala", "Cuarto" }, 1);

    fl (PID::revDecay, "Caida Reverb", 0.3f, 8.0f, 1.6f, 1.6f, "s");
    fl (PID::compMix,  "Mezcla Comp", 0.0f, 100.0f, 100.0f, 0.0f, "%");

    return { p.begin(), p.end() };
}
