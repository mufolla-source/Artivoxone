#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <array>

// ============================================================================
//  Vocal Assist: escucha la toma 15 segundos y propone la cadena completa:
//  ganancia de entrada, filtro de graves, puerta, EQ (cuerpo/barro/presencia/
//  brillo), de-esser, compresion, color, espacio y tonalidad de la afinacion.
// ============================================================================
class VocalAssist
{
public:
    static constexpr double listenSeconds = 15.0;

    struct Result
    {
        bool ok = false;
        std::map<juce::String, float> values;   // ID de parametro -> valor real
        int key = -1, scale = -1;               // tonalidad detectada (-1 = no)
        // Metricas medidas (despues de corregir la ganancia de entrada)
        float medianDb = -18, p90Db = -12, noiseDb = -80, lowRel = 0, mudRel = 0, presRel = 0, sibRel = 0, airRel = 0;
        float hpf = 80, gainDelta = 0;
        float absLow = -9, absMud = -8, absPres = -17, absSib = -22, absAir = -31, dynRange = 6;   // valores absolutos medidos
        int genre = -1;
    };

    void prepare (double s)
    {
        sr = s;
        blockLen = (int) (0.05 * sr);
        auto bp = [&] (juce::dsp::IIR::Filter<float>& f, float fc, float q)
        { f.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, juce::jmin (fc, (float) sr * 0.45f), q); f.reset(); };
        bp (fLow, 120.0f, 1.0f); bp (fMud, 320.0f, 1.0f); bp (fPres, 3200.0f, 1.2f); bp (fSib, 7000.0f, 1.4f);
        fAir.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, juce::jmin (11000.0f, (float) sr * 0.45f), 0.707f);
        fAir.reset();
        const size_t maxBlocks = (size_t) (listenSeconds / 0.05) + 8;
        blocks.clear(); blocks.reserve (maxBlocks);
        pitches.clear(); pitches.reserve (8000);
        rec.clear(); rec.reserve ((size_t) (listenSeconds * sr) + 4096);
    }

    void start()
    {
        blocks.clear(); pitches.clear(); rec.clear();
        samples = 0; acc = {}; accCount = 0; lastDetection = -1;
        done.store (false);
        progress.store (0.0f);
        active.store (true);
    }

    void cancel() { active.store (false); }

    // Hilo de audio: senal mono despues de la ganancia de entrada
    template <typename PitchSource>
    void feed (const float* x, int n, const PitchSource& pitch)
    {
        if (! active.load()) return;

        for (int i = 0; i < n; ++i)
        {
            const float v = x[i];
            if (rec.size() < rec.capacity()) rec.push_back (v);
            acc.total += v * v;
            const float a = fLow.processSample (v), b = fMud.processSample (v), c = fPres.processSample (v),
                        d = fSib.processSample (v), e = fAir.processSample (v);
            acc.low += a * a; acc.mud += b * b; acc.pres += c * c; acc.sib += d * d; acc.air += e * e;
            acc.peak = std::max (acc.peak, std::abs (v));

            if (++accCount >= blockLen)
            {
                if (blocks.size() < blocks.capacity()) blocks.push_back (acc);
                acc = {}; accCount = 0;
            }
        }

        if (pitch.isVoiced() && pitch.detectionCounter() != lastDetection && pitches.size() < pitches.capacity())
            pitches.push_back ({ pitch.getLastF0(), pitch.getLastConfidence() });
        lastDetection = pitch.detectionCounter();

        samples += n;
        progress.store ((float) juce::jmin (1.0, samples / (listenSeconds * sr)));
        if (samples >= listenSeconds * sr)
        {
            active.store (false);
            done.store (true);
        }
    }

    std::atomic<bool> active { false }, done { false };
    std::atomic<float> progress { 0.0f };

    // Hilo de mensajes: calcula la propuesta. currentInGainDb = ganancia actual de entrada.
    Result compute (float currentInGainDb) const
    {
        Result r;
        if (blocks.size() < 20) return r;

        auto toDb = [] (double e) { return 10.0 * std::log10 (e + 1e-12); };
        std::vector<double> lev;
        for (auto& b : blocks) lev.push_back (toDb (b.total / (double) blockLen));

        auto sorted = lev; std::sort (sorted.begin(), sorted.end());
        auto pct = [&] (double p) { return sorted[(size_t) juce::jlimit (0, (int) sorted.size() - 1, (int) (p * (sorted.size() - 1)))]; };
        const double noise = pct (0.10);
        const double loud  = pct (0.95);

        // Bloques activos (voz): 12 dB sobre el ruido y por encima de -55 dBFS
        double sum[6] = {}; int active = 0; std::vector<double> actLev;
        for (size_t i = 0; i < blocks.size(); ++i)
        {
            if (lev[i] > pct (0.95) - 15.0 && lev[i] > -60.0)   // robusto tambien con voces densas o muy comprimidas
            {
                auto& b = blocks[i];
                sum[0] += b.total; sum[1] += b.low; sum[2] += b.mud; sum[3] += b.pres; sum[4] += b.sib; sum[5] += b.air;
                ++active; actLev.push_back (lev[i]);
            }
        }
        if (active < 15) return r;   // no hay voz suficiente

        std::sort (actLev.begin(), actLev.end());
        const double median = actLev[actLev.size() / 2];

        auto rel = [&] (int k) { return toDb (sum[k]) - toDb (sum[0]); };
        const double lowRel = rel (1), mudRel = rel (2), presRel = rel (3), sibRel = rel (4), airRel = rel (5);

        // --- Ganancia de entrada: llevar la voz a ~ -18 dBFS RMS
        const double gainDelta = juce::jlimit (-12.0, 12.0, -18.0 - median);
        const double medianAfter = median + gainDelta;
        const double noiseAfter = noise + gainDelta;
        r.values["inGain"] = (float) juce::jlimit (-24.0, 24.0, (double) currentInGainDb + gainDelta);
        r.medianDb = (float) medianAfter; r.noiseDb = (float) noiseAfter; r.gainDelta = (float) gainDelta;
        r.p90Db = (float) (actLev[(size_t) (actLev.size() * 9 / 10)] + gainDelta);
        r.lowRel = (float) (lowRel + 9.0); r.mudRel = (float) (mudRel + 8.0); r.presRel = (float) (presRel + 17.0);
        r.sibRel = (float) (sibRel + 22.0); r.airRel = (float) (airRel + 31.0);   // desviacion respecto a una voz equilibrada
        r.absLow = (float) lowRel; r.absMud = (float) mudRel; r.absPres = (float) presRel; r.absSib = (float) sibRel; r.absAir = (float) airRel;
        r.dynRange = (float) (actLev[(size_t) (actLev.size() * 95 / 100)] - median);
        juce::ignoreUnused (loud);

        // --- Puerta solo si hay ruido de fondo audible y separado de la voz
        if (noiseAfter > -70.0 && medianAfter - noiseAfter > 18.0)
            r.values["gateThresh"] = (float) juce::jlimit (-80.0, -35.0, noiseAfter + 8.0);
        else
            r.values["gateThresh"] = -80.0f;

        // --- Filtro de graves a partir de la nota mas grave cantada
        double hpf = 80.0;
        if (pitches.size() > 30)
        {
            std::vector<float> f; for (auto& p : pitches) f.push_back (p.f0);
            std::sort (f.begin(), f.end());
            hpf = juce::jlimit (60.0, 140.0, 0.6 * f[f.size() / 10]);
        }
        r.values["hpfFreq"] = (float) hpf; r.hpf = (float) hpf;

        // --- EQ (referencias aproximadas de una voz equilibrada)
        r.values["lowGain"]  = (float) juce::jlimit (-4.0, 3.0, -(lowRel - (-9.0)) * 0.5);
        r.values["mudGain"]  = (float) juce::jlimit (-6.0, 1.0, -(mudRel - (-8.0)) * 0.6);
        r.values["presGain"] = (float) juce::jlimit (-3.0, 4.0, -(presRel - (-17.0)) * 0.5);
        r.values["airGain"]  = (float) juce::jlimit (-2.0, 5.0, -(airRel - (-31.0)) * 0.5);

        // --- De-esser segun la energia sibilante
        r.values["deessAmount"] = (float) juce::jlimit (15.0, 75.0, 30.0 + (sibRel - (-22.0)) * 5.0);
        r.values["deessFreq"] = 6500.0f;

        // --- Compresion: umbral ~4 dB bajo el nivel medio, ratio segun el rango dinamico
        const double dyn = actLev.back() - median;
        const double thr = medianAfter - 4.0;
        r.values["compAmount"]  = (float) juce::jlimit (10.0, 90.0, -thr / 0.42);
        r.values["compRatio"]   = (float) juce::jlimit (2.5, 6.0, 2.5 + dyn * 0.15);
        r.values["compAttack"]  = dyn > 14.0 ? 3.0f : 6.0f;
        r.values["compRelease"] = 100.0f;
        r.values["compAuto"]    = 1.0f;
        r.values["compMakeup"]  = 0.0f;

        // --- Color y un toque de espacio
        r.values["satDrive"] = 15.0f;
        r.values["revMix"] = 12.0f;
        r.values["revSize"] = 45.0f;
        r.values["dlyMix"] = 6.0f;

        // --- Tonalidad (perfiles de Krumhansl-Kessler)
        if (pitches.size() > 60)
        {
            double hist[12] = {};
            for (auto& p : pitches)
            {
                const double midi = 69.0 + 12.0 * std::log2 (p.f0 / 440.0);
                hist[((int) std::round (midi) % 12 + 12) % 12] += p.conf;
            }
            static const double maj[12] = { 6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88 };
            static const double mnr[12] = { 6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17 };
            double best = -2.0;
            for (int mode = 0; mode < 2; ++mode)
                for (int k = 0; k < 12; ++k)
                {
                    const double* prof = mode == 0 ? maj : mnr;
                    double mx = 0, my = 0;
                    for (int i = 0; i < 12; ++i) { mx += hist[i]; my += prof[i]; }
                    mx /= 12; my /= 12;
                    double sxy = 0, sxx = 0, syy = 0;
                    for (int i = 0; i < 12; ++i)
                    {
                        const double a = hist[(i + k) % 12] - mx, b = prof[i] - my;
                        sxy += a * b; sxx += a * a; syy += b * b;
                    }
                    const double c = sxy / std::sqrt (sxx * syy + 1e-12);
                    if (c > best) { best = c; r.key = k; r.scale = mode; }
                }
        }

        // Medidas absolutas con el mismo criterio que los perfiles de referencia
        if (rec.size() > (size_t) (sr * 3))
        {
            const auto m = measure (rec.data(), (int) rec.size(), sr);
            if (m.ok) { r.absLow = m.low; r.absMud = m.mud; r.absPres = m.pres; r.absSib = m.sib; r.absAir = m.air; r.dynRange = m.dyn; }
        }

        r.ok = true;
        return r;
    }

    // Grabacion de la toma analizada (para el ajuste fino en segundo plano)
    const std::vector<float>& getRecording() const { return rec; }

    // Medida de una senal con los mismos filtros (para comparar con los perfiles)
    struct Measure { bool ok = false; float low = 0, mud = 0, pres = 0, sib = 0, air = 0, dyn = 0, median = -100; };
    static Measure measure (const float* x, int n, double sr)
    {
        Measure m;
        using C = juce::dsp::IIR::Coefficients<float>;
        juce::dsp::IIR::Filter<float> f[5], hp80;
        hp80.coefficients = C::makeHighPass (sr, 80.0f, 0.707f);
        f[0].coefficients = C::makeBandPass (sr, 120.0f, 1.0f);
        f[1].coefficients = C::makeBandPass (sr, 320.0f, 1.0f);
        f[2].coefficients = C::makeBandPass (sr, 3200.0f, 1.2f);
        f[3].coefficients = C::makeBandPass (sr, juce::jmin (7000.0f, (float) sr * 0.45f), 1.4f);
        f[4].coefficients = C::makeHighPass (sr, juce::jmin (11000.0f, (float) sr * 0.45f), 0.707f);
        const int B = (int) (0.05 * sr);
        const int nb = n / B;
        if (nb < 20) return m;
        std::vector<std::array<double, 6>> e ((size_t) nb);
        for (int b = 0; b < nb; ++b)
        {
            std::array<double, 6> a {};
            for (int i = 0; i < B; ++i)
            {
                const float v = x[b * B + i];
                const float vh = hp80.processSample (v);   // total sin sub-graves (restos de 808)
                a[0] += vh * vh;
                for (int k = 0; k < 5; ++k) { const float y = f[k].processSample (v); a[(size_t) k + 1] += y * y; }
            }
            e[(size_t) b] = a;
        }
        auto toDb = [] (double v) { return 10.0 * std::log10 (v + 1e-20); };
        std::vector<double> lev; for (auto& a : e) lev.push_back (toDb (a[0] / B));
        auto srt = lev; std::sort (srt.begin(), srt.end());
        const double p95 = srt[srt.size() * 95 / 100];
        double sum[6] = {}; std::vector<double> act;
        for (int b = 0; b < nb; ++b)
            if (lev[(size_t) b] > p95 - 15.0 && lev[(size_t) b] > -60.0)
            { for (int k = 0; k < 6; ++k) sum[k] += e[(size_t) b][(size_t) k]; act.push_back (lev[(size_t) b]); }
        if (act.size() < 15) return m;
        std::sort (act.begin(), act.end());
        m.median = (float) act[act.size() / 2];
        m.dyn = (float) (act[act.size() * 95 / 100] - act[act.size() / 2]);
        m.low = (float) (toDb (sum[1]) - toDb (sum[0])); m.mud = (float) (toDb (sum[2]) - toDb (sum[0]));
        m.pres = (float) (toDb (sum[3]) - toDb (sum[0])); m.sib = (float) (toDb (sum[4]) - toDb (sum[0]));
        m.air = (float) (toDb (sum[5]) - toDb (sum[0]));
        m.ok = true;
        return m;
    }

private:
    std::vector<float> rec;
    struct Acc { float total = 0, low = 0, mud = 0, pres = 0, sib = 0, air = 0, peak = 0; };
    struct P { float f0, conf; };

    double sr = 44100.0;
    int blockLen = 2205, accCount = 0;
    double samples = 0;
    int lastDetection = -1;
    Acc acc;
    std::vector<Acc> blocks;
    std::vector<P> pitches;
    juce::dsp::IIR::Filter<float> fLow, fMud, fPres, fSib, fAir;
};
