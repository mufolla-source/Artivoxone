#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <atomic>

namespace avdsp
{
inline float dbToGain (float db)  { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)   { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float msCoeff (float ms, double sr) { return ms <= 0.0f ? 0.0f : (float) std::exp (-1.0 / (ms * 0.001 * sr)); }

// ---------------------------------------------------------------------------
// Puerta de ruido / expansor con histeresis (limpieza de fondo)
// ---------------------------------------------------------------------------
class Gate
{
public:
    void prepare (double s) { sr = s; env = 0.0f; gain = 1.0f; holdCount = 0; open = true; }
    void setThreshold (float db) { thrDb = db; }

    void process (float* x, int n)
    {
        if (thrDb <= -79.5f) { gain = 1.0f; return; }
        const float openThr  = dbToGain (thrDb);
        const float closeThr = dbToGain (thrDb - 4.0f);
        const float envRel = msCoeff (40.0f, sr);
        const float att = msCoeff (1.0f, sr), rel = msCoeff (120.0f, sr);
        const int holdLen = (int) (0.06 * sr);
        const float floorG = dbToGain (-40.0f);

        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (x[i]);
            env = a > env ? a : a + (env - a) * envRel;

            if (env > openThr) { open = true; holdCount = holdLen; }
            else if (env < closeThr) { if (holdCount > 0) --holdCount; else open = false; }

            const float target = open ? 1.0f : floorG;
            const float c = target > gain ? att : rel;
            gain = target + (gain - target) * c;
            x[i] *= gain;
        }
    }
    float getGainDb() const { return gainToDb (gain); }

private:
    double sr = 44100.0;
    float thrDb = -80.0f, env = 0.0f, gain = 1.0f;
    int holdCount = 0; bool open = true;
};

// ---------------------------------------------------------------------------
// De-esser profesional
//  - Division de bandas Linkwitz-Riley de 4.o orden (suma plana, sin colorear)
//  - Deteccion RELATIVA: compara la energia sibilante con la de la voz entera,
//    asi funciona igual con voces fuertes o suaves (como los de-essers de gama alta)
//  - Lookahead de 1.5 ms: atrapa el inicio de la "s" antes de que suene
//  - Solo reduce la banda aguda: el cuerpo de la voz no se toca
// ---------------------------------------------------------------------------
class DeEsser
{
public:
    void prepare (double s)
    {
        sr = s;
        la = (int) std::round (0.0015 * sr);
        delay.assign ((size_t) juce::nextPowerOfTwo (la + 4), 0.0f); dmask = (int) delay.size() - 1; dw = 0;
        envS = envF = 0.0f; gr = 1.0f; lastFreq = -1.0f;
        for (auto* f : { &lp1, &lp2, &hp1, &hp2, &det }) f->reset();
    }
    int getLatencySamples() const { return la; }

    void setParams (float amountPct, float freq)
    {
        amount = amountPct;
        if (std::abs (freq - lastFreq) > 1.0f)
        {
            const float f = juce::jmin (freq, (float) sr * 0.42f);
            auto lpc = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, f, 0.7071f);
            auto hpc = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, f, 0.7071f);
            lp1.coefficients = lpc; lp2.coefficients = lpc; hp1.coefficients = hpc; hp2.coefficients = hpc;
            det.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, f * 0.85f, 0.9f);
            lastFreq = freq;
        }
    }

    void process (float* x, int n, bool active = true)
    {
        const float thrRel = -6.0f - amount * 0.14f;
        const float maxRed = 4.0f + amount * 0.10f;
        const float envA = msCoeff (0.2f, sr), envR = msCoeff (30.0f, sr), grRel = msCoeff (45.0f, sr);
        float maxGr = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float in = x[i];
            // Deteccion sobre la senal sin retrasar (lookahead)
            const float sd = det.processSample (in);
            const float s2 = sd * sd, f2 = in * in;
            envS = s2 > envS ? s2 + (envS - s2) * envA : s2 + (envS - s2) * envR;
            envF = f2 > envF ? f2 + (envF - f2) * envA : f2 + (envF - f2) * envR;
            const float fullDb = 10.0f * std::log10 (envF + 1e-12f);
            const float relDb = 10.0f * std::log10 ((envS + 1e-12f) / (envF + 1e-12f));

            float redDb = 0.0f;
            if (active && amount > 0.01f && fullDb > -50.0f && relDb > thrRel)
                redDb = juce::jmin (maxRed, (relDb - thrRel) * 0.6f);
            const float tg = dbToGain (-redDb);
            gr = tg < gr ? tg : tg + (gr - tg) * grRel;

            // Audio retrasado, separado en bandas LR4
            delay[(size_t) dw] = in;
            const float dly = delay[(size_t) ((dw - la) & dmask)];
            dw = (dw + 1) & dmask;
            const float lo = lp2.processSample (lp1.processSample (dly));
            const float hi = hp2.processSample (hp1.processSample (dly));
            x[i] = lo + hi * gr;
            maxGr = std::max (maxGr, -gainToDb (gr));
        }
        grDb.store (maxGr);
    }

    std::atomic<float> grDb { 0.0f };

private:
    double sr = 44100.0;
    int la = 64, dw = 0, dmask = 127;
    std::vector<float> delay;
    float amount = 30.0f, envS = 0.0f, envF = 0.0f, gr = 1.0f, lastFreq = -1.0f;
    juce::dsp::IIR::Filter<float> lp1, lp2, hp1, hp2, det;
};

// ---------------------------------------------------------------------------
// Compresor vocal profesional con 4 modelos
//  0 Limpio   (VCA): transparente, rodilla suave, tiempos ajustables
//  1 Vintage  (opto tipo LA-2A): ataque ~10 ms, liberacion en dos etapas que
//             depende de cuanto tiempo lleva comprimiendo, ratio dependiente del
//             programa y deteccion con enfasis en medios-agudos; color de valvula
//  2 Agresivo (FET tipo 1176): ataque ultrarrapido (20-800 us), deteccion por
//             realimentacion, rodilla dura y armonicos impares
//  3 Serie    (FET -> Opto): la cadena clasica de los estudios urbanos: el FET
//             atrapa los picos (4:1) y el opto empareja el cuerpo
//  Todos con filtro de 100 Hz en la deteccion (las "p" no bombean) y mezcla paralela.
// ---------------------------------------------------------------------------
class Compressor
{
public:
    void prepare (double s)
    {
        sr = s;
        for (auto* st : { &a, &b }) st->prepare (sr);
    }

    void setParams (float amountPct, float ratio_, float attackMs, float releaseMs, int style_, float makeupDb, bool autoMakeup, float mixPct = 100.0f)
    {
        style = juce::jlimit (0, 3, style_);
        const float thr = -amountPct * 0.42f;
        mix = mixPct * 0.01f;
        float mk = makeupDb;

        switch (style)
        {
            case 0: a.configure (Stage::VCA,  thr, ratio_, attackMs, releaseMs, 6.0f); break;
            case 1: a.configure (Stage::Opto, thr, ratio_, 10.0f, releaseMs, 10.0f); break;
            case 2: a.configure (Stage::FET,  thr, ratio_, 0.02f + juce::jlimit (0.0f, 1.0f, attackMs / 100.0f) * 0.78f, juce::jlimit (50.0f, 1100.0f, releaseMs), 1.0f); break;
            default:
                a.configure (Stage::FET,  thr + 4.0f, juce::jmax (4.0f, ratio_), 0.3f, 80.0f, 1.0f);
                b.configure (Stage::Opto, thr - 2.0f, 3.0f, 10.0f, juce::jmax (150.0f, releaseMs), 10.0f);
                break;
        }
        if (autoMakeup)
        {
            const float r = style == 3 ? 3.5f : ratio_;
            mk += (-thr) * (1.0f - 1.0f / r) * (style == 3 ? 0.55f : 0.45f);
        }
        makeup = dbToGain (mk);
    }

    void process (float* x, int n)
    {
        float maxGr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float dry = x[i];
            float y = a.process (dry);
            float g = a.grDb;
            if (style == 3) { y = b.process (y); g += b.grDb; }
            y *= makeup;
            x[i] = dry + (y - dry) * mix;
            maxGr = std::max (maxGr, g);
        }
        grDb.store (maxGr);
    }

    std::atomic<float> grDb { 0.0f };

private:
    struct Stage
    {
        enum Model { VCA, Opto, FET };
        Model model = VCA;
        double sr = 44100.0;
        float thr = -16.0f, ratio = 4.0f, knee = 6.0f;
        float attC = 0.0f, relC = 0.0f, relSlowC = 0.0f, relMs = 120.0f;
        float grDb = 0.0f, rms = 0.0f, lastOut = 0.0f, held = 0.0f;
        juce::dsp::IIR::Filter<float> scHp, scShelf;

        void prepare (double s)
        {
            sr = s;
            scHp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 100.0f, 0.707f);
            scShelf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 2000.0f, 0.7f, juce::Decibels::decibelsToGain (4.0f));
            scHp.reset(); scShelf.reset(); grDb = 0; rms = 0; lastOut = 0; held = 0;
        }
        void configure (Model m, float t, float r, float attMs, float releaseMs, float k)
        {
            model = m; thr = t; ratio = juce::jmax (1.0f, r); knee = k; relMs = releaseMs;
            attC = msCoeff (attMs, sr);
            relC = msCoeff (model == Opto ? 60.0f : releaseMs, sr);
            relSlowC = msCoeff (model == Opto ? juce::jmax (500.0f, releaseMs * 4.0f) : releaseMs, sr);
        }
        float curve (float lev, float r) const
        {
            const float over = lev - thr;
            if (2.0f * over < -knee) return 0.0f;
            if (2.0f * std::abs (over) <= knee) { const float t = over + knee * 0.5f; return (1.0f - 1.0f / r) * t * t / (2.0f * knee); }
            return (1.0f - 1.0f / r) * over;
        }
        float process (float x)
        {
            // Senal de deteccion (FET: realimentacion desde la salida)
            float d = scHp.processSample (model == FET ? lastOut : x);
            if (model == Opto) d = scShelf.processSample (d);
            const float d2 = d * d;
            const float rmsC = model == Opto ? 0.9990f : (model == FET ? 0.80f : 0.9950f);
            rms = d2 + (rms - d2) * rmsC;
            const float lev = gainToDb (std::sqrt (rms) * 1.414f + 1e-9f);

            // Opto: el ratio sube cuanto mas comprime
            const float r = model == Opto ? juce::jmin (ratio, 2.0f + grDb * 0.35f) : ratio;
            const float target = curve (lev, r);

            if (target > grDb) grDb = target + (grDb - target) * attC;
            else
            {
                // Opto: liberacion en dos etapas (rapida al principio, lenta si lleva tiempo comprimiendo)
                const float c = model == Opto ? (grDb > held * 0.5f ? relC : relSlowC) : relC;
                grDb = target + (grDb - target) * c;
            }
            held = grDb > held ? grDb : held * 0.99995f;

            float y = x * dbToGain (-grDb);
            if (model == Opto)      y = std::tanh (y * 1.1f) / 1.1f + 0.015f * y * y;             // valvula: armonicos pares
            else if (model == FET)  y = std::tanh (y * 1.35f) / 1.35f + 0.01f * y * y * y;         // FET: impares
            lastOut = y;
            return y;
        }
    };

    double sr = 44100.0;
    Stage a, b;
    int style = 0;
    float makeup = 1.0f, mix = 1.0f;
};

// ---------------------------------------------------------------------------
// Limitador de seguridad estereo (techo -0.3 dBFS)
// ---------------------------------------------------------------------------
class SafetyLimiter
{
public:
    void prepare (double s) { sr = s; g = 1.0f; }
    void process (float* l, float* r, int n)
    {
        const float ceil = dbToGain (-0.3f);
        const float rel = msCoeff (80.0f, sr);
        for (int i = 0; i < n; ++i)
        {
            const float pk = std::max (std::abs (l[i]), r != nullptr ? std::abs (r[i]) : 0.0f);
            const float need = pk > ceil ? ceil / pk : 1.0f;
            g = need < g ? need : need + (g - need) * rel;
            l[i] *= g; if (r != nullptr) r[i] *= g;
            l[i] = juce::jlimit (-ceil, ceil, l[i]);
            if (r != nullptr) r[i] = juce::jlimit (-ceil, ceil, r[i]);
        }
    }
private:
    double sr = 44100.0; float g = 1.0f;
};
} // namespace avdsp
